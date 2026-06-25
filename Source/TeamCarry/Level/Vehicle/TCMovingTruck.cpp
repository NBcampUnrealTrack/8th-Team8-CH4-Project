// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Vehicle/TCMovingTruck.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Net/UnrealNetwork.h"

ATCMovingTruck::ATCMovingTruck()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	// 트럭 차체 회전은 조향 코드로만 (컨트롤러 회전 차단)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessAI = EAutoPossessAI::Disabled;

	TruckBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TruckBody"));
	SetRootComponent(TruckBody);
	TruckBody->SetCollisionProfileName(TEXT("Pawn"));

	CargoFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoFloor"));
	CargoFloor->SetupAttachment(TruckBody);
	CargoFloor->SetCollisionProfileName(TEXT("BlockAll")); // 캐릭터가 딛는 베이스

	DriverSeat = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeat"));
	DriverSeat->SetupAttachment(TruckBody);

	DriverInteractVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DriverInteractVolume"));
	DriverInteractVolume->SetupAttachment(TruckBody);
	DriverInteractVolume->SetBoxExtent(FVector(80.f));
	DriverInteractVolume->SetCollisionProfileName(TEXT("Trigger"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(TruckBody);
	SpringArm->TargetArmLength = 700.f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ATCMovingTruck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATCMovingTruck, CurrentDriver);
}

void ATCMovingTruck::BeginPlay()
{
	Super::BeginPlay();
}

// ── 빙의 콜백 (IMC on/off) ──
void ATCMovingTruck::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AddDriveMappingContextFor(NewController, true);   // 호스트 확정 컨트롤러로 즉시
}

void ATCMovingTruck::UnPossessed()
{
	AddDriveMappingContextFor(GetController(), false);
	Super::UnPossessed();
}

void ATCMovingTruck::OnRep_Controller()
{
	Super::OnRep_Controller();
	AddDriveMappingContextFor(GetController(), true);  // 클라 컨트롤러 복제 완료 시점
}

void ATCMovingTruck::AddDriveMappingContextFor(AController* InController, bool bAdd)
{
	APlayerController* PC = Cast<APlayerController>(InController);
	if (!PC || !PC->IsLocalController()) return;

	if (UEnhancedInputLocalPlayerSubsystem* Sub =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (DriveMappingContext)
		{
			if (bAdd) { Sub->AddMappingContext(DriveMappingContext, 1); }
			else { Sub->RemoveMappingContext(DriveMappingContext); }
		}
	}
}

bool ATCMovingTruck::CanInteract_Implementation(ATCPlayerCharacter* /*Player*/)
{
	return CurrentDriver == nullptr;
}

void ATCMovingTruck::OnFocus_Implementation()
{
	TruckBody->SetRenderCustomDepth(true);
}

void ATCMovingTruck::OnUnfocus_Implementation()
{
	TruckBody->SetRenderCustomDepth(false);
}

void ATCMovingTruck::OnInteract_Implementation(ATCPlayerCharacter* Player)
{
	if (!HasAuthority()) return;
	if (CurrentDriver == nullptr && Player)
	{
		ServerEnter(Player);
	}
}

void ATCMovingTruck::ServerEnter(ATCPlayerCharacter* Player)
{
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	CurrentDriver = Player;

	PC->UnPossess();
	Player->AttachToComponent(DriverSeat, FAttachmentTransformRules::SnapToTargetIncludingScale);
	Player->SetActorEnableCollision(false);
	if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
	{
		Move->DisableMovement();
		Move->StopMovementImmediately();
	}

	PC->Possess(this); // PossessedBy에서 IMC 부착

	// TODO(앉은 포즈): 캐릭터에 OnEnterVehicle() 신호
}

void ATCMovingTruck::ServerExit_Implementation()
{
	if (!CurrentDriver) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	ATCPlayerCharacter* Driver = CurrentDriver;

	if (PC) { PC->UnPossess(); } 

	Driver->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Driver->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Move = Driver->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	const FVector ExitLoc = GetActorLocation() - GetActorRightVector() * 200.f + FVector(0, 0, 50);
	Driver->SetActorLocation(ExitLoc);

	if (PC) { PC->Possess(Driver); }

	// TODO(앉은 포즈): 캐릭터에 OnExitVehicle() 신호

	CurrentDriver = nullptr;
	CurrentSpeed = CurrentThrottle = CurrentSteer = 0.f;
}

void ATCMovingTruck::OnRep_Driver()
{
	//UI/표시 갱신용(필요 시).
}

void ATCMovingTruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ATCMovingTruck::OnThrottle);
		EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ATCMovingTruck::OnThrottle);
		EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ATCMovingTruck::OnSteer);
		EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &ATCMovingTruck::OnSteer);
		EIC->BindAction(ExitAction, ETriggerEvent::Started, this, &ATCMovingTruck::OnExit);
	}
}

void ATCMovingTruck::OnThrottle(const FInputActionValue& V)
{
	CurrentThrottle = V.Get<float>();
	ServerSetInput(CurrentThrottle, CurrentSteer);
}

void ATCMovingTruck::OnSteer(const FInputActionValue& V)
{
	CurrentSteer = V.Get<float>();
	ServerSetInput(CurrentThrottle, CurrentSteer);
}

void ATCMovingTruck::OnExit(const FInputActionValue& /*V*/)
{
	ServerExit();
}

void ATCMovingTruck::ServerSetInput_Implementation(float InThrottle, float InSteer)
{
	CurrentThrottle = InThrottle;
	CurrentSteer = InSteer;
}

void ATCMovingTruck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;

	if (!FMath::IsNearlyZero(CurrentThrottle))
	{
		CurrentSpeed += CurrentThrottle * Acceleration * DeltaSeconds;
	}
	else
	{
		const float Drop = BrakeDecel * DeltaSeconds;
		CurrentSpeed = (CurrentSpeed > 0.f)
			? FMath::Max(0.f, CurrentSpeed - Drop)
			: FMath::Min(0.f, CurrentSpeed + Drop);
	}
	CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.5f, MaxSpeed);

	const FVector Delta = GetActorForwardVector() * CurrentSpeed * DeltaSeconds;
	AddActorWorldOffset(Delta, true);

	if (!FMath::IsNearlyZero(CurrentSpeed) && !FMath::IsNearlyZero(CurrentSteer))
	{
		const float SpeedRatio = CurrentSpeed / MaxSpeed;
		const float YawDelta = CurrentSteer * TurnRateAtSpeed * SpeedRatio * DeltaSeconds;
		AddActorWorldRotation(FRotator(0.f, YawDelta, 0.f));
	}
}

FTransform ATCMovingTruck::GetCargoSpawnTransform(int32 Index) const
{
	if (CargoSpawnPoints.IsValidIndex(Index) && CargoSpawnPoints[Index])
	{
		return CargoSpawnPoints[Index]->GetComponentTransform();
	}
	return GetActorTransform();
}