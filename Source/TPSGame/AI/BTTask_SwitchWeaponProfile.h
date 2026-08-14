#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Character/EnemyCharacter.h"
#include "AIController.h"
#include "BTTask_SwitchWeaponProfile.generated.h"

/*
	무기 프로필을 교체하는 BT태스크
*/
UCLASS()
class TPSGAME_API UBTTask_SwitchWeaponProfile : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_SwitchWeaponProfile() { NodeName = TEXT("Switch Weapon Profile"); }

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*) override;

    // 한 사이클당 무기를 실제로 교체할 확률
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0"))
    float SwitchChance = 0.35f;
};