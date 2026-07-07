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


	// 데이터 테이블의 Mass를 실제 물리 바디에 반영.
	const float StatMass = GetFurnitureStat()->GetMass();
	if (FurnitureMesh && StatMass > 0.f)
	{
		FurnitureMesh->SetMassOverrideInKg(NAME_None, StatMass, true);
	}



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
	// PP 아웃라인 셰이더(M_PP_OutlineHLSL)가 CustomStencil==1 실루엣 둘레에 하이라이트 링을 그린다
	if (FurnitureMesh)
	{
		FurnitureMesh->SetCustomDepthStencilValue(1);
		FurnitureMesh->SetRenderCustomDepth(bEnabled);
	}
}

void AFurnitureActor::FurnitureOffset(FVector LocationOffset, float YawOffset, float PitchOffset)
{
	// 서버에서만 이루어짐
	if (HasAuthority() && GrabSystem)
	{
		GrabSystem->AddFurnitureOffset(LocationOffset, YawOffset, PitchOffset);
	}
}

void AFurnitureActor::ExecuteTestOffset()
{
	// 디버그 자동 이동/회전: bTestAutoOffset 켠 가구만
	if (HasAuthority() && bTestAutoOffset && GrabSystem)
	{
		GrabSystem->AddFurnitureOffset(TestLocationSpeed, TestYawSpeed, TestPitchSpeed);
	}
}
