#include "Level/AI/TCBTTask_GrabFurniture.h"
#include "Level/AI/TCRobberAIController.h"
#include "Level/AI/TCRobberCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"

UTCBTTask_GrabFurniture::UTCBTTask_GrabFurniture()
{
	NodeName = TEXT("Grab Furniture");
}

EBTNodeResult::Type UTCBTTask_GrabFurniture::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	ATCRobberCharacter* Robber = Cast<ATCRobberCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(ATCRobberAIController::Key_TargetFurniture));
	if (!Robber || !Target) return EBTNodeResult::Failed;

	Robber->GrabFurniture(Target); 
	return Robber->IsCarrying() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
