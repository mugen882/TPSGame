#include "Weapon/WeaponRocketLauncher.h"
#include "Projectile.h"
#include "AbilitySystem/TPSAttributeSet.h"

AWeaponRocketLauncher::AWeaponRocketLauncher()
{
	FireInterval = 1.5f;
	Damage = 50.f;
}

void AWeaponRocketLauncher::BeginPlay()
{
	Super::BeginPlay();
	
}

bool AWeaponRocketLauncher::FireInternal(const FVector& AimPoint, AController* InstigatorController, FHitResult& OutHit)
{
	// 투사체 무기는 즉시 명중 지점이 없다. OutHit을 채우지 않는다.
	if (!ProjectileClass)
	{
		return false;   // 투사체 클래스 미설정 → 발사 실패(탄약 소모 없음)
	}

	const FVector  MuzzleLoc = GetMuzzleLocation();
	const FRotator SpawnRot = (AimPoint - MuzzleLoc).Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProjectile* Proj = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, MuzzleLoc, SpawnRot, SpawnParams);
	if (Proj)
	{
		Proj->SetDamage(Damage);                       // 무기 데미지 전달
		Proj->SetDamageEffectClass(DamageEffectClass); // GE 클래스 전달
	}

	return Proj != nullptr;
}

FGameplayAttribute AWeaponRocketLauncher::GetAmmoAttribute() const
{
	return UTPSAttributeSet::GetRocketAmmoAttribute();
}
