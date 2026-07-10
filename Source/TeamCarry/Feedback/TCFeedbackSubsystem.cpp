// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFeedbackSubsystem.h"
#include "Feedback/TCFeedbackComponent.h"
#include "Feedback/TCFeedbackOverride.h"
#include "Feedback/TCFootstepComponent.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Core/TeamCarryGameState.h"
#include "Core/TeamCarryGameMode.h"
#include "Network/Carry/TCCarriableFurniture.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "Level/Vehicle/TCMovingTruck.h"
#include "Network/Session/TCSessionFlow.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

namespace
{
	const TCHAR* DefaultTruckInSound = TEXT("/Game/Developers/goldb/Audio/SW_TruckIn.SW_TruckIn");
	const TCHAR* DefaultTruckInFX = TEXT("/Game/Developers/goldb/VFX/NS_TruckInPop.NS_TruckInPop");
	const TCHAR* DefaultCountdownSound = TEXT("/Game/Developers/goldb/Audio/SW_CountTick.SW_CountTick");
	const TCHAR* DefaultGoSound = TEXT("/Game/Developers/goldb/Audio/SW_CountGo.SW_CountGo");
	const TCHAR* DefaultBGMTracks[] = {
		TEXT("/Game/Developers/goldb/Audio/SW_BGM_01.SW_BGM_01"),
		TEXT("/Game/Developers/goldb/Audio/SW_BGM_02.SW_BGM_02"),
	};
	// 타이틀/로비 BGM — 인게임 BGM(페이즈 전환 트리거)과 달리 맵 진입 즉시 재생
	const TCHAR* DefaultTitleBGM = TEXT("/Game/Developers/goldb/Audio/SW_BGM_Title.SW_BGM_Title");
	const TCHAR* DefaultLobbyBGM = TEXT("/Game/Developers/goldb/Audio/SW_BGM_Lobby.SW_BGM_Lobby");
	const TCHAR* DefaultGameClear = TEXT("/Game/Developers/goldb/Audio/SW_GameClear.SW_GameClear");
	const TCHAR* TimeWarningWidgetPath = TEXT("/Game/Developers/goldb/UI/WBP_TimeWarning.WBP_TimeWarning_C");
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
	for (const TCHAR* Path : DefaultBGMTracks)
	{
		if (USoundBase* Track = LoadObject<USoundBase>(nullptr, Path))
		{
			BGMTracks.Add(Track);
		}
	}

	// ── 타이틀/로비 BGM: 맵 진입 즉시 재생 ──
	// 맵 판별은 ATCPlayerController::BeginPlay 와 동일하게 SessionFlow 설정 경로와 비교한다
	// (맵 이름 하드코딩 금지 — ini 로 경로를 바꿔도 계속 맞아떨어지도록).
	const FString MapPath = UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName());
	const UTCSessionFlow* Flow = InWorld.GetGameInstance()
		? InWorld.GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	const TCHAR* MenuBGMPath = nullptr;
	if (Flow && MapPath.Equals(Flow->GetTitleMapPath(), ESearchCase::IgnoreCase))
	{
		MenuBGMPath = DefaultTitleBGM;
	}
	else if (Flow && MapPath.Equals(Flow->GetLobbyMapPath(), ESearchCase::IgnoreCase))
	{
		MenuBGMPath = DefaultLobbyBGM;
	}
	if (MenuBGMPath)
	{
		if (USoundBase* MenuTrack = LoadObject<USoundBase>(nullptr, MenuBGMPath))
		{
			BGMComp = UGameplayStatics::SpawnSound2D(&InWorld, MenuTrack, 1.f, 1.f, 0.f, nullptr, false, false);
			if (BGMComp)
			{
				// 인게임 BGM(0.22)보다 높게 잡는다 — 메뉴 트랙은 편곡이 성겨서 같은 레벨이면
				// 훨씬 작게 들리고(체감 음량), 메뉴에는 경쟁하는 조작음도 없다.
				BGMComp->FadeIn(1.5f, 0.5f);
				UE_LOG(LogTemp, Log, TEXT("[Feedback] 메뉴 BGM 시작: %s"), *MenuTrack->GetName());
			}
		}
	}

	// 레벨에 이미 있는 운반 가구·플레이어에 피드백 컴포넌트 부착
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
	// 플레이어 캐릭터에는 발소리 컴포넌트 부착
	if (Actor->IsA<ATCPlayerCharacter>())
	{
		if (!Actor->FindComponentByClass<UTCFootstepComponent>())
		{
			UTCFootstepComponent* Foot = NewObject<UTCFootstepComponent>(Actor, TEXT("TCFootstep"));
			Foot->RegisterComponent();
		}
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
		else if (Phase == EGamePhase::Playing)
		{
			if (GoSound) { UGameplayStatics::PlaySound2D(this, GoSound); }
			// 시작음이 끝난 뒤 BGM 페이드인
			bBGMBoosted = false;
			World->GetTimerManager().SetTimer(BGMStartTimer, this,
				&UTCFeedbackSubsystem::StartBGM, 1.2f, false);
		}
	}

	// ── 시간 압박 연출: 잔여시간이 임계 이하로 떨어지면 경고 UI + BGM 배속 ──
	// (제한시간 종료 자체는 ATeamCarryGameMode::Tick 이 처리 — 게임 룰은 게임모드 소관)
	// 경고 UI는 BGM 재생 여부와 무관하게 떠야 하므로 BGMComp 조건을 게이트에 두지 않는다
	// (BGM 미로드/정지 시 경고까지 통째로 사라지던 결합 제거).
	if (Phase == EGamePhase::Playing && !bBGMBoosted)
	{
		// 제한시간의 정본은 게임모드(BP에서 스테이지별 오버라이드) — 서버/호스트에서 읽고,
		// 순수 클라이언트는 서브시스템 기본값 폴백 (기본값을 게임모드와 일치시킬 것)
		float EffectiveLimit = TimeLimitSeconds;
		if (const ATeamCarryGameMode* GM = World->GetAuthGameMode<ATeamCarryGameMode>())
		{
			EffectiveLimit = GM->TimeLimitSeconds;
		}
		const float RemainingTime = EffectiveLimit - GS->ElapsedTime;
		if (RemainingTime <= BGMSpeedupRemaining)
		{
			bBGMBoosted = true;
			if (BGMComp && BGMComp->IsPlaying())
			{
				BGMComp->SetPitchMultiplier(BGMSpeedupPitch);
				UE_LOG(LogTemp, Log, TEXT("[Feedback] BGM 배속 x%.2f (잔여 %.0f초)"), BGMSpeedupPitch, RemainingTime);
			}

			// 시간 임박 경고 UI — 붉은 비네트 펄스 + 경고 문구 (뷰포트가 수명 관리)
			// 리슨 서버 월드의 PC 목록은 심리스 트래블 뒤 원격 클라이언트가 0번에 올 수 있어
			// GetPlayerController(0) 대신 '로컬' 컨트롤러를 명시적으로 찾는다 (호스트에서 경고 미표시 원인).
			if (UClass* WarnCls = LoadClass<UUserWidget>(nullptr, TimeWarningWidgetPath))
			{
				if (APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr)
				{
					if (UUserWidget* Warn = CreateWidget<UUserWidget>(PC, WarnCls))
					{
						Warn->AddToViewport(50); // HUD 위, 결과창 아래쯤
					}
				}
			}
		}
	}

	// 게임 종료: BGM 페이드아웃 + 클리어 팡파르 + 시간 경고 UI 제거
	if (GS->bIsGameFinished && !bBGMFadedOut)
	{
		bBGMFadedOut = true;
		StopBGM();
		if (USoundBase* Clear = LoadObject<USoundBase>(nullptr, DefaultGameClear))
		{
			UGameplayStatics::PlaySound2D(this, Clear);
		}
		if (UClass* WarnCls = LoadClass<UUserWidget>(nullptr, TimeWarningWidgetPath))
		{
			TArray<UUserWidget*> Warns;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Warns, WarnCls, false);
			for (UUserWidget* Wg : Warns)
			{
				Wg->RemoveFromParent();
			}
		}
	}
}

