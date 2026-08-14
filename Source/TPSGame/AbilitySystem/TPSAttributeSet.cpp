#include "AbilitySystem/TPSAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Character/CommonCharacter.h"

UTPSAttributeSet::UTPSAttributeSet()
{
	
}

void UTPSAttributeSet::InitializeAttributes(float NewHealth)
{
	InitHealth(NewHealth);
	InitMaxHealth(NewHealth);
}

void UTPSAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UTPSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 데미지 메타 어트리뷰트로 들어온 값을 Health에서 차감
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.0f);   // 메타 어트리뷰트는 소비 후 초기화

		if (LocalDamage > 0.0f)
		{
			AActor* Instigator = Data.EffectSpec.GetContext().GetInstigator();

			AActor* TargetActor = nullptr;
			if (Data.Target.AbilityActorInfo.IsValid())
			{
				TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
			}

			if (ACommonCharacter* TargetChar = Cast<ACommonCharacter>(TargetActor))
			{
				TargetChar->NotifyDamageFrom(Instigator);   // 캐릭터에 알림
			}

			const float NewHealth = GetHealth() - LocalDamage;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
		}
	}
}