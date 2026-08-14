#include "AI/BTTask_ClearBlackboardValue.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearBlackboardValue::UBTTask_ClearBlackboardValue()
{
    NodeName = TEXT("Clear Blackboard Value");
}

EBTNodeResult::Type UBTTask_ClearBlackboardValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->ClearValue(BlackboardKey.SelectedKeyName);
    }
    return EBTNodeResult::Succeeded;
}