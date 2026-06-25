// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/AI/TCBTTask_SetEscapeLocation.h"
#include "Level/AI/TCRobberAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UTCBTTask_SetEscapeLocation::UTCBTTask_SetEscapeLocation()
{
	NodeName = TEXT("Set Escape Location");
}

EBTNodeResult::Type UTCBTTask_SetEscapeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	// 태그 붙은 도주 지점 찾기
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(OwnerComp.GetWorld(), EscapePointTag, Found);
	if (Found.Num() == 0 || !Found[0]) return EBTNodeResult::Failed;

	BB->SetValueAsVector(ATCRobberAIController::Key_EscapeLocation, Found[0]->GetActorLocation());
	return EBTNodeResult::Succeeded;
}
