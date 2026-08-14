#include "Weapon/WeaponRocketLauncher.h"
#include "Projectile.h"

AWeaponRocketLauncher::AWeaponRocketLauncher()
{
	CurrentAmmo = MaxAmmo = 2;
	FireInterval = 1.5f;
	Damage = 50.f;
}

void AWeaponRocketLauncher::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWeaponRocketLauncher::FireInternal(const FVector& AimPoint, AController* InstigatorController)
{
	if (!ProjectileClass)
	{
		return;
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
}