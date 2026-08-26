#include "AbilitySystem/TPSAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Character/CommonCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Common/TPSLog.h"

UTPSAttributeSet::UTPSAttributeSet()
{
	
}

void UTPSAttributeSet::InitializeAttributes(float NewHealth)
{
	InitHealth(NewHealth);
	InitMaxHealth(NewHealth);
}

void UTPSAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*
		REPNOTIFY_Always를 쓰는 이유

		기본값(REPNOTIFY_OnChanged)은 "복제된 값이 클라 로컬 값과 다를 때만" OnRep을 호출한다.
		그런데 GAS는 클라가 어빌리티를 예측 실행하면서 어트리뷰트를 미리 바꿔놓는 경우가 많다.
		이때 서버 값이 도착해도 이미 같은 값이라 OnRep이 생략되고,
		결과적으로 ASC의 어트리뷰트 변경 델리게이트가 발화하지 않아 UI가 멈춘다.

		Always로 두면 예측이 맞았든 롤백됐든 항상 갱신 경로를 타므로 안전하다.
	*/
	DOREPLIFETIME_CONDITION_NOTIFY(UTPSAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTPSAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);

	/*
		탄약은 COND_OwnerOnly.

		Health와 달리 남의 탄약은 누구도 볼 필요가 없다.
	*/
	DOREPLIFETIME_CONDITION_NOTIFY(UTPSAttributeSet, RifleAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTPSAttributeSet, MachineGunAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTPSAttributeSet, RocketAmmo, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTPSAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	// ASC에 변경을 통지해 GetGameplayAttributeValueChangeDelegate 구독자들을 깨운다.
	// (ACommonCharacter::HandleHealthChanged, UHealthBarWidget 등)
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTPSAttributeSet, Health, OldHealth);
}

void UTPSAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTPSAttributeSet, MaxHealth, OldMaxHealth);
}

void UTPSAttributeSet::OnRep_RifleAmmo(const FGameplayAttributeData& OldValue)
{
	UE_LOG(TPSLog, Warning, TEXT("OnRep_RifleAmmo Old(B=%.0f C=%.0f) -> New(B=%.0f C=%.0f)"),
		OldValue.GetBaseValue(), OldValue.GetCurrentValue(),
		RifleAmmo.GetBaseValue(), RifleAmmo.GetCurrentValue());

	GAMEPLAYATTRIBUTE_REPNOTIFY(UTPSAttributeSet, RifleAmmo, OldValue);
}

void UTPSAttributeSet::OnRep_MachineGunAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTPSAttributeSet, MachineGunAmmo, OldValue);
}

void UTPSAttributeSet::OnRep_RocketAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTPSAttributeSet, RocketAmmo, OldValue);
}

void UTPSAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}

	/*
		탄약 하한만 여기서 막는다.
		상한(MaxAmmo)은 무기 BP의 설정값이라 AttributeSet이 알 수 없다.
		AttributeSet이 WeaponManager를 들여다보면 계층이 뒤집히므로,
		상한 클램프는 값을 아는 쪽(재장전 시점)에서 처리한다.
	*/
	else if (Attribute == GetRifleAmmoAttribute()
		|| Attribute == GetMachineGunAmmoAttribute()
		|| Attribute == GetRocketAmmoAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UTPSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 이 함수는 GE가 실제로 "실행"되는 머신에서만 호출된다.
	// Instant GE는 서버에서만 실행되므로 사실상 서버 전용 경로다.

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
