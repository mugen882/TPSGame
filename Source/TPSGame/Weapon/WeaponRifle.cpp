#include "WeaponRifle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/EnemyCharacter.h"
#include "Common/TPSGameDefine.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/TPSAttributeSet.h"

AWeaponRifle::AWeaponRifle()
{
	FireInterval = 0.3f;
	Damage = 5.f;
}

bool AWeaponRifle::FireInternal(const FVector& AimPoint, AController* InstigatorController, FHitResult& OutHit)
{
	const FVector Start = GetMuzzleLocation();
	const FVector Dir = (AimPoint - Start).GetSafeNormal();
	const FVector End = Start + Dir * FireRange;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	// 총구에서 크로스헤어 방향으로 → 조준점 찾기
	if (!GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Weapon, Params))
	{
		return true;   // 빗나가도 발사 자체는 이뤄졌으므로 탄약 소모
	}

	AActor* HitActor = OutHit.GetActor();
	if (!HitActor) return true;

	if (Cast<ACommonCharacter>(HitActor))
	{
		// 가해자는 발사한 캐릭터(Instigator)로 통일
		ACommonCharacter::ApplyDamageEffect(HitActor, DamageEffectClass, Damage, GetInstigator());
		OnHitConfirmed.Broadcast();
	}

	return true;
}

FGameplayAttribute AWeaponRifle::GetAmmoAttribute() const
{
	return UTPSAttributeSet::GetRifleAmmoAttribute();
}

bool AWeaponRifle::TracePredictedImpactInternal(const FVector& AimPoint, FHitResult& OutHit) const
{
	/*
		판정 없는 예측 트레이스. 데미지를 적용하지 않는다.
		결과는 호출부(어빌리티)가 GameplayCue 파라미터로 실어 보낸다.
	*/
	const FVector Start = GetMuzzleLocation();
	const FVector Dir = (AimPoint - Start).GetSafeNormal();
	const FVector End = Start + Dir * FireRange;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Weapon, Params);
}
