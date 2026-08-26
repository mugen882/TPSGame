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

    // 클라이언트에서도 로켓이 보이도록 복제한다.
    bReplicates = true;
    SetReplicateMovement(true);
}

void AProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 발사자 본인과는 충돌하지 않게 한다.
    if (AActor* Inst = GetInstigator())
    {
        CollisionComp->IgnoreActorWhenMoving(Inst, true);
    }
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse,
    const FHitResult& Hit)
{
    /*
        판정과 파괴는 서버만 수행한다.
        클라이언트 사본도 충돌 이벤트를 받지만 여기서 처리하면
        데미지가 이중 적용되거나 서버보다 먼저 사라진다.
        (Destroy는 서버에서 호출하면 클라 사본까지 정리된다)
    */
    if (!HasAuthority())
    {
        return;
    }

    HandleImpact(OtherActor, Hit);

    /*
        즉시 Destroy하면 방금 보낸 멀티캐스트가 일부 클라이언트에서 유실될 수 있다.
        먼저 숨기고 짧은 수명을 준다.
    */
    SetActorEnableCollision(false);
    if (ProjectileMovement)
    {
        ProjectileMovement->StopMovementImmediately();
    }
    SetActorHiddenInGame(true);
    SetLifeSpan(0.2f);
}

void AProjectile::Multicast_PlayImpactFX_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal)
{
    if (UNiagaraSystem* VFX = GetImpactVFX())
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), VFX, Location, FVector(Normal).Rotation(), FVector(GetImpactFXScale()));
    }
    if (USoundBase* SFX = GetImpactSound())
    {
        UGameplayStatics::PlaySoundAtLocation(this, SFX, Location);
    }
}

void AProjectile::HandleImpact(AActor* HitActor, const FHitResult& Hit)
{
    // 기본 투사체: 직접 맞은 대상 1명
    ACommonCharacter::ApplyDamageEffect(HitActor, DamageEffectClass, Damage, GetInstigator());

    // 폭발 연출은 전 클라이언트로 멀티캐스트한다.
    // 서버에서 직접 스폰하면 데디케이티드 서버에서는 아무에게도 보이지 않는다.
    Multicast_PlayImpactFX(Hit.ImpactPoint, Hit.ImpactNormal);
}