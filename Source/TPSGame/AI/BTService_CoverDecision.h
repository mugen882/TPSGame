#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_CoverDecision.generated.h"

/**
 * 버스트 쿨다운 중이거나 최근 누적피해가 일정이상이면 엄폐하도록 결정하는 서비스
 * 위상 Selector(엄폐/교전)의 게이트 값을 매 틱 갱신한다.
 */
UCLASS()
class TPSGAME_API UBTService_CoverDecision : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_CoverDecision();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
    UPROPERTY(EditAnywhere, Category = "Cover")
	float MinCoverHoldTime = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Cover")
	float DamageThresholdRatio = 0.1f;   // 최근 누적 피해가 MaxHealth의 이 비율 이상이면 엄폐

    float CoverEnterTime = -1000.f;
};