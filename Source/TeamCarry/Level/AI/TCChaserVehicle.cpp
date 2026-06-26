// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/AI/TCChaserVehicle.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

ATCChaserVehicle::ATCChaserVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(TEXT("Pawn"));

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Body);
	HitBox->SetBoxExtent(FVector(150.f, 100.f, 80.f));
	HitBox->SetCollisionProfileName(TEXT("OverlapAllDynamic")); 
}

void ATCChaserVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATCChaserVehicle, bDisabled);
}

void ATCChaserVehicle::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SetDriveInput(1.f, 0.f);
	}
}

void ATCChaserVehicle::SetDriveInput(float InThrottle, float InSteer)
{
	CurrentThrottle = FMath::Clamp(InThrottle, -1.f, 1.f);
	CurrentSteer = FMath::Clamp(InSteer, -1.f, 1.f);
}

void ATCChaserVehicle::Disable()
{
	if (!HasAuthority() || bDisabled) return;

	bDisabled = true;
	
	SwerveDir = (FMath::RandBool()) ? 1.f : -1.f;
	CurrentThrottle = 0.f;
}

void ATCChaserVehicle::OnRep_Disabled()
{
	// 클라 시각 처리 훅
}

void ATCChaserVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UE_LOG(LogTemp, Warning, TEXT("[Chaser] Tick Speed=%.1f Throttle=%.1f Auth=%d"),
		CurrentSpeed, CurrentThrottle, HasAuthority());
	if (!HasAuthority()) return; // 서버 권위 이동

	if (bDisabled)
	{
		// 무력화: 가속 끊고 관성 감속 + 한쪽으로 꺾으며 멈춤
		const float Drop = BrakeDecel * DeltaSeconds;
		CurrentSpeed = FMath::Max(0.f, CurrentSpeed - Drop);

		if (CurrentSpeed > 10.f)
		{
			AddActorWorldRotation(FRotator(0.f, SwerveDir * SwerveYawRate * DeltaSeconds, 0.f));
			AddActorWorldOffset(GetActorForwardVector() * CurrentSpeed * DeltaSeconds, false);
		}
		return;
	}

	// 정상 추격 이동
	if (!FMath::IsNearlyZero(CurrentThrottle))
	{
		CurrentSpeed += CurrentThrottle * Acceleration * DeltaSeconds;
	}
	else
	{
		const float Drop = BrakeDecel * DeltaSeconds;
		CurrentSpeed = (CurrentSpeed > 0.f) ? FMath::Max(0.f, CurrentSpeed - Drop)
			: FMath::Min(0.f, CurrentSpeed + Drop);
	}
	CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.5f, MaxSpeed);

	AddActorWorldOffset(GetActorForwardVector() * CurrentSpeed * DeltaSeconds, true);

	if (!FMath::IsNearlyZero(CurrentSpeed) && !FMath::IsNearlyZero(CurrentSteer))
	{
		const float SpeedRatio = CurrentSpeed / MaxSpeed;
		AddActorWorldRotation(FRotator(0.f, CurrentSteer * TurnRateAtSpeed * SpeedRatio * DeltaSeconds, 0.f));
	}
}