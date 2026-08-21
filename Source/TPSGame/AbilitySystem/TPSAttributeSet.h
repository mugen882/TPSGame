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

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

private:
	// 클램프 등 사전 처리
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// 데미지 적용 후처리 (Health 차감)
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
