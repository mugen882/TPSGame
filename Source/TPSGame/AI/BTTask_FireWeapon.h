#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FireWeapon.generated.h"

// 노드 인스턴스별 메모리 (버스트 잔여시간 추적)
struct FBTFireWeaponMemory
{
    float RemainingBurst = 0.f;
    bool  bBursting = false;
    bool  bBurstCoolingDown = false;
    float RemainingBurstCooldown = 1.f;
};

/*
    총 발사하는 BT태스크
    단발 / 연사 처리
    연사시 쿨다운 처리
*/

UCLASS()
class TPSGAME_API UBTTask_FireWeapon : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_FireWeapon();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTFireWeaponMemory); }
};