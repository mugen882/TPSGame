#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "EnemyCombatComponent.generated.h"

class UEnemyCombatProfile;
class AEnemyCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TPSGAME_API UEnemyCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEnemyCombatComponent();

    void GrantFireAbilities();  // 프로파일 FireAbility 일괄 grant (BeginPlay 전 호출)
    void SwitchToRandomProfile(bool bAvoidRepeat = true);

    UEnemyCombatProfile* GetCurrentProfile() const { return CurrentProfile; }
    FGameplayAbilitySpecHandle GetFireAbilitySpecHandle() const { return FireAbilitySpecHandle; }
    float GetAttackRange() const;

protected:
    void ApplyCombatProfile(UEnemyCombatProfile* Profile);
    AEnemyCharacter* GetOwnerEnemy() const;

    UPROPERTY(EditAnywhere, Category="Combat")
    TArray<TObjectPtr<UEnemyCombatProfile>> CombatProfiles;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatProfile> CurrentProfile = nullptr;

    UPROPERTY()
    FGameplayAbilitySpecHandle FireAbilitySpecHandle;

    UPROPERTY(EditAnywhere, Category="Combat")
    float MinProfileSwitchInterval = 3.0f;

    UPROPERTY(EditAnywhere, Category="Combat")
    float DefaultAttackRange = 100.0f;   // 프로파일 적용 전 폴백

    float LastProfileSwitchTime = -1000.f;
};