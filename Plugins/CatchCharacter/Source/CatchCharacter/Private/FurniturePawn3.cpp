#include "CatchCharacter/Public/FurniturePawn3.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "Kismet/KismetMathLibrary.h"

AFurniturePawn3::AFurniturePawn3()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	FurnitureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FurnitureMesh"));
	RootComponent = FurnitureMesh;

	FurnitureStat = CreateDefaultSubobject<UFurnitureStat>(TEXT("FurnitureStat"));
}

void AFurniturePawn3::BeginPlay()
{
	Super::BeginPlay();

	// 평소에는 물리 시뮬레이션 활성화
	if (FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	}

	if (HasAuthority())
	{
		if (!FurnitureDataRow.IsNull())
		{
			FFurnitureData* Data = FurnitureDataRow.GetRow<FFurnitureData>(TEXT("FurniturePawn_Init"));
			if (Data && FurnitureStat)
			{
				FurnitureStat->InitializeStats(*Data);
			}
		}
	}
}

void AFurniturePawn3::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && GrabbedPlayers.Num() > 0)
	{
		HandleMovement(DeltaTime);
		SyncPlayersToFurniture();
	}
}

void AFurniturePawn3::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFurniturePawn3, GrabbedPlayers);
}

void AFurniturePawn3::Grab(ACharacter* Grabber, UPrimitiveComponent* GrabberComponent)
{
	if (!HasAuthority() || !Grabber || !GrabberComponent || GrabbedPlayers.Contains(Grabber)) return;

	if (GrabbedPlayers.Num() == 0)
	{
		FurnitureMesh->SetSimulatePhysics(false);
		FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		
		// 바닥 충돌 방지용 미세 부양
		AddActorWorldOffset(FVector(0, 0, 5.0f));
	}

	// 캐릭터와 가구가 서로 밀어내지 않도록 설정
	Grabber->MoveIgnoreActorAdd(this);
	this->MoveIgnoreActorAdd(Grabber);

	GrabbedPlayers.Add(Grabber);

	// 잡는 순간의 가구 대비 플레이어의 상대 좌표 저장 (소켓 지원 구조)
	FTransform FurnitureTransform = GetActorTransform();
	FTransform GrabberTransform = GrabberComponent->GetComponentTransform(); // 손이나 지정된 컴포넌트 위치
	FTransform RelativeTransform = GrabberTransform.GetRelativeTransform(FurnitureTransform);

	FGrabInfo Info;
	Info.GrabberComponent = GrabberComponent;
	Info.RelativeLocation = RelativeTransform.GetLocation();
	Info.RelativeRotation = RelativeTransform.Rotator();
	GrabInfos.Add(Grabber, Info);

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

void AFurniturePawn3::Release(ACharacter* Grabber)
{
	if (!HasAuthority() || !Grabber || !GrabbedPlayers.Contains(Grabber)) return;

	Grabber->MoveIgnoreActorRemove(this);
	this->MoveIgnoreActorRemove(Grabber);

	GrabbedPlayers.Remove(Grabber);
	GrabInfos.Remove(Grabber);

	if (GrabbedPlayers.Num() == 0)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	}

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

void AFurniturePawn3::HandleMovement(float DeltaTime)
{
	if (!FurnitureStat || !FurnitureStat->IsRequirementMet()) return;

	FVector CombinedMoveVector = FVector::ZeroVector;
	int32 ActiveInputs = 0;

	for (ACharacter* Player : GrabbedPlayers)
	{
		if (Player && Player->GetCharacterMovement())
		{
			// 가속도 기반 방향 도출
			FVector PlayerInput = Player->GetCharacterMovement()->GetCurrentAcceleration();
			
			if (PlayerInput.SizeSquared() > 0.01f)
			{
				CombinedMoveVector += PlayerInput.GetSafeNormal();
				ActiveInputs++;
			}
		}
	}

	if (ActiveInputs > 0)
	{
		FVector MoveDirection = CombinedMoveVector.GetSafeNormal();
		float MaxSpeed = FurnitureStat->GetBaseSpeed();
		if(MaxSpeed <= 0) MaxSpeed = 300.f;

		FVector CurrentLocation = GetActorLocation();
		FVector NewLocation = CurrentLocation + (MoveDirection * MaxSpeed * DeltaTime);
		
		SetActorLocation(NewLocation, true);
	}
}

void AFurniturePawn3::SyncPlayersToFurniture()
{
	FTransform FurnitureTransform = GetActorTransform();

	for (ACharacter* Player : GrabbedPlayers)
	{
		if (Player && GrabInfos.Contains(Player))
		{
			FGrabInfo Info = GrabInfos[Player];

			// 가구의 이동 + 처음 잡았을 때의 오프셋 = 캐릭터가 있어야 할 절대 위치
			FTransform TargetTransform = FTransform(Info.RelativeRotation, Info.RelativeLocation) * FurnitureTransform;

			// 플레이어가 도망가지 못하게 목표 위치로 텔레포트 (안정적인 Pawn2 방식)
			Player->SetActorLocation(TargetTransform.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			
			// 회전은 캐릭터가 서 있는 상태를 유지하도록 Z축(Yaw)만 가져옴
			FRotator TargetRot = TargetTransform.Rotator();
			TargetRot.Pitch = 0.0f;
			TargetRot.Roll = 0.0f;
			Player->SetActorRotation(TargetRot, ETeleportType::TeleportPhysics);
		}
	}
}

void AFurniturePawn3::OnRep_GrabbedPlayers() 
{
}