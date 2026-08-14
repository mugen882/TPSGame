#include "WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Character/CommonCharacter.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"
#include "Character/PlayerCharacter.h"
#include "Common/TPSGameplayTags.h"
#include "Kismet/GameplayStatics.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWeaponBase::PlayImpactEffect(const FHitResult& Hit)
{
	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentAmmo = MaxAmmo;
}

bool AWeaponBase::CanFire() const
{
	return HasAmmo();
}

bool AWeaponBase::Fire(const FVector& AimPoint, AController* InstigatorController)
{
	if (!CanFire())
	{
		return false;
	}

	if (!FireInternal(AimPoint, InstigatorController))
	{
		// 실제 발사가 이뤄지지 않았으면 탄약/연출을 소모하지 않는다.
		return false;
	}

	ShowMuzzleFlash();
	PlayFireSound();

	if (!bInfiniteAmmo)
	{
		--CurrentAmmo;
	}

	return true;
}

FVector AWeaponBase::GetMuzzleLocation() const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		return WeaponMesh->GetSocketLocation(MuzzleSocketName);
	}
	return WeaponMesh ? WeaponMesh->GetComponentLocation() : GetActorLocation();
}

void AWeaponBase::SetWeaponVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
	// 부착된 자식 컴포넌트까지 확실히 전파
	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(bVisible, true);
	}
}

void AWeaponBase::ShowMuzzleFlash()
{
	if (!MuzzleFlash || !WeaponMesh)
	{
		return;
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		MuzzleFlash, WeaponMesh, MuzzleSocketName,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true);

	if (Comp)
	{
		Comp->SetWorldScale3D(FVector(MuzzleFlashScale));
	}
}

void AWeaponBase::PlayFireSound()
{
	if (FireSound == nullptr)
	{
		return;
	}

	FVector FireLocation = GetMuzzleLocation();

	UGameplayStatics::PlaySoundAtLocation(
		this,
		FireSound,
		FireLocation
	);
}

void AWeaponBase::Reload()
{
	CurrentAmmo = MaxAmmo;
}