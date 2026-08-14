#pragma once
#include "AI/BTTask_SwitchWeaponProfile.h"
#include "Character/EnemyCharacter.h"


EBTNodeResult::Type UBTTask_SwitchWeaponProfile::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    AEnemyCharacter* Enemy = AICon ? Cast<AEnemyCharacter>(AICon->GetPawn()) : nullptr;
    if (!Enemy) return EBTNodeResult::Failed;

    if (FMath::FRand() <= SwitchChance)
    {
        Enemy->SwitchToRandomProfile(true);
    }
    return EBTNodeResult::Succeeded;
}