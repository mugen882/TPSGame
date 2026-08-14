#include "UI/HealthBarWidget.h"
#include "Character/PlayerCharacter.h"
#include "Components/ProgressBar.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#include "AbilitySystem/TPSAttributeSet.h"

void UHealthBarWidget::SetAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	// 재바인딩 시 중복되지 않도록 이전 구독부터 정리
	Unbind();

	ASC = InASC;

	// 속성 변경 델리게이트에 바인딩
	HealthChangedHandle = InASC
		->GetGameplayAttributeValueChangeDelegate(UTPSAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UHealthBarWidget::HandleHealthChanged);

	MaxHealthChangedHandle = InASC
		->GetGameplayAttributeValueChangeDelegate(UTPSAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UHealthBarWidget::HandleMaxHealthChanged);

	// 바인딩 직후 현재 값으로 1회 갱신
	CachedHealth = InASC->GetNumericAttribute(UTPSAttributeSet::GetHealthAttribute());
	CachedMaxHealth = InASC->GetNumericAttribute(UTPSAttributeSet::GetMaxHealthAttribute());
	UpdateBar();
	UpdateText();
}

void UHealthBarWidget::SetOwningCharacter(ACommonCharacter* InPlayer)
{
	if (!InPlayer)
	{
		return;
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InPlayer);
	UAbilitySystemComponent* ASComp = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

	SetAbilitySystem(ASComp);
}

void UHealthBarWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	UpdateBar();
	UpdateText();
}

void UHealthBarWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	UpdateBar();
	UpdateText();
}

void UHealthBarWidget::UpdateBar()
{
	if (HealthBarFill)
	{
		const float Percent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
		HealthBarFill->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}

void UHealthBarWidget::UpdateText()
{
	if (HealthText)
	{
		FString HealthString = FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(CachedHealth), FMath::RoundToInt(CachedMaxHealth));
		HealthText->SetText(FText::FromString(HealthString));
	}
}

void UHealthBarWidget::Unbind()
{
	if (UAbilitySystemComponent* ASComp = ASC.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASComp->GetGameplayAttributeValueChangeDelegate(UTPSAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASComp->GetGameplayAttributeValueChangeDelegate(UTPSAttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ASC.Reset();
}

void UHealthBarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}
