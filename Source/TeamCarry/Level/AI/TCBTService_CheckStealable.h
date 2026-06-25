// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "TCBTService_CheckStealable.generated.h"

/**
 * 
 */
UCLASS()
class TEAMCARRY_API UTCBTService_CheckStealable : public UBTService
{
	GENERATED_BODY()
	
public:
	UTCBTService_CheckStealable();
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

