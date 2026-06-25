// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TCBTTask_FindPatrolPoint.generated.h"

/**
 * 
 */
UCLASS()
class TEAMCARRY_API UTCBTTask_FindPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UTCBTTask_FindPatrolPoint();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	// 배회 반경 (스폰 혹은 현재 위치 기준)
	UPROPERTY(EditAnywhere, Category = "AI")
	float PatrolRadius = 800.f;
};