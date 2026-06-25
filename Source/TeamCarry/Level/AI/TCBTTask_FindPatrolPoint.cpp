// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/AI/TCBTTask_FindPatrolPoint.h"
#include "Level/AI/TCRobberAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "AIController.h"

UTCBTTask_FindPatrolPoint::UTCBTTask_FindPatrolPoint()
{
	NodeName = TEXT("Find Patrol Point");
}

EBTNodeResult::Type UTCBTTask_FindPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!BB || !AICon || !AICon->GetPawn()) return EBTNodeResult::Failed;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(OwnerComp.GetWorld());
	if (!Nav) return EBTNodeResult::Failed;

	// 현재 위치 기준 반경 안에서 NavMesh 위 랜덤 지점
	const FVector Origin = AICon->GetPawn()->GetActorLocation();
	FNavLocation Result;
	if (Nav->GetRandomReachablePointInRadius(Origin, PatrolRadius, Result))
	{
		BB->SetValueAsVector(ATCRobberAIController::Key_PatrolLocation, Result.Location);
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}
