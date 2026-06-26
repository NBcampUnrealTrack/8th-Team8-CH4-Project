// TCCarrySpeedComponent.cpp
#include "Player/Component/TCCarrySpeedComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UTCCarrySpeedComponent::UTCCarrySpeedComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTCCarrySpeedComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTCCarrySpeedComponent, CarrySpeed);
	DOREPLIFETIME(UTCCarrySpeedComponent, bIsCarrying);
}

void UTCCarrySpeedComponent::BeginPlay()
{
	Super::BeginPlay();

	// 원래 이동속도 백업 (BeginPlay 시점의 CMC 기본값)
	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* CMC = Owner->GetCharacterMovement())
		{
			OriginalMaxWalkSpeed = CMC->MaxWalkSpeed;
		}
	}
}

void UTCCarrySpeedComponent::SetCarrySpeed(float NewSpeed)
{
	if (!GetOwner()->HasAuthority()) return;

	if (!bIsCarrying)
	{
		// 처음 운반 시작: 현재 MaxWalkSpeed를 원본으로 백업
		if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* CMC = Owner->GetCharacterMovement())
			{
				OriginalMaxWalkSpeed = CMC->MaxWalkSpeed;
			}
		}
	}

	CarrySpeed  = NewSpeed;
	bIsCarrying = true;
	ApplySpeedToCMC(CarrySpeed);  // 서버 즉시 적용
}

void UTCCarrySpeedComponent::ClearCarrySpeed()
{
	if (!GetOwner()->HasAuthority()) return;

	CarrySpeed  = 0.0f;
	bIsCarrying = false;
	ApplySpeedToCMC(OriginalMaxWalkSpeed);  // 서버: 원래 속도 복원
}

void UTCCarrySpeedComponent::OnRep_CarrySpeed()
{
	// 클라이언트: 복제된 값 수신 시 CMC 적용
	if (bIsCarrying)
	{
		ApplySpeedToCMC(CarrySpeed);
	}
	else
	{
		ApplySpeedToCMC(OriginalMaxWalkSpeed);
	}
}

void UTCCarrySpeedComponent::ApplySpeedToCMC(float Speed)
{
	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* CMC = Owner->GetCharacterMovement())
		{
			CMC->MaxWalkSpeed = Speed;
		}
	}
}
