#include "Level/AI/TCBTTask_FindStolenTarget.h"
#include "TCRobberAIController.h"
#include "Level/LevelState/TCGoalZone.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UTCBTTask_FindStolenTarget::UTCBTTask_FindStolenTarget()
{
	NodeName = TEXT("Find Stolen Target");
}

EBTNodeResult::Type UTCBTTask_FindStolenTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	// 점수산정구역 찾아서 배달된 짐 하나 집기
	ATCGoalZone* Zone = Cast<ATCGoalZone>(
		UGameplayStatics::GetActorOfClass(OwnerComp.GetWorld(), ATCGoalZone::StaticClass()));
	if (!Zone) return EBTNodeResult::Failed;

	AActor* Target = Zone->GetAnyDeliveredFurniture();
	if (!Target) return EBTNodeResult::Failed; // 훔칠 짐 없음

	BB->SetValueAsObject(ATCRobberAIController::Key_TargetFurniture, Target);
	return EBTNodeResult::Succeeded;
}
