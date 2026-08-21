#include "WeaponManagerComponent.h"
#include "Weapon/WeaponBase.h"
#include "GameFramework/Character.h"
#include "Character/CommonCharacter.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Common/TPSLog.h"
#include "Net/UnrealNetwork.h"

UWeaponManagerComponent::UWeaponManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // CurrentWeaponType 복제를 위해 컴포넌트 자체의 복제를 켠다.
    SetIsReplicatedByDefault(true);
}

void UWeaponManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    /*
        REPNOTIFY_Always인 이유

        소유 클라는 예측을 위해 CurrentWeaponType에 값을 미리 써둔다.
        기본값(OnChanged)이면 서버가 같은 값을 보냈을 때 OnRep이 생략되어
        예측이 틀렸을 경우 되돌릴 기회가 사라진다. Always면 서버 값이 도착할 때마다
        반드시 로컬 상태를 서버 기준으로 다시 맞춘다.
    */
    DOREPLIFETIME_CONDITION_NOTIFY(UWeaponManagerComponent, CurrentWeaponType, COND_None, REPNOTIFY_Always);
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

    /*
        복제가 스폰보다 먼저 도착했을 수 있다.

        OnRep_CurrentWeaponType이 Weapons가 비어 있는 상태에서 불리면 조용히 무시되고,
        CurrentWeaponType은 다시 바뀌지 않으므로 OnRep이 재발생하지 않는다.
        그래서 스폰이 끝난 시점에 이미 받아둔 값을 한 번 더 반영한다.
        (양쪽 순서 어느 쪽이든 마지막에 실행된 쪽이 상태를 확정한다)
    */
    if (CurrentWeaponType != EWeaponType::None)
    {
        ApplyWeaponTypeLocally();
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

void UWeaponManagerComponent::ApplyWeaponType(EWeaponType WeaponType)
{
    if (WeaponType == EWeaponType::None || (int32)EWeaponType::MAX <= (int32)WeaponType) return;
    if (!Weapons.Find(WeaponType)) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    UE_LOG(TPSLog, Warning, TEXT("[%.2f] Apply  | %s | %s | Owner=%s | %s -> %s"),
        TPSNetDebug::GetSyncedTime(Owner),
        TPSNetDebug::NetModeToString(Owner->GetNetMode()),
        TPSNetDebug::NetRoleToString(Owner->GetLocalRole()),
        *Owner->GetName(),
        *UEnum::GetValueAsString(CurrentWeaponType),
        *UEnum::GetValueAsString(WeaponType));

    /*
        무기 종류 확정. GA_SwapWeapon이 서버와 소유 클라 양쪽에서 호출한다.
        서버      : 복제 변수에 기록 -> 모든 클라로 전파
        소유 클라 : 같은 값을 로컬로 미리 기록(예측). 서버 값이 도착하면
                    REPNOTIFY_Always 덕분에 OnRep이 반드시 실행되어 확정된다.
        그 외(시뮬레이티드 프록시) : 호출하지 않는다. OnRep_CurrentWeaponType이 처리한다.
        주의 : 서버는 OnRep을 받지 않으므로 ApplyWeaponTypeLocally를 직접 호출해야 한다.
    */
    CurrentWeaponType = WeaponType;
    ApplyWeaponTypeLocally();
}

void UWeaponManagerComponent::OnRep_CurrentWeaponType()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    UE_LOG(TPSLog, Warning, TEXT("[%.2f] OnRep  | %s | %s | Owner=%s | Weapon=%s"),
        TPSNetDebug::GetSyncedTime(Owner),
        TPSNetDebug::NetModeToString(GetOwner()->GetNetMode()),
        TPSNetDebug::NetRoleToString(GetOwner()->GetLocalRole()),
        *GetOwner()->GetName(),
        *UEnum::GetValueAsString(CurrentWeaponType));

    // 서버가 확정한 값으로 로컬 상태를 맞춘다.
    // 예측이 맞았다면 같은 값을 다시 적용하는 것이므로 부작용이 없다.
    ApplyWeaponTypeLocally();
}

void UWeaponManagerComponent::ApplyWeaponTypeLocally()
{
	EWeaponType WeaponType = CurrentWeaponType;
    if (WeaponType == EWeaponType::None || (int32)EWeaponType::MAX <= (int32)WeaponType) return;

    SetCurrentWeapon(WeaponType);

    WeaponFireTag.Reset();
    switch (WeaponType)
    {
    case EWeaponType::Rifle:            WeaponFireTag.AddTag(TAG_Input_Fire_Rifle);           break;
    case EWeaponType::RocketLauncher:   WeaponFireTag.AddTag(TAG_Input_Fire_RocketLauncher);  break;
    case EWeaponType::MachineGun:       WeaponFireTag.AddTag(TAG_Input_Fire_MachineGun);      break;
    default: return;
    }

    if (ACommonCharacter* Owner = GetOwnerCharacter())
        Owner->OnWeaponChanged.Broadcast(WeaponType);
}

void UWeaponManagerComponent::ServerEquipWeaponByClass(TSubclassOf<AWeaponBase> WeaponClass)
{
    if (!WeaponClass) return;
    if (CurrentWeapon && CurrentWeapon->IsA(WeaponClass)) return;

    for (const TPair<EWeaponType, AWeaponBase*>& Pair : Weapons)
    {
        if (Pair.Value && Pair.Value->IsA(WeaponClass))
        {
            ApplyWeaponType(Pair.Key);
            return;
        }
    }
    UE_LOG(TPSLog, Warning, TEXT("[WeaponManager] ServerEquipWeaponByClass: %s 인스턴스가 Weapons에 없음"), *GetNameSafe(WeaponClass));
}
