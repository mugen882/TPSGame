#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_FireBase.generated.h"

class AWeaponBase;
class ACommonCharacter;

/*
    발사 베이스 클래스
    무기 발사 관련 처리
    쿨다운 적용
*/

UCLASS(Abstract)
class TPSGAME_API UGA_FireBase : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_FireBase();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // 쿨다운 GE를 무기 FireInterval(SetByCaller)로 적용
    virtual void ApplyCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;

    virtual const FGameplayTagContainer* GetCooldownTags() const override; // CheckCooldown에서 이 태그 여부로 쿨다운 체크

    // 발사 타이밍 분기: 플레이어=즉발, 그 외(적)=노티파이 대기
    virtual bool ShouldFireImmediately(const FGameplayAbilityActorInfo* ActorInfo) const;

    // 활성 무기 한 발 발사
    void FireOnce(const FGameplayAbilityActorInfo* ActorInfo) const;
    ACommonCharacter* GetOwningCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;

private:
    // 발사 몽타주 재생.
    // bEndAbilityOnMontageEnd
    // true(적): 몽타주 종료가 어빌리티를 끝낸다(노티파이가 발사).
    // false(플레이어): 연출용 fire-and-forget 재생만 하고 어빌리티는 즉시 종료된다.
    // 반환값: 재생할 몽타주가 있었으면 true.
    bool PlayFireMontage(const FGameplayAbilityActorInfo* ActorInfo, bool bEndAbilityOnMontageEnd);

    UFUNCTION()
    void OnFireMontageEnded();

private:
    FGameplayTagContainer CooldownTagContainer; // { Cooldown.Fire }
};