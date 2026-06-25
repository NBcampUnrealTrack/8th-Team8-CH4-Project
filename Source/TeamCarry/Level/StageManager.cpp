// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/StageManager.h"
#include "Core/MovableFurniture.h"
#include "Core/StageHost.h"
#include "GameFramework/GameModeBase.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AStageManager::AStageManager()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AStageManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStageManager, TargetCount);
	DOREPLIFETIME(AStageManager, DeliveredCount);
	DOREPLIFETIME(AStageManager, DamageCount);
	DOREPLIFETIME(AStageManager, bStageCleared);
}

void AStageManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		TargetCount = (TargetCountOverride > 0) ? TargetCountOverride : CountFurnitureInLevel();
		OnStageProgress.Broadcast(DeliveredCount, TargetCount);
	}
}

int32 AStageManager::CountFurnitureInLevel() const
{
	int32 Count = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Implements<UMovableFurniture>()) { ++Count; }
	}
	return Count;
}

void AStageManager::RegisterDelivery(AActor*)
{
	if (!HasAuthority() || bStageCleared) return;
	DeliveredCount += 1;
	OnStageProgress.Broadcast(DeliveredCount, TargetCount);
	EvaluateClear();
}

void AStageManager::RegisterDamage(AActor*)
{
	if (!HasAuthority()) return;
	DamageCount += 1;
}

void AStageManager::EvaluateClear()
{
	if (bStageCleared || TargetCount <= 0 || DeliveredCount < TargetCount) return;

	bStageCleared = true;
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		if (GM->Implements<UStageHost>())
		{
			IStageHost::Execute_OnStageCleared(GM);
		}
	}
}

void AStageManager::UnregisterDelivery(AActor* Furniture)
{
	if (!HasAuthority()) return;
	if (bStageCleared) return;            
	if (DeliveredCount <= 0) return;

	DeliveredCount -= 1;
	OnStageProgress.Broadcast(DeliveredCount, TargetCount);
}

void AStageManager::OnRep_Progress() { OnStageProgress.Broadcast(DeliveredCount, TargetCount); }
void AStageManager::OnRep_Cleared() { /* 클라 결과 UI 트리거 */ }

