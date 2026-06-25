// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TCRobberAIController.generated.h"

class UBehaviorTree;
class UBlackboardComponent;

/**
 * 
 */
UCLASS()
class TEAMCARRY_API ATCRobberAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATCRobberAIController();

	virtual void OnPossess(APawn* InPawn) override;

	static const FName Key_TargetFurniture;
	static const FName Key_EscapeLocation;
	static const FName Key_PatrolLocation;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;
};
	
