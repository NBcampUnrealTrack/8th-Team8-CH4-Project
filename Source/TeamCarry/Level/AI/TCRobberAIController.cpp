// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/AI/TCRobberAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

const FName ATCRobberAIController::Key_TargetFurniture(TEXT("TargetFurniture"));
const FName ATCRobberAIController::Key_EscapeLocation(TEXT("EscapeLocation"));
const FName ATCRobberAIController::Key_PatrolLocation(TEXT("PatrolLocation"));

ATCRobberAIController::ATCRobberAIController()
{
}

void ATCRobberAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset); // Blackboard도 BT 에셋에 연결,자동 초기화
	}
}
