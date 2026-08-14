#include "WeaponMachineGun.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/CommonCharacter.h"
#include "Common/TPSGameDefine.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"
#include "Character/PlayerCharacter.h"

AWeaponMachineGun::AWeaponMachineGun()
{
	FireInterval = 0.15f;
	Damage = 12.f;
}

void AWeaponMachineGun::BeginPlay()
{
	Super::BeginPlay();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player)
	{
		Player->OnAmmoReloaded.AddDynamic(this, &AWeaponMachineGun::AmmoReload);
	}
}

void AWeaponMachineGun::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player)
	{
		Player->OnAmmoReloaded.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AWeaponMachineGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastShot += DeltaTime;
	// 잠깐 안 쏘면 회복 시작
	if (TimeSinceLastShot > 0.1f && CurrentSpread > 0.f)
	{
		CurrentSpread = FMath::Max(CurrentSpread - SpreadRecoveryRate * DeltaTime, 0.f);
	}
}

void AWeaponMachineGun::FireInternal(const FVector& AimPoint, AController* InstigatorController)
{
	const FVector Start = GetMuzzleLocation();
	FVector Dir = (AimPoint - Start).GetSafeNormal();

	// 누적 spread만큼 방향을 랜덤하게 틀기 * 난이도 spread
	const float TotalSpread = (BaseSpreadAngle + CurrentSpread) * GetSpreadMultiplier();
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(TotalSpread));

	// spread 누적
	CurrentSpread = FMath::Min(CurrentSpread + SpreadPerShot, MaxSpreadAngle);
	TimeSinceLastShot = 0.f;

	const FVector End = Start + Dir * FireRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Params))
	{
		return;
	}

	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor) return;
	if (Cast<ACommonCharacter>(HitActor))
	{
		ACommonCharacter::ApplyDamageEffect(HitActor, DamageEffectClass, Damage, GetOwner());
		OnHitConfirmed.Broadcast();
	}
}

void AWeaponMachineGun::AmmoReload()
{
	TimeSinceLastShot = 0.f;
	CurrentSpread = 0.f;
}
