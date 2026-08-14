#pragma once
 
#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ClearBlackboardValue.generated.h"
 
/**
  지정한 블랙보드 키의 값을 비우는 태스크 클래스
 */
UCLASS()
class TPSGAME_API UBTTask_ClearBlackboardValue : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
 
public:
	UBTTask_ClearBlackboardValue();
 
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};