#include "ProjectileRocket.h"
#include "Character/CommonCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

void AProjectileRocket::HandleImpact(AActor* HitActor, const FHitResult& Hit)
{
    // 단일 대상 대신 반경 폭발 (연출 포함)
    Explode(Hit.ImpactPoint);
}

void AProjectileRocket::Explode(const FVector& Center)
{
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_Pawn);

    GetWorld()->OverlapMultiByObjectType(
        Overlaps, Center, FQuat::Identity, ObjParams,
        FCollisionShape::MakeSphere(ExplosionRadius), Params);

    AActor* InstigatorActor = GetInstigator();

    TSet<ACommonCharacter*> Damaged;
    for (const FOverlapResult& O : Overlaps)
    {
        ACommonCharacter* Char = Cast<ACommonCharacter>(O.GetActor());
        if (!Char || Char == InstigatorActor || Damaged.Contains(Char)) continue;

        Damaged.Add(Char);

        const float Dist = FVector::Dist(Center, Char->GetActorLocation());
        const float Alpha = FMath::Clamp(1.f - (Dist / ExplosionRadius), 0.f, 1.f);
        const float Falloff = FMath::Lerp(MinDamageRatio, 1.f, Alpha);

        ACommonCharacter::ApplyDamageEffect(Char, DamageEffectClass, Damage * Falloff, GetInstigator());
    }

    UNiagaraSystem* VFX = ExplosionVFX ? ExplosionVFX : ImpactVFX;
    if (VFX)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX, Center, FRotator::ZeroRotator, FVector(3.f), true, true);

    USoundBase* SFX = ExplosionSound ? ExplosionSound : ImpactSound;
    if (SFX)
        UGameplayStatics::PlaySoundAtLocation(this, SFX, Center);

    if (bDrawDebugExplosion)
        DrawDebugSphere(GetWorld(), Center, ExplosionRadius, 16, FColor::Orange, false, 1.5f);
}