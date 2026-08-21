#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TPSAttributeSet.generated.h"

// 어트리뷰트 접근자(Getter/Setter/Initter) 매크로
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class TPSGAME_API UTPSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTPSAttributeSet();

	void InitializeAttributes(float NewHealth);

	// 리플리케이션 대상 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// 현재 체력
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, Health)

	// 최대 체력
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, MaxHealth)

	/*
		데미지 — 메타(meta) 어트리뷰트

		캐릭터의 상태가 아니라, GE에서 PostGameplayEffectExecute로 피해량을 넘기기 위한
		1회성 운반 수단이다. 값을 읽은 즉시 0으로 리셋되며 평상시 값은 항상 0이다.

		리플리케이션을 붙이지 않는 이유:
		0이 아닌 값은 단일 콜스택 안에서만 존재하므로, NetUpdate 시점에는 이미 0으로
		돌아와 있다. 즉 복제를 걸어도 클라이언트는 영원히 0만 받는다.

		클라이언트에 피해량을 알려야 한다면 GameplayCue의 RawMagnitude를 쓴다.
		순간 이벤트는 상태 채널이 아니라 이벤트 채널로 보내는 것이 맞다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, Damage)

	/*
		탄약 — 무기 종류별 독립

		무기 액터의 int32에서 어트리뷰트로 옮긴 이유:
		GAS의 Cost 파이프라인(CheckCost/ApplyCost)을 그대로 쓰기 위해서다.
		그러면 CommitAbility() 한 줄이
		  - 탄약 부족 시 어빌리티 활성화 차단
		  - 예측 차감(클라 즉시 반영) 및 서버 거부 시 롤백
		을 모두 처리한다. 무기 액터의 평범한 멤버로는 어느 쪽도 불가능하다.

		MaxAmmo는 어트리뷰트로 만들지 않는다. 무기 BP CDO의 설정값이라
		모든 머신이 이미 동일하게 알고 있어 복제할 이유가 없다.
	*/
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RifleAmmo, Category = "Ammo")
	FGameplayAttributeData RifleAmmo;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, RifleAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MachineGunAmmo, Category = "Ammo")
	FGameplayAttributeData MachineGunAmmo;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, MachineGunAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RocketAmmo, Category = "Ammo")
	FGameplayAttributeData RocketAmmo;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, RocketAmmo)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_RifleAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MachineGunAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RocketAmmo(const FGameplayAttributeData& OldValue);

private:
	// 클램프 등 사전 처리
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// 데미지 적용 후처리 (Health 차감)
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
