// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Struct/InteractableDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AInteractableDoor::AInteractableDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorRoot);
	DoorMesh->SetRenderCustomDepth(false); // 외곽선 기본 OFF
}

void AInteractableDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInteractableDoor, bIsOpen);
	DOREPLIFETIME(AInteractableDoor, bIsInteracting);
}

void AInteractableDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DoorMesh->SetRelativeRotation(ClosedRotation);
}

void AInteractableDoor::BeginPlay()
{
	Super::BeginPlay();
	DoorMesh->SetRelativeRotation(bIsOpen ? OpenRotation : ClosedRotation);
	bAnimating = false;
}


bool AInteractableDoor::CanInteract_Implementation(ATCPlayerCharacter* /*Player*/)
{
	return !bIsInteracting; // 애니메이션 중이면 전 클라 잠금
}

void AInteractableDoor::OnInteract_Implementation(ATCPlayerCharacter* /*Player*/)
{
	// GrabComponent의 ServerTryInteract를 거쳐 서버에서 호출됨
	if (HasAuthority())
	{
		HandleInteract();
	}
}

void AInteractableDoor::OnFocus_Implementation()
{
	DoorMesh->SetRenderCustomDepth(true);  // 외곽선 하이라이트 ON
}

void AInteractableDoor::OnUnfocus_Implementation()
{
	DoorMesh->SetRenderCustomDepth(false); // OFF
}

void AInteractableDoor::HandleInteract()
{
	if (!HasAuthority() || bIsInteracting) return;

	bIsInteracting = true; // 복제  전 클라 잠금
	bIsOpen = !bIsOpen;    // 목표 토글  복제
	StartAnimation();
	ForceNetUpdate();
}

void AInteractableDoor::StartAnimation() { bAnimating = true; }

void AInteractableDoor::OnRep_IsOpen() { StartAnimation(); }
void AInteractableDoor::OnRep_IsInteracting() { /* 잠금 상태 변화 훅 */ }

void AInteractableDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAnimating) return;

	const FRotator Target = bIsOpen ? OpenRotation : ClosedRotation;
	const FRotator Current = DoorMesh->GetRelativeRotation();
	const FRotator NewRot = FMath::RInterpConstantTo(Current, Target, DeltaSeconds, OpenSpeed);
	DoorMesh->SetRelativeRotation(NewRot);

	if (NewRot.Equals(Target, 0.5f))
	{
		DoorMesh->SetRelativeRotation(Target);
		bAnimating = false;

		if (HasAuthority()) // 열림 판정은 서버 단독
		{
			bIsInteracting = false; // 전 클라 잠금 해제
			ForceNetUpdate();
		}
	}
}
