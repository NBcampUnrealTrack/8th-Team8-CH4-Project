// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TCBTTask_SetEscapeLocation.generated.h"

/**
 * 
 */
UCLASS()
class TEAMCARRY_API UTCBTTask_SetEscapeLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UTCBTTask_SetEscapeLocation();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	// 레벨의 도주 지점 액터에 붙일 태그
	UPROPERTY(EditAnywhere, Category = "AI")
	FName EscapePointTag = TEXT("RobberEscape");
};
	
