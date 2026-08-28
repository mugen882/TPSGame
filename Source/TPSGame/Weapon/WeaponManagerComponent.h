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

    /*
     이 함수는 OnRep 경로에서도 불리므로 순수 로컬 반영만 담당한다.
     Weapon visible 변경 포함
    */
    void SetCurrentWeapon(EWeaponType WeaponType);

    void ApplyWeaponType(EWeaponType WeaponType);

    // 어빌리티를 거치지 않는 서버 권위 경로
    void ServerEquipWeaponByClass(TSubclassOf<AWeaponBase> WeaponClass);

    const FGameplayTagContainer& GetWeaponFireTag() const { return WeaponFireTag; }

protected:
    UPROPERTY(EditAnywhere, Category="Weapon")
    TMap<EWeaponType, TSubclassOf<AWeaponBase>> WeaponClasses;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon")
    TMap<EWeaponType, AWeaponBase*> Weapons;

    /*
        CurrentWeapon / Weapons 는 복제하지 않는다.

        탄약이 어트리뷰트로 이관되면 무기 액터가 들고 있는 값은
        전부 BP CDO에서 오는 설정값뿐이라 모든 머신이 이미 동일하게 가지고 있다.
        따라서 무기 액터를 복제할 이유가 없고, 각 머신이 로컬로 스폰한 뒤
        '어떤 종류를 들고 있는가'만 1바이트로 복제하면 충분하다.
        (4인 코옵 기준 액터 채널 12개를 아낀다)
    */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
    TObjectPtr<AWeaponBase> CurrentWeapon = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponType)
    EWeaponType CurrentWeaponType = EWeaponType::None;

protected:
    UFUNCTION()
    void OnRep_CurrentWeaponType();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /*
        소유 캐릭터가 파괴될 때 무기 액터를 함께 정리한다.

        무기는 복제하지 않고 각 머신이 로컬로 스폰한다. 그래서 캐릭터가
        파괴돼도 자동으로 따라 사라지지 않는다. 어태치만 풀리고 월드에 남아
        리스폰이 반복될수록 바닥에 총이 쌓인다.

        복제 액터였다면 소유자 파괴와 함께 정리됐을 일을, 로컬 스폰을 선택한
        대가로 직접 처리하는 것이다.
    */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    ACommonCharacter* GetOwnerCharacter() const;

    // 순수 로컬 반영 — 메시 가시성, 발사 입력 태그, UI 브로드캐스트
    void ApplyWeaponTypeLocally();

private:
    FGameplayTagContainer WeaponFireTag;
};