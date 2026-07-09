// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFootstepComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	// 플레이스홀더 발소리 4종 (정식 에셋 도입 시 인스턴스에서 교체)
	const TCHAR* DefaultSteps[] = {
		TEXT("/Game/Developers/goldb/Audio/SW_Step01.SW_Step01"),
		TEXT("/Game/Developers/goldb/Audio/SW_Step02.SW_Step02"),
		TEXT("/Game/Developers/goldb/Audio/SW_Step03.SW_Step03"),
		TEXT("/Game/Developers/goldb/Audio/SW_Step04.SW_Step04"),
	};

	// 발소리 1회 재생 — 펭귄 웨이들: 사운드 배열이 [좌,우,좌,우] 순서라
	// 인덱스 패리티를 매번 뒤집어 좌/우 발이 교대로 나가게 한다 (+랜덤 베리에이션)
	void PlayStepAt(UObject* Ctx, ACharacter* OwnerChar,
		const TArray<TObjectPtr<USoundBase>>& Sounds, int32& LastIndex, float Volume)
	{
		if (!OwnerChar || Sounds.Num() == 0)
		{
			return;
		}
		int32 Idx;
		if (Sounds.Num() >= 4)
		{
			// +홀수 = 패리티(좌/우) 반전, +2 랜덤 = 같은 발의 베리에이션 선택
			Idx = (LastIndex + 1 + (FMath::RandBool() ? 2 : 0)) % Sounds.Num();
		}
		else
		{
			Idx = FMath::RandRange(0, Sounds.Num() - 1);
			if (Idx == LastIndex) { Idx = (Idx + 1) % Sounds.Num(); }
		}
		LastIndex = Idx;

		FVector Foot = OwnerChar->GetActorLocation();
		if (const UCapsuleComponent* Cap = OwnerChar->GetCapsuleComponent())
		{
			Foot.Z -= Cap->GetScaledCapsuleHalfHeight();
		}
		UGameplayStatics::PlaySoundAtLocation(Ctx, Sounds[Idx], Foot, Volume,
			FMath::RandRange(0.96f, 1.04f)); // 좌/우 피치차는 에셋에 있으니 랜덤 폭은 좁게
	}
}

UTCFootstepComponent::UTCFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTCFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar)
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (StepSounds.Num() == 0)
	{
		for (const TCHAR* Path : DefaultSteps)
		{
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, Path))
			{
				StepSounds.Add(S);
			}
		}
	}
}

void UTCFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerChar || StepSounds.Num() == 0)
	{
		return;
	}

	const UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement();
	const float Speed = OwnerChar->GetVelocity().Size2D();
	const float IdleSentinel = SlowInterval * 0.5f;
	if (!Move || !Move->IsMovingOnGround() || Speed < MinSpeed)
	{
		// 걷다가 멈춘 첫 틱: 마무리 스텝 한 발 (급정지의 어색함 완화)
		if (StepClock != IdleSentinel)
		{
			if (Move && Move->IsMovingOnGround())
			{
				PlayStepAt(this, OwnerChar, StepSounds, LastIndex, Volume * 0.7f);
			}
			StepClock = IdleSentinel; // 다음 걸음은 절반 지점부터
		}
		return;
	}

	StepClock -= DeltaTime;
	if (StepClock > 0.f)
	{
		return;
	}
	const float Alpha = FMath::GetMappedRangeValueClamped(
		FVector2D(MinSpeed, MaxSpeed), FVector2D(0.f, 1.f), Speed);
	StepClock = FMath::Lerp(SlowInterval, FastInterval, Alpha);

	PlayStepAt(this, OwnerChar, StepSounds, LastIndex, Volume);
}
