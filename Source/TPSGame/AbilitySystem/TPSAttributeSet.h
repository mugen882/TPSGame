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

public:
	// 현재 체력
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, Health)

	// 최대 체력
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, MaxHealth)

	// 데미지 어트리뷰트
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UTPSAttributeSet, Damage)

private:
	// 클램프 등 사전 처리
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// MaxHealth 변경 시 현재 Health 보정
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	// 데미지 적용 후처리 (Health 차감)
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};