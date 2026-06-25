// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TCBTTask_FindStolenTarget.generated.h"

/**
 * 
 */
UCLASS()
class TEAMCARRY_API UTCBTTask_FindStolenTarget : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UTCBTTask_FindStolenTarget();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
