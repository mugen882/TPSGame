#include "Projectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Character/CommonCharacter.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"

AProjectile::AProjectile()
{
    PrimaryActorTick.bCanEverTick = false;

    // 구체 콜리전을 루트로
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(6.f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionComp->SetCollisionObjectType(ECC_GameTraceChannel1); // Weapon 채널
    CollisionComp->SetNotifyRigidBodyCollision(true);             // Simulation Generates Hit Events
    CollisionComp->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
    RootComponent = CollisionComp;

    // 시각 메시 (콜리전은 끔)
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 발사체 이동
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 8000.f;
    ProjectileMovement->MaxSpeed = 8000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.f; // 직선 탄도. 곡사면 0.1~0.3

    InitialLifeSpan = 3.f; // 3초 뒤 자동 소멸
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse,
    const FHitResult& Hit)
{
    HandleImpact(OtherActor, Hit);
    Destroy();
}

void AProjectile::HandleImpact(AActor* HitActor, const FHitResult& Hit)
{
    // 기본 투사체: 직접 맞은 대상 1명
    ACommonCharacter::ApplyDamageEffect(HitActor, DamageEffectClass, Damage, GetInstigator());

    if (ImpactVFX)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, Hit.ImpactPoint, Hit.ImpactNormal.Rotation(), FVector(3.f));
    if (ImpactSound)
        UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
}