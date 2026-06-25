// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/StageClearProvider.h"
#include "StageManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageProgress, int32, Delivered, int32, Target);

UCLASS()
class TEAMCARRY_API AStageManager : public AActor, public IStageClearProvider
{
	GENERATED_BODY()

public:
	AStageManager();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool  IsStageCleared()    const override { return bStageCleared; }
	virtual int32 GetDeliveredCount() const override { return DeliveredCount; }
	virtual int32 GetTargetCount()    const override { return TargetCount; }

	void RegisterDelivery(AActor* Furniture);
	void RegisterDamage(AActor* DamagedActor);

	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FOnStageProgress OnStageProgress;

	void UnregisterDelivery(AActor* Furniture);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Stage")
	int32 TargetCountOverride = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progress) 
	int32 TargetCount = 0;
	
	UPROPERTY(ReplicatedUsing = OnRep_Progress) 
	int32 DeliveredCount = 0;
	
	UPROPERTY(Replicated)                       
	int32 DamageCount = 0;
	
	UPROPERTY(ReplicatedUsing = OnRep_Cleared)  
	bool  bStageCleared = false;

	UFUNCTION() 
	void OnRep_Progress();
	
	UFUNCTION() 
	void OnRep_Cleared();

private:
	int32 CountFurnitureInLevel() const;
	void  EvaluateClear();
};