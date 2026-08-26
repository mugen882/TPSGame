#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileRocket.generated.h"

UCLASS()
class TPSGAME_API AProjectileRocket : public AProjectile
{
    GENERATED_BODY()

public:
	float GetExplosionRadius() const { return ExplosionRadius; }

protected:
    virtual void HandleImpact(AActor* HitActor, const FHitResult& Hit) override;

    void Explode(const FVector& Center);

    virtual UNiagaraSystem* GetImpactVFX() const override
    {
        return ExplosionVFX ? ExplosionVFX : ImpactVFX;
    }

    virtual USoundBase* GetImpactSound() const override
    {
        return ExplosionSound ? ExplosionSound : ImpactSound;
    }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Explosion")
    float ExplosionRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Explosion")
    float MinDamageRatio = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Explosion")
    TObjectPtr<class UNiagaraSystem> ExplosionVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Explosion")
    TObjectPtr<class USoundBase> ExplosionSound;

    UPROPERTY(EditAnywhere, Category = "Rocket|Explosion")
    bool bDrawDebugExplosion = false;
};