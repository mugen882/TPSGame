#include "EnemyCombatComponent.h"
#include "Character/EnemyCharacter.h"
#include "AI/EnemyCombatProfile.h"
#include "AbilitySystemComponent.h"
#include "Weapon/WeaponManagerComponent.h"
#include "Subsystem/DifficultySubsystem.h"
#include "AbilitySystem/TPSAttributeSet.h"

UEnemyCombatComponent::UEnemyCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

AEnemyCharacter* UEnemyCombatComponent::GetOwnerEnemy() const
{
    return Cast<AEnemyCharacter>(GetOwner());
}

void UEnemyCombatComponent::GrantFireAbilities()
{
    AEnemyCharacter* Owner = GetOwnerEnemy();
    if (!Owner) return;
    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
    if (!ASC) return;

    TSet<TSubclassOf<UGameplayAbility>> Granted;
    for (UEnemyCombatProfile* P : CombatProfiles)
    {
        if (P && P->FireAbility && !Granted.Contains(P->FireAbility))
        {
            ASC->GiveAbility(FGameplayAbilitySpec(P->FireAbility, 1, INDEX_NONE, Owner));
            Granted.Add(P->FireAbility);
        }
    }
}

float UEnemyCombatComponent::GetAttackRange() const
{
    return CurrentProfile ? CurrentProfile->AttackRange : DefaultAttackRange;
}

void UEnemyCombatComponent::ApplyCombatProfile(UEnemyCombatProfile* Profile)
{
    if (!Profile) return;
    CurrentProfile = Profile;

    AEnemyCharacter* Owner = GetOwnerEnemy();
    if (!Owner) return;

    float DamageMul = 1.f;
    float SpreadMul = 1.f;
    if (const UGameInstance* GI = Owner->GetGameInstance())
    {
        if (const UDifficultySubsystem* Diff = GI->GetSubsystem<UDifficultySubsystem>())
        {
            DamageMul = Diff->EnemyDamageMul;
            SpreadMul = Diff->EnemySpreadMul;
        }   
    }   

    // 데미지는 무기에 종속 → 무기 장착 후 스케일 적용
    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
    if (ASC && Profile->FireAbility)
    {
        if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Profile->FireAbility))
            FireAbilitySpecHandle = Spec->Handle;
    }
    if (Profile->WeaponClass)
    {
        if (UWeaponManagerComponent* WM = Owner->GetWeaponManager())
        {
            WM->EquipWeaponByClass(Profile->WeaponClass);
            if (AWeaponBase* Wpn = WM->GetCurrentWeapon())
            {
                Wpn->SetDamage(Wpn->GetBaseDamage() * DamageMul);
                Wpn->SetSpreadMultiplier(SpreadMul);
            }
        }
    }
}

void UEnemyCombatComponent::SwitchToRandomProfile(bool bAvoidRepeat)
{
    const int32 Num = CombatProfiles.Num();
    if (Num == 0) return;

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastProfileSwitchTime < MinProfileSwitchInterval) return;

    if (Num == 1)
    {
        ApplyCombatProfile(CombatProfiles[0]);
        LastProfileSwitchTime = Now;
        return;
    }

    int32 Index = FMath::RandRange(0, Num - 1);
    if (bAvoidRepeat && CurrentProfile && CombatProfiles[Index] == CurrentProfile)
        Index = (Index + 1) % Num;

    ApplyCombatProfile(CombatProfiles[Index]);
    LastProfileSwitchTime = Now;
}