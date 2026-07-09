// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFeedbackSubsystem.h"
#include "Feedback/TCFeedbackComponent.h"
#include "Feedback/TCFeedbackOverride.h"
#include "Core/TeamCarryGameState.h"
#include "Network/Carry/TCCarriableFurniture.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "Level/Vehicle/TCMovingTruck.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"

namespace
{
	const TCHAR* DefaultTruckInSound = TEXT("/Game/Developers/goldb/Audio/SW_TruckIn.SW_TruckIn");
	const TCHAR* DefaultTruckInFX = TEXT("/Game/Developers/goldb/VFX/NS_TruckInPop.NS_TruckInPop");
	const TCHAR* DefaultCountdownSound = TEXT("/Game/Developers/goldb/Audio/SW_CountTick.SW_CountTick");
	const TCHAR* DefaultGoSound = TEXT("/Game/Developers/goldb/Audio/SW_CountGo.SW_CountGo");
}

bool UTCFeedbackSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// 게임/PIE 월드에서만 (에디터 프리뷰·인액티브 월드 제외)
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UTCFeedbackSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return; // 데디서버는 코스메틱 없음 (틱은 돌지만 아래 가드로 무시)
	}

	// 피드백 에셋 로드
	TruckInSound = LoadObject<USoundBase>(nullptr, DefaultTruckInSound);
	TruckInFX = LoadObject<UNiagaraSystem>(nullptr, DefaultTruckInFX);
	CountdownSound = LoadObject<USoundBase>(nullptr, DefaultCountdownSound);
	GoSound = LoadObject<USoundBase>(nullptr, DefaultGoSound);

	// 레벨에 이미 있는 운반 가구에 피드백 컴포넌트 부착
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AttachFeedbackIfFurniture(*It);
	}
	// 이후 스폰되는 가구에도 자동 부착
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UTCFeedbackSubsystem::AttachFeedbackIfFurniture));
}

void UTCFeedbackSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

void UTCFeedbackSubsystem::AttachFeedbackIfFurniture(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	// 부착 대상: TCCarriableFurniture 계열 또는 GrabSystem을 가진 가구(TCFurnitureActor 계열)
	const bool bIsFurniture = Actor->IsA<ATCCarriableFurniture>()
		|| Actor->FindComponentByClass<UFurnitureGrabSystem>() != nullptr;
	if (!bIsFurniture || Actor->FindComponentByClass<UTCFeedbackComponent>())
	{
		return;
	}
	// 액터가 인터페이스로 자동 피드백을 거부하면 부착하지 않음
	if (Actor->Implements<UTCFeedbackOverride>() &&
		!ITCFeedbackOverride::Execute_ShouldAutoFeedback(Actor))
	{
		return;
	}
	UTCFeedbackComponent* Comp = NewObject<UTCFeedbackComponent>(Actor, TEXT("TCFeedback"));
	Comp->RegisterComponent();
}

void UTCFeedbackSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>();
	if (!GS)
	{
		return;
	}

	// ── 가구 감소 감지 → 적재 판별 ──
	// 적재(OnFurnitureEnterTruck)만 점수도 함께 갱신된다. 파괴 연출은
	// TCFeedbackComponent::EndPlay가 가구 위치에서 직접 담당하므로 여기선 적재만 본다.
	const int32 Remaining = GS->RemainingFurniture;
	const int32 Score = GS->TotalScore;
	if (LastRemaining >= 0 && Remaining < LastRemaining)
	{
		bPendingClassify = true;
		PendingTimeLeft = 0.3f; // 점수 복제가 늦게 오는 경우 대비한 판별 창
		PendingBaseScore = LastScore;
	}
	LastRemaining = Remaining;
	if (bPendingClassify)
	{
		if (Score != PendingBaseScore)
		{
			bPendingClassify = false;
			PlayDeposit();  // 점수 변화 동반 = 트럭 적재
		}
		else
		{
			PendingTimeLeft -= DeltaTime;
			if (PendingTimeLeft <= 0.f)
			{
				bPendingClassify = false;  // 점수 무변화 = 파괴 — 컴포넌트가 처리
			}
		}
	}
	LastScore = Score;

	// ── 페이즈 전환음 ──
	const EGamePhase Phase = GS->CurrentPhase;
	if (!bPhaseInitialized)
	{
		bPhaseInitialized = true;
		LastPhase = Phase;
		return;
	}
	if (Phase != LastPhase)
	{
		LastPhase = Phase;
		if (Phase == EGamePhase::Countdown && CountdownSound)
		{
			UGameplayStatics::PlaySound2D(this, CountdownSound);
		}
		else if (Phase == EGamePhase::Playing && GoSound)
		{
			UGameplayStatics::PlaySound2D(this, GoSound);
		}
	}
}

void UTCFeedbackSubsystem::PlayDeposit()
{
	UWorld* World = GetWorld();
	if (TruckInSound) { UGameplayStatics::PlaySound2D(this, TruckInSound); }
	if (TruckInFX && World)
	{
		// 트럭 위에서 팝 — 트럭이 없는 맵이면 로컬 플레이어 앞에 폴백
		FVector Loc;
		if (AActor* Truck = UGameplayStatics::GetActorOfClass(World, ATCMovingTruck::StaticClass()))
		{
			Loc = Truck->GetActorLocation() + FVector(0, 0, 250);
		}
		else if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Loc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 200 + FVector(0, 0, 100);
		}
		else
		{
			return;
		}
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, TruckInFX, Loc,
			FRotator::ZeroRotator, FVector(1.5f));
		UE_LOG(LogTemp, Log, TEXT("[Feedback] 적재 팝 스폰: %s"), *Loc.ToCompactString());
	}
}
