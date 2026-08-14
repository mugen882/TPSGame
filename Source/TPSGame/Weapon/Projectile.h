#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class UGameplayEffect;

/*
    발사체 클래스
    날아가다 적에게 히트시 데미지 줌
*/

UCLASS()
class TPSGAME_API AProjectile : public AActor
{
    GENERATED_BODY()

public:
    AProjectile();

public:
    void SetDamage(float InDamage) { Damage = InDamage; }
    void SetDamageEffectClass(const TSubclassOf<UGameplayEffect>& InDamageEffectClass) { DamageEffectClass = InDamageEffectClass; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
               UPrimitiveComponent* OtherComp, FVector NormalImpulse,
               const FHitResult& Hit);

    // 히트 시 실제 피해/연출 — 서브클래스가 오버라이드 (기본: 단일 대상)
    virtual void HandleImpact(AActor* HitActor, const FHitResult& Hit);

protected:
    // 충돌 + 루트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionComp;

    // 시각적 메시 (총알/탄)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    // 발사체 이동
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    // 충돌 VFX/SFX
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|FX")
    TObjectPtr<UNiagaraSystem> ImpactVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|FX")
    TObjectPtr<USoundBase> ImpactSound;

    UPROPERTY(EditAnywhere, Category="Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float Damage = 1.f;
};