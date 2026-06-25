// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Struct/BreakableProp.h"
#include "Components/StaticMeshComponent.h"
#include "Core/MovableFurniture.h"
#include "Level/StageManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ABreakableProp::ABreakableProp()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
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
	return OtherActor && OtherActor->Implements<UMovableFurniture>();
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

