// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFeedbackComponent.h"
#include "Feedback/TCFeedbackOverride.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "Core/TeamCarryGameState.h"
#include "Core/TeamCarryGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace
{
	// 플레이스홀더 기본 에셋 (정식 에셋 도입 시 여기만 교체하거나 인스턴스에서 오버라이드)
	const TCHAR* DefaultPickupSound = TEXT("/Game/Developers/goldb/Audio/SW_Pickup.SW_Pickup");
	const TCHAR* DefaultDropSound = TEXT("/Game/Developers/goldb/Audio/SW_Drop.SW_Drop");
	const TCHAR* DefaultThrowSound = TEXT("/Game/Developers/goldb/Audio/SW_Throw.SW_Throw");

	// 놓는 순간 이 속도(cm/s) 이상이면 '던지기'로 판정 (던지기 임펄스=1000, 운반 속도≈300)
	constexpr float ThrowSpeedThreshold = 600.f;

	// 잔여시간 임박 시 남은 가구 빨간 아웃라인(스텐실 4) — 제한 5분 중 마지막 60초.
	// BGM 배속(TCFeedbackSubsystem::BGMSpeedupRemaining=60)과 같은 순간에 발동한다.
	constexpr float UrgentTimeLimit = 300.f;
	constexpr float UrgentRemaining = 60.f;

	// 내구도 감소(타격) 시 재생 — 2종 교대 (헤더 무수정을 위해 cpp 로컬 상수)
	const TCHAR* DefaultHitSounds[] = {
		TEXT("/Game/Developers/goldb/Audio/SW_Hit01.SW_Hit01"),
		TEXT("/Game/Developers/goldb/Audio/SW_Hit02.SW_Hit02"),
	};
	const TCHAR* DefaultPickupFX = TEXT("/Game/Developers/goldb/VFX/NS_GrabPuff.NS_GrabPuff");
	const TCHAR* DefaultBreakSound = TEXT("/Game/Developers/goldb/Audio/SW_Impact.SW_Impact");
	const TCHAR* DefaultBreakFX = TEXT("/Game/Developers/goldb/VFX/NS_ImpactPuff.NS_ImpactPuff");
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
		}
		else if (LastHealth > 0.f && Health < LastHealth - KINDA_SMALL_NUMBER)
		{
			// 내구도 깎임 — 소프트 우드 히트 (2종 랜덤 + 피치 흔들림)
			const int32 HitIdx = FMath::RandRange(0, 1);
			if (USoundBase* HitS = LoadObject<USoundBase>(nullptr, DefaultHitSounds[HitIdx]))
			{
				UGameplayStatics::PlaySoundAtLocation(this, HitS, Owner->GetActorLocation(),
					1.f, FMath::RandRange(0.9f, 1.1f));
			}
		}
		LastHealth = Health;
	}

	const bool bGrabbed = ReadGrabbed();

	// ── 운반 하이라이트: 잡혀 있는 동안 스텐실 3(초록 링)을 매 틱 재주장 ──
	// 포커스 시스템(OnUnfocus)이 커스텀뎁스를 꺼도 다음 틱에 즉시 복구된다.
	if (bGrabbed)
	{
		if (UStaticMeshComponent* MeshC = Owner->FindComponentByClass<UStaticMeshComponent>())
		{
			if (MeshC->CustomDepthStencilValue != 3) { MeshC->SetCustomDepthStencilValue(3); }
			if (!MeshC->bRenderCustomDepth) { MeshC->SetRenderCustomDepth(true); }
		}
	}
	else
	{
		// ── 시간 임박: 남아 있는(안 잡힌) 가구에 빨간 링(스텐실 4) 재주장 ──
		// 남은 시간에 어느 가구를 옮겨야 하는지 한눈에 보이게 한다.
		// 제한시간 정본은 게임모드(서버) — 클라 폴백은 UrgentTimeLimit 상수.
		UWorld* W = GetWorld();
		const ATeamCarryGameState* GS = W ? W->GetGameState<ATeamCarryGameState>() : nullptr;
		float EffectiveLimit = UrgentTimeLimit;
		if (W)
		{
			if (const ATeamCarryGameMode* GM = W->GetAuthGameMode<ATeamCarryGameMode>())
			{
				EffectiveLimit = GM->TimeLimitSeconds;
			}
		}
		const bool bUrgent = GS && !GS->bIsGameFinished
			&& GS->CurrentPhase == EGamePhase::Playing
			&& (EffectiveLimit - GS->ElapsedTime) <= UrgentRemaining;
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

	// 놓는 순간: 포커스 규칙(스텐실 1)으로 복원하고 링은 끈다 (포커스하면 다시 켜짐)
	if (!bGrabbed)
	{
		if (UStaticMeshComponent* MeshC = Owner->FindComponentByClass<UStaticMeshComponent>())
		{
			MeshC->SetCustomDepthStencilValue(1);
			MeshC->SetRenderCustomDepth(false);
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
