// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/AI/TCBTService_CheckStealable.h"
#include "Level/AI/TCRobberAIController.h"
#include "Level/LevelState/TCGoalZone.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UTCBTService_CheckStealable::UTCBTService_CheckStealable()
{
	NodeName = TEXT("Check Stealable");
	Interval = 0.3f;       // 확인하는 주기
	RandomDeviation = 0.05f;
}

void UTCBTService_CheckStealable::TickNode(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return;

	ATCGoalZone* Zone = Cast<ATCGoalZone>(
		UGameplayStatics::GetActorOfClass(OwnerComp.GetWorld(), ATCGoalZone::StaticClass()));

	AActor* Target = Zone ? Zone->GetAnyDeliveredFurniture() : nullptr;

	// 짐을 TargetFurniture에 직접 기록 (있으면 채우고, 없으면 비움)
	BB->SetValueAsObject(ATCRobberAIController::Key_TargetFurniture, Target);
}
