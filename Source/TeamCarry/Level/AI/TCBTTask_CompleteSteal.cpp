// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/AI/TCBTTask_CompleteSteal.h"
#include "Level/AI/TCRobberAIController.h"
#include "Level/AI/TCRobberCharacter.h"
#include "Level/LevelState/TCGoalZone.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UTCBTTask_CompleteSteal::UTCBTTask_CompleteSteal()
{
	NodeName = TEXT("Complete Steal");
}

EBTNodeResult::Type UTCBTTask_CompleteSteal::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	ATCRobberCharacter* Robber = Cast<ATCRobberCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(ATCRobberAIController::Key_TargetFurniture));
	if (!Robber || !Target) return EBTNodeResult::Failed;

	// GoalZone에 훔침 통보 → DeliveredCount 감소 + Set에서 제거
	if (ATCGoalZone* Zone = Cast<ATCGoalZone>(
		UGameplayStatics::GetActorOfClass(OwnerComp.GetWorld(), ATCGoalZone::StaticClass())))
	{
		Zone->OnFurnitureStolen(Target);
	}

	// 짐 떼고 제거 (도주 완료 = 짐이 맵에서 사라짐)
	Robber->ReleaseFurniture();
	Target->Destroy();

	BB->ClearValue(ATCRobberAIController::Key_TargetFurniture);
	return EBTNodeResult::Succeeded;
}
