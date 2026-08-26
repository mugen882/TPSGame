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

    /*
        투사체는 복제한다.

        서버에서만 스폰하면 클라이언트 화면에 로켓이 보이지 않는다.
        히트스캔과 달리 비행 시간이 있어 "날아가는 모습" 자체가 연출이므로
        액터를 복제하는 편이 Cue로 흉내내는 것보다 간단하고 정확하다.
    */

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

    /*
        폭발 연출을 전 클라이언트에 재생.

        투사체가 복제 액터가 되었으므로 GameplayCue 대신 멀티캐스트를 쓴다.
        VFX/사운드 에셋이 투사체 BP 설정이라 Cue로 넘기려면 파라미터에
        억지로 실어야 하는데, 액터가 이미 모든 머신에 존재하니 그럴 이유가 없다.
    */
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayImpactFX(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal);

    virtual UNiagaraSystem* GetImpactVFX() const { return ImpactVFX; }
    virtual USoundBase* GetImpactSound() const { return ImpactSound; }
    virtual float           GetImpactFXScale() const { return 3.f; }

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