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
	}

	// 그랩 시스템 초기화
	if (GrabSystem)
	{
		GrabSystem->Setup(FurnitureMesh, FurnitureStat);
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
