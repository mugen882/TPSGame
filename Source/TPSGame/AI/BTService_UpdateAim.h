#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_UpdateAim.generated.h"

/*
    에임위치 설정하는 서비스 클래스
    옵션에 따라 요격점 예측
*/

UCLASS()
class TPSGAME_API UBTService_UpdateAim : public UBTService_BlackboardBase
{
    GENERATED_BODY()

public:
    UBTService_UpdateAim();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    // 추가 키 셀렉터(TargetActor)를 블랙보드 애셋에 대해 해석한다.
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

private:
    // 타겟 액터 키 (BlackboardKey는 부모가 AimLocation 용으로 제공)
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKeySelector;
};