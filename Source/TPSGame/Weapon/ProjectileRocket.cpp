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
    /*
        판정(반경 데미지)은 서버에서만, 연출은 멀티캐스트로 전원에게.

        기존에는 이 함수가 둘 다 로컬로 처리했다. 로켓 발사는 서버에서만
        실행되므로 데디케이티드 서버에서는 폭발 연출이 아무에게도 보이지 않았다.
    */
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

        // 폭발은 아군도 맞는다. 반경 판정은 조준 실수가 아니라 위치 선택의 결과다.
        ACommonCharacter::ApplyDamageEffect(
            Char, DamageEffectClass, Damage * Falloff, GetInstigator(), /*bAffectsFriendly=*/true);
    }

    // 연출은 베이스의 멀티캐스트를 재사용한다.
    Multicast_PlayImpactFX(Center, FVector::UpVector);

    if (bDrawDebugExplosion)
        DrawDebugSphere(GetWorld(), Center, ExplosionRadius, 16, FColor::Orange, false, 1.5f);
}