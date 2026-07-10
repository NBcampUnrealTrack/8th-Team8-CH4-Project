// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Struct/BreakableProp.h"
#include "Components/StaticMeshComponent.h"
#include "Core/MovableFurniture.h"
#include "Core/TCIDamageable.h"
#include "Furniture/TCFurnitureActor.h"
#include "Level/StageManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ABreakableProp::ABreakableProp()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// 생성자에서 SetSimulatePhysics 금지(끄는 방향도 동일) — CDO 생성 중 물리 머티리얼
	// 조회 에러로 쿠킹이 실패한다. 프로퍼티 직접 기록으로 대체.
	Mesh->BodyInstance.bSimulatePhysics = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetNotifyRigidBodyCollision(true);
}

void ABreakableProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABreakableProp, DamageLevel);
}

void ABreakableProp::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Mesh->OnComponentHit.AddDynamic(this, &ABreakableProp::OnMeshHit);
	}
}

bool ABreakableProp::IsFurniture(AActor* OtherActor) const
{
	return OtherActor && OtherActor->IsA(ATCFurnitureActor::StaticClass());
}

void ABreakableProp::OnMeshHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector NormalImpulse, const FHitResult&)
{
	if (!HasAuthority()) return;
	if (DamageLevel >= MaxDamageLevel) return;
	if (!IsFurniture(OtherActor)) return;
	if (NormalImpulse.Size() < ImpactImpulseThreshold) return;

	DamageLevel = FMath::Clamp(DamageLevel + 1, 0, MaxDamageLevel);
	OnDamageChanged(DamageLevel);

	if (AStageManager* SM = Cast<AStageManager>(
		UGameplayStatics::GetActorOfClass(this, AStageManager::StaticClass())))
	{
		SM->RegisterDamage(this); // 집계만, 감점은 Core
	}
}

void ABreakableProp::OnRep_DamageLevel()
{
	OnDamageChanged(DamageLevel);
}

void ABreakableProp::ApplyDamage_Implementation(int32 Amount, AActor*)
{
	if (!HasAuthority() || DamageLevel >= MaxDamageLevel) return;

	DamageLevel = FMath::Clamp(DamageLevel + Amount, 0, MaxDamageLevel);
	OnDamageChanged(DamageLevel);

	if (AStageManager* SM = Cast<AStageManager>(
		UGameplayStatics::GetActorOfClass(this, AStageManager::StaticClass())))
	{
		SM->RegisterDamage(this);
	}
}