// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFeedbackComponent.h"
#include "Feedback/TCFeedbackOverride.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "Core/TeamCarryGameState.h"
#include "Core/TeamCarryGameMode.h"
#include "Player/Component/GrabComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

namespace
{
	// 플레이스홀더 기본 에셋 (정식 에셋 도입 시 여기만 교체하거나 인스턴스에서 오버라이드)
	const TCHAR* DefaultPickupSound = TEXT("/Game/Developers/goldb/Audio/SW_Pickup.SW_Pickup");
	const TCHAR* DefaultDropSound = TEXT("/Game/Developers/goldb/Audio/SW_Drop.SW_Drop");
	const TCHAR* DefaultThrowSound = TEXT("/Game/Developers/goldb/Audio/SW_Throw.SW_Throw");

	// 놓는 순간 이 속도(cm/s) 이상이면 '던지기'로 판정 (던지기 임펄스=1000, 운반 속도≈300)
	constexpr float ThrowSpeedThreshold = 600.f;

	// 핫타임 시 남은 가구 빨간 아웃라인(스텐실 4) — GS->bIsHotTime 으로 판정.
	// TCFeedbackSubsystem 이 트럭 비율 기준으로 설정하고 GameState 복제로 전파한다.

	// 내구도 감소(타격) 시 재생 — 2종 교대 (헤더 무수정을 위해 cpp 로컬 상수)
	const TCHAR* DefaultHitSounds[] = {
		TEXT("/Game/Developers/goldb/Audio/SW_Hit01.SW_Hit01"),
		TEXT("/Game/Developers/goldb/Audio/SW_Hit02.SW_Hit02"),
	};
	const TCHAR* DefaultPickupFX = TEXT("/Game/Developers/goldb/VFX/NS_GrabPuff.NS_GrabPuff");
	const TCHAR* DefaultBreakSound = TEXT("/Game/Developers/goldb/Audio/SW_Impact.SW_Impact");
	const TCHAR* DefaultBreakFX = TEXT("/Game/Developers/goldb/VFX/NS_ImpactPuff.NS_ImpactPuff");
	// 강한 충돌 체감 — 묵직한 '쿵' 레이어 + 만화식 별 팝
	const TCHAR* DefaultThudSound = TEXT("/Game/Developers/goldb/Audio/SW_Thud.SW_Thud");
	const TCHAR* DefaultHitStarsFX = TEXT("/Game/Developers/goldb/VFX/NS_HitStars.NS_HitStars");
	// 파괴 디메리트 체감 — 감점 팝업 + 하강 실패 스팅
	const TCHAR* BreakPenaltyWidgetPath = TEXT("/Game/Developers/goldb/UI/WBP_BreakPenalty.WBP_BreakPenalty_C");
	const TCHAR* DefaultBreakPenaltySound = TEXT("/Game/Developers/goldb/Audio/SW_BreakPenalty.SW_BreakPenalty");
	constexpr float BreakPenaltyPopupSeconds = 1.8f;
}

UTCFeedbackComponent::UTCFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f; // 상태 관찰용이라 20Hz면 충분
}

void UTCFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	// 데디서버는 코스메틱 없음
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// 잡힘 상태 소스 결정: ①GrabSystem 컴포넌트(TCFurnitureActor 계열) ②bIsGrabbed 리플렉션
	GrabSystem = Owner->FindComponentByClass<UFurnitureGrabSystem>();
	if (!GrabSystem)
	{
		GrabbedProp = CastField<FBoolProperty>(Owner->GetClass()->FindPropertyByName(TEXT("bIsGrabbed")));
	}
	if (!GrabSystem && !GrabbedProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Feedback] %s: 잡힘 상태 소스 없음 — 관찰 비활성"), *GetNameSafe(Owner));
		SetComponentTickEnabled(false);
		return;
	}
	bLastGrabbed = ReadGrabbed();

	// 내구도 관찰 (있는 가구만) — 파괴 순간 감지용
	Stat = Owner->FindComponentByClass<UFurnitureStat>();
	LastHealth = Stat ? Stat->GetCurrentHealth() : -1.f;

	// 기본 에셋 로드 (인스턴스에서 지정했으면 유지)
	if (!PickupSound) { PickupSound = LoadObject<USoundBase>(nullptr, DefaultPickupSound); }
	if (!DropSound) { DropSound = LoadObject<USoundBase>(nullptr, DefaultDropSound); }
	if (!PickupFX) { PickupFX = LoadObject<UNiagaraSystem>(nullptr, DefaultPickupFX); }
	if (!BreakSound) { BreakSound = LoadObject<USoundBase>(nullptr, DefaultBreakSound); }
	if (!BreakFX) { BreakFX = LoadObject<UNiagaraSystem>(nullptr, DefaultBreakFX); }

	UE_LOG(LogTemp, Log, TEXT("[Feedback] %s 부착 완료 (sound: %s/%s, fx: %s)"), *GetNameSafe(Owner),
		PickupSound ? TEXT("O") : TEXT("X"), DropSound ? TEXT("O") : TEXT("X"), PickupFX ? TEXT("O") : TEXT("X"));
}

void UTCFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || (!GrabSystem && !GrabbedProp))
	{
		return;
	}

	// ── 내구도 관찰: 0 도달 = 파괴(퍼프+파열음), 감소 = 타격음 ──
	if (Stat)
	{
		const float Health = Stat->GetCurrentHealth();
		if (LastHealth > 0.f && Health <= 0.f)
		{
			const FVector Loc = Owner->GetActorLocation();
			if (BreakSound) { UGameplayStatics::PlaySoundAtLocation(this, BreakSound, Loc); }
			if (BreakFX) { UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakFX, Loc); }

			// 파괴 디메리트 체감 — 하강 실패 스팅(2D) + 감점 팝업(1.8초 후 자동 제거).
			// 파괴는 배송 점수를 통째로 잃는 사건인데 연출이 약해 손해가 체감되지 않던 문제.
			if (USoundBase* PenaltyS = LoadObject<USoundBase>(nullptr, DefaultBreakPenaltySound))
			{
				UGameplayStatics::PlaySound2D(this, PenaltyS);
			}
			if (UWorld* World = GetWorld())
			{
				if (UClass* PenaltyCls = LoadClass<UUserWidget>(nullptr, BreakPenaltyWidgetPath))
				{
					if (APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr)
					{
						if (UUserWidget* Popup = CreateWidget<UUserWidget>(PC, PenaltyCls))
						{
							Popup->AddToViewport(45); // HUD 위, 시간 경고(50)보다는 아래
							TWeakObjectPtr<UUserWidget> WeakPopup = Popup;
							FTimerHandle PopupTimer;
							World->GetTimerManager().SetTimer(PopupTimer, [WeakPopup]()
							{
								if (WeakPopup.IsValid())
								{
									WeakPopup->RemoveFromParent();
								}
							}, BreakPenaltyPopupSeconds, false);
						}
					}
				}
			}
		}
		else if (LastHealth > 0.f && Health < LastHealth - KINDA_SMALL_NUMBER)
		{
			// 인원 미달 운반의 내구도 드레인(잡힌 상태의 지속 소모)은 충돌 히트 피드백 대상이 아님
			const bool bUnderMannedDrain = ReadGrabbed() && Stat
				&& Stat->GetGrabbedPlayerNum() < Stat->GetRequiredPlayer();
			if (!bUnderMannedDrain)
			{
				// 내구도 깎임 — 소프트 우드 히트 (2종 랜덤 + 피치 흔들림)
				const int32 HitIdx = FMath::RandRange(0, 1);
				if (USoundBase* HitS = LoadObject<USoundBase>(nullptr, DefaultHitSounds[HitIdx]))
				{
					UGameplayStatics::PlaySoundAtLocation(this, HitS, Owner->GetActorLocation(),
						1.f, FMath::RandRange(0.9f, 1.1f));
				}

				// 우드 히트 아래에 저역 '쿵'을 겹쳐 무게감을 만든다 (피치 랜덤으로 반복감 완화)
				if (USoundBase* Thud = LoadObject<USoundBase>(nullptr, DefaultThudSound))
				{
					UGameplayStatics::PlaySoundAtLocation(this, Thud, Owner->GetActorLocation(),
						1.f, FMath::RandRange(0.92f, 1.06f));
				}

				// 만화식 별 팝 — 가구 상단에서 터져 '띵' 하고 부딪힌 게 한눈에 보이게
				if (UNiagaraSystem* Stars = LoadObject<UNiagaraSystem>(nullptr, DefaultHitStarsFX))
				{
					FVector Origin, Extent;
					Owner->GetActorBounds(false, Origin, Extent);
					const FVector Top(Origin.X, Origin.Y, Origin.Z + Extent.Z * 0.6f);
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Stars, Top);
				}
			}
		}
		LastHealth = Health;
	}

	const bool bGrabbed = ReadGrabbed();

	// ── 아웃라인 우선순위 (머신별 로컬) ──
	// 스텐실은 메시당 하나뿐이라 포커스(1)·운반(3)·임박(4)이 서로 덮어씀. 우선순위:
	//   ① 로컬 플레이어가 지금 조준 중(잡을 수 있음) = 노랑 — '개인별' 표시라 내 화면에서만
	//   ② 잡혀 있음 = 초록   ③ 시간 임박 + 안 잡힘 = 빨강
	// 커스텀뎁스 스텐실은 복제되지 않는 렌더 상태이므로 각자 자기 화면 기준으로 갈린다.
	bool bLocallyFocused = false;
	if (UWorld* FW = GetWorld())
	{
		if (APlayerController* LPC = GEngine ? GEngine->GetFirstLocalPlayerController(FW) : nullptr)
		{
			if (APawn* LocalPawn = LPC->GetPawn())
			{
				if (UGrabComponent* LocalGrab = LocalPawn->FindComponentByClass<UGrabComponent>())
				{
					// 자기가 이미 들고 있는 가구는 노랑보다 운반 초록이 맞음 → 제외
					bLocallyFocused = (LocalGrab->CurrentBestTarget == Owner)
					               && (LocalGrab->GetGrabbedActor() != Owner);
				}
			}
		}
	}

	if (bLocallyFocused)
	{
		if (UStaticMeshComponent* MeshC = Owner->FindComponentByClass<UStaticMeshComponent>())
		{
			if (MeshC->CustomDepthStencilValue != 1) { MeshC->SetCustomDepthStencilValue(1); }
			if (!MeshC->bRenderCustomDepth) { MeshC->SetRenderCustomDepth(true); }
		}
	}
	else if (bGrabbed)
	{
		// ── 운반 하이라이트: 잡혀 있는 동안 스텐실 3(초록 링)을 매 틱 재주장 ──
		// 포커스 시스템(OnUnfocus)이 커스텀뎁스를 꺼도 다음 틱에 즉시 복구된다.
		if (UStaticMeshComponent* MeshC = Owner->FindComponentByClass<UStaticMeshComponent>())
		{
			if (MeshC->CustomDepthStencilValue != 3) { MeshC->SetCustomDepthStencilValue(3); }
			if (!MeshC->bRenderCustomDepth) { MeshC->SetRenderCustomDepth(true); }
		}
	}
	else
	{
		// ── 핫타임: 남아 있는(안 잡힌) 가구에 빨간 링(스텐실 4) 재주장 ──
		// TCFeedbackSubsystem 이 트럭 비율 기준으로 GS->bIsHotTime 을 설정하면
		// 모든 클라이언트에 복제되어 이곳에서 빨간 링을 표시한다.
		UWorld* W = GetWorld();
		const ATeamCarryGameState* GS = W ? W->GetGameState<ATeamCarryGameState>() : nullptr;
		const bool bUrgent = GS && !GS->bIsGameFinished
			&& GS->CurrentPhase == EGamePhase::Playing
			&& GS->bIsHotTime;
		if (bUrgent)
		{
			if (UStaticMeshComponent* MeshC = Owner->FindComponentByClass<UStaticMeshComponent>())
			{
				if (MeshC->CustomDepthStencilValue != 4) { MeshC->SetCustomDepthStencilValue(4); }
				if (!MeshC->bRenderCustomDepth) { MeshC->SetRenderCustomDepth(true); }
			}
		}
	}

	if (bGrabbed == bLastGrabbed)
	{
		return;
	}
	bLastGrabbed = bGrabbed;
	UE_LOG(LogTemp, Log, TEXT("[Feedback] %s 잡힘 전이: %s"), *GetNameSafe(Owner),
		bGrabbed ? TEXT("잡기") : TEXT("놓기"));

	// 놓는 순간: 핫타임이면 빨간 링(4)을 즉시 복원, 아니면 포커스 규칙(1)으로 끈다
	if (!bGrabbed)
	{
		if (UStaticMeshComponent* MeshC = Owner->FindComponentByClass<UStaticMeshComponent>())
		{
			UWorld* RW = GetWorld();
			const ATeamCarryGameState* RGS = RW ? RW->GetGameState<ATeamCarryGameState>() : nullptr;
			const bool bStillUrgent = RGS && !RGS->bIsGameFinished
				&& RGS->CurrentPhase == EGamePhase::Playing
				&& RGS->bIsHotTime;
			if (bStillUrgent)
			{
				MeshC->SetCustomDepthStencilValue(4);
				MeshC->SetRenderCustomDepth(true);
			}
			else
			{
				MeshC->SetCustomDepthStencilValue(1);
				MeshC->SetRenderCustomDepth(false);
			}
		}
	}

	// 소유자가 인터페이스로 거부하면 재생하지 않음
	if (Owner->Implements<UTCFeedbackOverride>() &&
		!ITCFeedbackOverride::Execute_ShouldAutoFeedback(Owner))
	{
		return;
	}

	const FVector Loc = GetFXLocation();
	if (bGrabbed)
	{
		if (PickupSound) { UGameplayStatics::PlaySoundAtLocation(this, PickupSound, Loc); }
		if (PickupFX)
		{
			// 들어올릴 때 바닥 먼지 — 액터 바운즈 밑면에서 스폰
			FVector Origin, Extent;
			Owner->GetActorBounds(false, Origin, Extent);
			const FVector Base(Origin.X, Origin.Y, Origin.Z - Extent.Z + 8.f);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupFX, Base);
		}
	}
	else
	{
		// 놓기 vs 던지기 — 던지기는 놓는 즉시 임펄스가 실려 속도로 구분된다
		if (Owner->GetVelocity().Size() > ThrowSpeedThreshold)
		{
			if (USoundBase* ThrowS = LoadObject<USoundBase>(nullptr, DefaultThrowSound))
			{
				UGameplayStatics::PlaySoundAtLocation(this, ThrowS, Loc);
			}
		}
		else if (DropSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DropSound, Loc);
		}
	}
}


bool UTCFeedbackComponent::ReadGrabbed() const
{
	if (GrabSystem)
	{
		return GrabSystem->GetGrabbedPlayers().Num() > 0;
	}
	AActor* Owner = GetOwner();
	return (Owner && GrabbedProp) ? GrabbedProp->GetPropertyValue_InContainer(Owner) : false;
}

FVector UTCFeedbackComponent::GetFXLocation() const
{
	AActor* Owner = GetOwner();
	if (Owner && Owner->Implements<UTCFeedbackOverride>())
	{
		return ITCFeedbackOverride::Execute_GetFeedbackLocation(Owner);
	}
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}
