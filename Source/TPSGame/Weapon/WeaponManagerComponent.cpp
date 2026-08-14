#include "WeaponManagerComponent.h"
#include "Weapon/WeaponBase.h"
#include "GameFramework/Character.h"
#include "Character/CommonCharacter.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Common/TPSLog.h"

UWeaponManagerComponent::UWeaponManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponManagerComponent::SpawnWeapons()
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = OwnerChar;
    SpawnParams.Instigator = OwnerChar;

    const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);

    // 소유한 모든 무기 스폰
    for (const TPair<EWeaponType, TSubclassOf<AWeaponBase>>& Pair : WeaponClasses)
    {
        AWeaponBase* Weapon = GetWorld()->SpawnActor<AWeaponBase>(
            Pair.Value, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!Weapon) continue;

        Weapon->AttachToComponent(OwnerChar->GetMesh(), AttachRules, FName("WeaponSocket"));
        Weapon->SetWeaponVisible(false);
        Weapons.Add(Pair.Key, Weapon);
        Weapon->SetOwner(OwnerChar);
    }
}

void UWeaponManagerComponent::SetCurrentWeapon(EWeaponType WeaponType)
{
    if (!Weapons.Find(WeaponType)) return;

    ACommonCharacter* OwnerChar = Cast<ACommonCharacter>(GetOwner());
    if (!OwnerChar) return;

    if (CurrentWeapon)
    {
        CurrentWeapon->SetWeaponVisible(false);
    }

    CurrentWeaponType = WeaponType;
    CurrentWeapon = Weapons.FindRef(WeaponType);
    if (CurrentWeapon)
    {
        CurrentWeapon->SetWeaponVisible(true);
    }
}

ACommonCharacter* UWeaponManagerComponent::GetOwnerCharacter() const
{
    return Cast<ACommonCharacter>(GetOwner());
}

void UWeaponManagerComponent::EquipWeapon(EWeaponType InWeaponType)
{
    if (!Weapons.Find(InWeaponType)) return;
    if (InWeaponType == EWeaponType::None || (int32)EWeaponType::MAX <= (int32)InWeaponType) return;

    ACommonCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    if (Owner->IsReloading() || Owner->IsSwapping()) return;
    if (CurrentWeaponType == InWeaponType) return;

    PendingWeaponType = InWeaponType;

    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
    if (ASC && WeaponFireTag.IsValid() && !WeaponFireTag.IsEmpty())
        ASC->CancelAbilities(&WeaponFireTag);
    if (ASC)
        ASC->AddLooseGameplayTag(TAG_State_Swapping);

    if (WeaponSwapMontage)
    {
        if (UAnimInstance* AnimInst = Owner->GetMesh()->GetAnimInstance())
        {
            AnimInst->Montage_Play(WeaponSwapMontage);
            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &UWeaponManagerComponent::OnSwapMontageEnded);
            AnimInst->Montage_SetEndDelegate(EndDelegate, WeaponSwapMontage);
        }
    }
    else
    {
        DoEquipWeapon(PendingWeaponType);
        if (ASC)
            ASC->RemoveLooseGameplayTag(TAG_State_Swapping);
        PendingWeaponType = EWeaponType::None;
    }
}

void UWeaponManagerComponent::DoEquipWeapon(EWeaponType WeaponType)
{
    SetCurrentWeapon(WeaponType);

    WeaponFireTag.Reset();
    switch (CurrentWeaponType)
    {
    case EWeaponType::Rifle:            WeaponFireTag.AddTag(TAG_Input_Fire_Rifle);           break;
    case EWeaponType::RocketLauncher:   WeaponFireTag.AddTag(TAG_Input_Fire_RocketLauncher);  break;
    case EWeaponType::MachineGun:       WeaponFireTag.AddTag(TAG_Input_Fire_MachineGun);      break;
    default: return;
    }

    if (ACommonCharacter* Owner = GetOwnerCharacter())
        Owner->OnWeaponChanged.Broadcast(CurrentWeaponType);

    PendingWeaponType = EWeaponType::None;
}

void UWeaponManagerComponent::EquipWeaponByClass(TSubclassOf<AWeaponBase> WeaponClass)
{
    if (!WeaponClass) return;
    if (CurrentWeapon && CurrentWeapon->IsA(WeaponClass)) return;

    const TMap<EWeaponType, AWeaponBase*>& WeaponsMap = GetWeapons();
    for (const TPair<EWeaponType, AWeaponBase*>& Pair : WeaponsMap)
    {
        if (Pair.Value && Pair.Value->IsA(WeaponClass))
            {
                EquipWeapon(Pair.Key);
                return;
	    }
    }
    UE_LOG(TPSLog, Warning, TEXT("[WeaponManager] EquipWeaponByClass: %s 인스턴스가 Weapons에 없음"), *GetNameSafe(WeaponClass));
}

void UWeaponManagerComponent::OnWeaponSwapNotify()
{
    if ((int32)PendingWeaponType > 0)
        DoEquipWeapon(PendingWeaponType);
}

void UWeaponManagerComponent::OnSwapMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    ACommonCharacter* Owner = GetOwnerCharacter();
    if (!Owner)
    {
        PendingWeaponType = EWeaponType::None;
        return;
    }

    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();

    // 죽은 상태면 스왑 마무리 안 함
    if (ASC && ASC->HasMatchingGameplayTag(TAG_State_Dead))
    {
        PendingWeaponType = EWeaponType::None;
        return;
    }

    if ((int32)PendingWeaponType > (int32)EWeaponType::None)
        DoEquipWeapon(PendingWeaponType);

    if (ASC)
        ASC->RemoveLooseGameplayTag(TAG_State_Swapping);

    PendingWeaponType = EWeaponType::None;
}