void UTCFeedbackSubsystem::StartBGM()
{
	UWorld* World = GetWorld();
	if (!World || BGMTracks.Num() == 0 || (BGMComp && BGMComp->IsPlaying()))
	{
		return;
	}
	// 매 판 랜덤 트랙
	USoundBase* Track = BGMTracks[FMath::RandRange(0, BGMTracks.Num() - 1)];
	BGMComp = UGameplayStatics::SpawnSound2D(World, Track, 1.f, 1.f, 0.f, nullptr, false, false);
	if (BGMComp)
	{
		BGMComp->SetPitchMultiplier(1.f);
		BGMComp->FadeIn(2.0f, 0.22f); // 2초에 걸쳐 볼륨 0.22까지 — 조작음(픽업/드롭)이 묻히지 않게
		UE_LOG(LogTemp, Log, TEXT("[Feedback] BGM 시작: %s"), *Track->GetName());
	}
}

void UTCFeedbackSubsystem::StopBGM()
{
	if (BGMComp && BGMComp->IsPlaying())
	{
		BGMComp->FadeOut(1.5f, 0.f);
	}
}

void UTCFeedbackSubsystem::PlayDeposit()
{
	UWorld* World = GetWorld();
	if (TruckInSound) { UGameplayStatics::PlaySound2D(this, TruckInSound); }
	if (TruckInFX && World)
	{
		// 적재존(트리거) 또는 트럭 위에서 팝. 레벨 트럭은 BP(BP_ToyTruck/BP_TruckTrigger)라
		// C++ 타입 매칭이 안 됨 — 클래스 이름으로 탐색한다. 못 찾으면 스폰하지 않는다
		// (플레이어 앞 폴백은 오히려 오답이라 제거).
		AActor* Anchor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString ClsName = It->GetClass()->GetName();
			if (ClsName.Contains(TEXT("TruckTrigger")))
			{
				Anchor = *It;
				break; // 적재존이 최우선
			}
			if (!Anchor && (ClsName.Contains(TEXT("ToyTruck")) || It->IsA<ATCMovingTruck>()))
			{
				Anchor = *It;
			}
		}
		if (!Anchor)
		{
			return;
		}
		// 트리거 액터 위치는 볼륨 중심(공중) — 이펙트가 바닥에서 재생되도록 지면으로 내린다.
		// 존 안의 가구/트럭(무버블)에 걸리지 않게 WorldStatic 오브젝트 타입만 트레이스.
		FVector Loc = Anchor->GetActorLocation();
		FHitResult Hit;
		FCollisionObjectQueryParams ObjParams(ECC_WorldStatic);
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = true;
		QueryParams.AddIgnoredActor(Anchor); // 트리거 박스 자신에 걸리면 시작점이 그대로 반환된다
		if (World->LineTraceSingleByObjectType(Hit, Loc + FVector(0, 0, 100), Loc - FVector(0, 0, 1000), ObjParams, QueryParams))
		{
			Loc.Z = Hit.Location.Z + 8.f;
		}
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, TruckInFX, Loc,
			FRotator::ZeroRotator, FVector(1.5f));
		UE_LOG(LogTemp, Log, TEXT("[Feedback] 적재 팝 스폰: %s (%s)"), *Loc.ToCompactString(), *Anchor->GetName());
	}
}
