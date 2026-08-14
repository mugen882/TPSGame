#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_KeepRange.generated.h"

// 스트레이프 방향 유지를 위한 노드 인스턴스 메모리
struct FBTKeepRangeMemory
{
    float StrafeSign = 1.f;       // +1 / -1
    float StrafeFlipTimer = 0.f;  // 주기적으로 방향 전환
};

/*
    거리 유지하는 서비스 클래스
    머신건일 때 좌우 스트레이프 이동
*/

UCLASS()
class TPSGAME_API UBTService_KeepRange : public UBTService_BlackboardBase
{
    GENERATED_BODY()

public:
    UBTService_KeepRange();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTKeepRangeMemory); }

private:
    // BlackboardKey(부모) = TargetActor(Object)로 사용
    UPROPERTY(EditAnywhere, Category = "Move")
    float AcceptanceRadius = 50.f;

    // 스트레이프 방향을 몇 초마다 뒤집을지
    UPROPERTY(EditAnywhere, Category = "Move")
    float StrafeFlipInterval = 1.5f;

    // 스트레이프 횡이동 거리
    UPROPERTY(EditAnywhere, Category = "Move")
    float StrafeDistance = 300.f;
};