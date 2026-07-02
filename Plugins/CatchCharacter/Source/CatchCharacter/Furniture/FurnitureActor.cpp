// Fill out your copyright notice in the Description page of Project Settings.

#include "FurnitureActor.h"
#include "Components/StaticMeshComponent.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureDamage.h"

AFurnitureActor::AFurnitureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	FurnitureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FurnitureMesh"));
	RootComponent = FurnitureMesh;

	FurnitureStat = CreateDefaultSubobject<UFurnitureStat>(TEXT("FurnitureStat"));
	
	GrabSystem = CreateDefaultSubobject<UFurnitureGrabSystem>(TEXT("GrabSystem"));
	
	DamageSystem = CreateDefaultSubobject<UFurnitureDamage>(TEXT("DamageSystem"));

	FurnitureMesh->OnComponentHit.AddDynamic(DamageSystem, &UFurnitureDamage::OnHit);
}

void AFurnitureActor::BeginPlay()
{
	Super::BeginPlay();

	// 물리설정
	if (FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	}

	// 서버에서 스텟정보 초기화
	if (HasAuthority())
	{
		if (!FurnitureDataRow.IsNull())
		{
			FFurnitureData* Data = FurnitureDataRow.GetRow<FFurnitureData>(TEXT("FurnitureActor_Init"));
			if (Data && FurnitureStat)
			{
				FurnitureStat->InitializeStats(*Data);
			}
		}
		if (IsValid(FurnitureStat) && IsValid(DamageSystem))
		{
			DamageSystem->Setup(FurnitureStat);
		}
	}

	// 그랩 시스템 초기화
	if (GrabSystem)
	{
		GrabSystem->Setup(FurnitureMesh, FurnitureStat);
	}

	// 가구만 이동/회전 디버깅용 코드
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			TestTimerHandle,
			this,
			&AFurnitureActor::ExecuteTestOffset,
			1.f,
			true
		);
	}
}

void AFurnitureActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFurnitureActor::SetHighlight(bool bEnabled)
{
	// 지금은 더미임
}

void AFurnitureActor::FurnitureOffset(FVector LocationOffset, float YawOffset)
{
	if (HasAuthority() && bTestAutoOffset && GrabSystem)
	{
		GrabSystem->AddFurnitureOffset(LocationOffset, YawOffset);
	}
}

void AFurnitureActor::ExecuteTestOffset()
{
	if (HasAuthority() && bTestAutoOffset && GrabSystem)
	{
		// offset량 설정
		FVector FrameLocOffset = TestLocationSpeed * 1.f;
		float FrameYawOffset = TestYawSpeed * 1.f;

		// 새로 추가했던 오프셋 전용 함수 호출
		GrabSystem->AddFurnitureOffset(FrameLocOffset, FrameYawOffset);
	}
}
