#include "WeaponRifle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/EnemyCharacter.h"
#include "Common/TPSGameDefine.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"

AWeaponRifle::AWeaponRifle()
{
	FireInterval = 0.3f;
	Damage = 5.f;
}

void AWeaponRifle::FireInternal(const FVector& AimPoint, AController* InstigatorController)
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
		return;
	}

	PlayImpactEffect(Hit);

	AActor* HitActor = Hit.GetActor();
	if (!HitActor) return;

	if (Cast<ACommonCharacter>(HitActor))
	{
		ACommonCharacter::ApplyDamageEffect(HitActor, DamageEffectClass, Damage, GetOwner());
		OnHitConfirmed.Broadcast();
	}
}