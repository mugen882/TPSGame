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

bool AWeaponRifle::FireInternal(const FVector& AimPoint, AController* InstigatorController)
{
	const FVector Start = GetMuzzleLocation();
	const FVector Dir = (AimPoint - Start).GetSafeNormal();
	const FVector End = Start + Dir * FireRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	// 총구에서 크로스헤어 방향으로 → 조준점 찾기
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Params))
	{
		return true;   // 빗나가도 발사 자체는 이뤄졌으므로 탄약 소모
	}

	AActor* HitActor = Hit.GetActor();
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

void AWeaponRifle::FireCosmeticInternal(const FVector& AimPoint)
{
	/*
		연출 전용 트레이스. 데미지를 적용하지 않는다.

		서버의 권위 트레이스와 별개로 각 머신이 자기 화면의 탄착 이펙트를 만든다.
		클라이언트가 서버 왕복을 기다리지 않고 즉시 피드백을 받게 하기 위함이다.
	*/
	const FVector Start = GetMuzzleLocation();
	const FVector Dir = (AimPoint - Start).GetSafeNormal();
	const FVector End = Start + Dir * FireRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Params))
	{
		PlayImpactEffect(Hit);
	}
}
