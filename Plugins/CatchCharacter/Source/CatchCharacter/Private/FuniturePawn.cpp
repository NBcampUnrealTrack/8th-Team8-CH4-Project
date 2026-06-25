#include "CatchCharacter/Public/FuniturePawn.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"

AFuniturePawn::AFuniturePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	FurnitureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FurnitureMesh"));
	RootComponent = FurnitureMesh;

	FurnitureStat = CreateDefaultSubobject<UFurnitureStat>(TEXT("FurnitureStat"));
}

void AFuniturePawn::BeginPlay()
{
	Super::BeginPlay();

	// 물리 설정은 반드시 BeginPlay에서 수행
	FurnitureMesh->SetSimulatePhysics(true);
	FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	FurnitureMesh->SetMassOverrideInKg(NAME_None, 100.0f);
	FurnitureMesh->SetLinearDamping(1.0f);
	FurnitureMesh->SetAngularDamping(2.0f);

	if (HasAuthority())
	{
		if (!FurnitureDataRow.IsNull())
		{
			FFurnitureData* Data = FurnitureDataRow.GetRow<FFurnitureData>(TEXT("FurnitureInit"));
			if (Data && FurnitureStat)
			{
				FurnitureStat->InitializeStats(*Data);
				UpdatePhysicsState();
			}
		}
	}
}

void AFuniturePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && GrabbedPlayers.Num() > 0)
	{
		FVector FurnitureVelocity = FurnitureMesh->GetPhysicsLinearVelocity();
		
		if (FurnitureVelocity.SizeSquared() < 100.f)
		{
			FurnitureVelocity = FVector::ZeroVector;
		}

		for (ACharacter* Player : GrabbedPlayers)
		{
			if (Player)
			{
				UCharacterMovementComponent* CMC = Player->GetCharacterMovement();
				if (CMC)
				{
					FVector CurrentVel = CMC->Velocity;
					CMC->Velocity = FMath::VInterpTo(CurrentVel, FurnitureVelocity, DeltaTime, 10.0f);
				}
			}
		}
	}
}

void AFuniturePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFuniturePawn, GrabbedPlayers);
}

void AFuniturePawn::Grab(ACharacter* Grabber, UPrimitiveComponent* GrabberComponent)
{
	if (!HasAuthority() || !Grabber || !GrabberComponent || GrabbedPlayers.Contains(Grabber)) return;

	UCapsuleComponent* CharacterCapsule = Grabber->GetCapsuleComponent();
	if (!CharacterCapsule) return;

	UPhysicsConstraintComponent* NewConstraint = NewObject<UPhysicsConstraintComponent>(this);
	NewConstraint->RegisterComponent();
	NewConstraint->AttachToComponent(FurnitureMesh, FAttachmentTransformRules::KeepWorldTransform);
	NewConstraint->SetWorldLocation(GrabberComponent->GetComponentLocation());

	NewConstraint->SetConstrainedComponents(CharacterCapsule, NAME_None, FurnitureMesh, NAME_None);
	ConfigureConstraint(NewConstraint);

	GrabbedPlayers.Add(Grabber);
	ActiveConstraints.Add(Grabber, NewConstraint);

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}

	UpdatePhysicsState();
}

void AFuniturePawn::ConfigureConstraint(UPhysicsConstraintComponent* NewConstraint)
{
	if (!NewConstraint) return;


	NewConstraint->SetLinearXLimit(LCM_Locked, 0.0f);
	NewConstraint->SetLinearYLimit(LCM_Locked, 0.0f);
	NewConstraint->SetLinearZLimit(LCM_Limited, 10.0f);
	NewConstraint->SetAngularSwing1Limit(ACM_Locked, 0.0f);
	NewConstraint->SetAngularSwing2Limit(ACM_Locked, 0.0f);
	NewConstraint->SetAngularTwistLimit(ACM_Locked, 0.0f);

	NewConstraint->SetDisableCollision(true);
}

void AFuniturePawn::Release(ACharacter* Grabber)
{
	if (!HasAuthority() || !Grabber || !GrabbedPlayers.Contains(Grabber)) return;

	if (UPhysicsConstraintComponent** FoundConstraint = ActiveConstraints.Find(Grabber))
	{
		if (*FoundConstraint)
		{
			(*FoundConstraint)->DestroyComponent();
		}
		ActiveConstraints.Remove(Grabber);
	}

	GrabbedPlayers.Remove(Grabber);

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}

	UpdatePhysicsState();
}

void AFuniturePawn::UpdatePhysicsState()
{
	if (!HasAuthority() || !FurnitureStat) return;

	if (FurnitureStat->IsRequirementMet())
	{
		FurnitureMesh->SetLinearDamping(0.5f);
	}
	else
	{
		if (GrabbedPlayers.Num() > 0)
		{
			FurnitureMesh->SetLinearDamping(15.0f);
		}
		else
		{
			FurnitureMesh->SetLinearDamping(5.0f);
		}
	}
}

void AFuniturePawn::OnRep_GrabbedPlayers() {}
