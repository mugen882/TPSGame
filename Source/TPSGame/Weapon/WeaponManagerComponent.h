#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/TPSGameTypes.h"
#include "GameplayTagContainer.h"
#include "WeaponManagerComponent.generated.h"

class AWeaponBase;
class ACommonCharacter;
class UAnimMontage;

/*
    플레이어용 무기 관리자 클래스
    소유한 모든 무기의 클래스를 가지고 있음
    현재 장착중인 무기의 정보를 얻을 수 있음
*/
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TPSGAME_API UWeaponManagerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UWeaponManagerComponent();

    void SpawnWeapons();

    AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }
    EWeaponType  GetCurrentWeaponType() const { return CurrentWeaponType; }
    const TMap<EWeaponType, AWeaponBase*>& GetWeapons() const { return Weapons; }
    AWeaponBase* GetWeapon(EWeaponType WeaponType) const { return Weapons.FindRef(WeaponType); }

    void SetCurrentWeapon(EWeaponType WeaponType);  // visible 토글 포함

    void EquipWeapon(EWeaponType WeaponType);
    void DoEquipWeapon(EWeaponType WeaponType);
    void EquipWeaponByClass(TSubclassOf<AWeaponBase> WeaponClass);
    void OnWeaponSwapNotify();
    const FGameplayTagContainer& GetWeaponFireTag() const { return WeaponFireTag; }

protected:
    UPROPERTY(EditAnywhere, Category="Weapon")
    TMap<EWeaponType, TSubclassOf<AWeaponBase>> WeaponClasses;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon")
    TMap<EWeaponType, AWeaponBase*> Weapons;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
    TObjectPtr<AWeaponBase> CurrentWeapon = nullptr;

    EWeaponType CurrentWeaponType = EWeaponType::None;

private:
    ACommonCharacter* GetOwnerCharacter() const;
    void OnSwapMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
    UPROPERTY(EditAnywhere, Category="Weapon")
    TObjectPtr<UAnimMontage> WeaponSwapMontage;

    FGameplayTagContainer WeaponFireTag;
    EWeaponType PendingWeaponType = EWeaponType::None;
};