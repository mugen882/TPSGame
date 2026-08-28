#include "Character/CommonCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/TPSAttributeSet.h"
#include "Common/TPSGameplayTags.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffect.h"
#include "Weapon/WeaponBase.h"
#include "UI/HealthBarWidget.h"
#include "Weapon/WeaponManagerComponent.h"
#include "Network/LagCompensationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Common/TPSLog.h"

ACommonCharacter::ACommonCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

    // ASC 자체의 리플리케이션
    AbilitySystemComponent->SetIsReplicated(true);

    /*
        GameplayEffect 리플리케이션 모드
    */
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UTPSAttributeSet>(TEXT("AttributeSet"));

    WeaponManager = CreateDefaultSubobject<UWeaponManagerComponent>(TEXT("WeaponManager"));

    LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

    OnWeaponChanged.AddDynamic(this, &ACommonCharacter::ChangeWeapon);
}

UAbilitySystemComponent* ACommonCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACommonCharacter::InitAbilityActorInfoAndBind(const TCHAR* CallSite)
{
    if (!AbilitySystemComponent || !AttributeSet)
    {
        UE_LOG(TPSLog, Error, TEXT("%s InitAbilityActorInfo SKIPPED (from %s) — ASC=%s, AttributeSet=%s"),
            *TPSNetDebug::TPSNetPrefix(this), CallSite,
            AbilitySystemComponent ? TEXT("OK") : TEXT("NULL"),
            AttributeSet ? TEXT("OK") : TEXT("NULL"));
        return;
    }

    // InitAbilityActorInfo는 중복 호출에 안전하다
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // 델리게이트는 중복 바인딩되면 콜백이 여러 번 불리므로 한 번만 건다.
    if (!bAttributeDelegatesBound)
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
            .AddUObject(this, &ACommonCharacter::HandleHealthChanged);

        /*
            탄약 UI는 이제 델리게이트로 자동 갱신된다.
            예측 차감(클라 즉시)과 서버 확정(OnRep) 양쪽에서 발화함
        */
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetRifleAmmoAttribute())
            .AddUObject(this, &ACommonCharacter::HandleAmmoChanged);
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMachineGunAmmoAttribute())
            .AddUObject(this, &ACommonCharacter::HandleAmmoChanged);
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetRocketAmmoAttribute())
            .AddUObject(this, &ACommonCharacter::HandleAmmoChanged);

        /*
            State.Dead 태그 구독.

            사망 연출의 트리거를 "체력 0 관측"에서 "태그 변화"로 옮겼다.
            서버가 GE_Dead를 적용하면 태그가 전원에게 복제되고, 각 머신이
            여기서 로컬 연출을 수행한다. 늦게 접속한 클라이언트도 초기 복제로
            태그를 받으므로 이미 죽은 캐릭터가 산 것처럼 보이지 않는다.
        */
        AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_Dead, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &ACommonCharacter::OnDeadTagChanged);

        bAttributeDelegatesBound = true;

        UE_LOG(TPSLog, Verbose, TEXT("%s InitAbilityActorInfo (from %s) — delegates BOUND, Health=%.1f/%.1f"),
            *TPSNetDebug::TPSNetPrefix(this), CallSite,
            AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
    }
    else
    {
        UE_LOG(TPSLog, Verbose, TEXT("%s InitAbilityActorInfo (from %s) — delegates skip, Health=%.1f/%.1f"),
            *TPSNetDebug::TPSNetPrefix(this), CallSite,
            AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
    }
}

void ACommonCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 서버 경로
    InitAbilityActorInfoAndBind(TEXT("PossessedBy"));
    GrantDefaultAbilities();
}

void ACommonCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // 클라이언트 경로. PlayerState 복제 순서에 따라 BeginPlay보다 늦을 수 있어
    // 여기서 한 번 더 보장한다.
    InitAbilityActorInfoAndBind(TEXT("OnRep_PlayerState"));
}

void ACommonCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 클라이언트 경로.
    // 이게 없으면 시뮬레이티드 프록시 적의 체력바가 갱신되지 않는다.
    InitAbilityActorInfoAndBind(TEXT("BeginPlay"));

    // 무기 액터를 리플리케이트로 전환하면서 HasAuthority() 게이트를 건다.
    // 지금 게이트를 걸면 클라에 무기 메시가 아예 없는 중간 상태가 되므로
    // 무기 슬롯 배열 + OnRep_CurrentWeapon 작업과 함께 옮긴다.
    SpawnWeapons();
}

bool ACommonCharacter::TargetIsDead(AActor* Actor)
{
    if (ACommonCharacter* TargetCharacter = Cast<ACommonCharacter>(Actor))
    {
        if (TargetCharacter->IsDead() == true)
        {
            return true;
        }
    }

    return false;
}

void ACommonCharacter::ServerHandleDeathAuthority()
{
    if (!HasAuthority() || !AbilitySystemComponent) return;
    if (IsDead()) return;                       // 이미 사망 처리됨
    if (!DeadEffectClass) 
    {
        UE_LOG(TPSLog, Error, TEXT("%s DeadEffectClass 미지정 — 사망 태그가 복제되지 않는다"),
            *TPSNetDebug::TPSNetPrefix(this));
        return;
    }

    /*
        Infinite GE로 State.Dead를 부여한다.

        Loose 태그와 달리 GE가 부여한 태그는 ReplicationMode가 Mixed든 Minimal이든
        모든 클라이언트로 복제된다. 늦게 접속한 클라이언트도 초기 복제 시점에 받는다.
    */
    FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
    Context.AddSourceObject(this);

    FGameplayEffectSpecHandle Spec =
        AbilitySystemComponent->MakeOutgoingSpec(DeadEffectClass, 1.0f, Context);
    if (Spec.IsValid())
    {
        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    }

    AbilitySystemComponent->CancelAllAbilities();

    UE_LOG(TPSLog, Verbose, TEXT("%s 사망 처리 (GE_Dead 적용)"), *TPSNetDebug::TPSNetPrefix(this));
}

void ACommonCharacter::OnDeadTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    // 태그가 붙는 순간에만 반응한다. 제거(부활)는 폰 교체로 처리하므로 여기서 다루지 않는다.
    if (NewCount > 0)
    {
        HandleDeath();
    }
}

void ACommonCharacter::HandleDeath()
{
    if (bDeathPresented)
    {
        return;   // 태그 이벤트가 중복 발화해도 연출은 한 번만
    }
    bDeathPresented = true;

    GetCharacterMovement()->DisableMovement();

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetMesh()->SetAllBodiesSimulatePhysics(true);
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->WakeAllRigidBodies();
}

bool ACommonCharacter::IsDead() const
{
    return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Dead);
}

bool ACommonCharacter::IsReloading() const
{
    return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Reloading);
}

bool ACommonCharacter::IsSwapping() const
{
    return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Swapping);
}

void ACommonCharacter::ApplyDamageEffect(
    AActor* Target,
    TSubclassOf<UGameplayEffect> DamageEffectClass,
    float Damage,
    AActor* SourceActor)
{
    if (!Target || !DamageEffectClass || !SourceActor) return;

    ACommonCharacter* HitChar = Cast<ACommonCharacter>(Target);
    if (!HitChar) return;

    /*
        데미지 적용은 서버 전용이다.
        트레이스와 데미지는 서버에서만 일어난다.
    */
    if (!HitChar->HasAuthority()) return;

    UAbilitySystemComponent* TargetASC = HitChar->GetAbilitySystemComponent();
    if (!TargetASC) return;

    ACommonCharacter* SourceChar = Cast<ACommonCharacter>(SourceActor);
    UAbilitySystemComponent* SourceASC = SourceChar ? SourceChar->GetAbilitySystemComponent() : nullptr;
    if (!SourceASC)
    {
        return;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(SourceActor);
    Context.AddInstigator(SourceActor, SourceActor);   // 가해자 등록 (방향성 비네트용)

    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage, Damage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC); // PostGameplayEffectExecute에서 Health가 차감된다.
    }
}

void ACommonCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 가해자는 피격자 본인만 알면 된다.
    DOREPLIFETIME_CONDITION(ACommonCharacter, LastDamageInstigator, COND_OwnerOnly);
}

void ACommonCharacter::NotifyDamageFrom(AActor* DamageInstigator)
{
    LastDamageInstigator = DamageInstigator;
}

void ACommonCharacter::SpawnWeapons()
{
    if (!WeaponManager) return;
    WeaponManager->SpawnWeapons();

    const TMap<EWeaponType, AWeaponBase*>& Weapons = WeaponManager->GetWeapons();
    for (const TPair<EWeaponType, AWeaponBase*>& Pair : Weapons)
    {
		AWeaponBase* Weapon = Pair.Value;
        if (Weapon)
            Weapon->OnHitConfirmed.AddDynamic(this, &ACommonCharacter::HandleWeaponHitConfirmed);
    }

    EquipInitialWeapon();

    // 무기가 준비된 뒤에야 MaxAmmo를 알 수 있으므로 여기서 초기화한다.
    InitAmmoAttributes();
}

void ACommonCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACommonCharacter::GrantDefaultAbilities()
{
    // 어빌리티 부여는 서버 권위. 클라는 ASC 리플리케이션으로 스펙을 받는다.
    if (!HasAuthority()) return;

    if (bAbilitiesGranted || !AbilitySystemComponent) return;

    for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
    {
        if (Ability)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1));
        }
    }

    bAbilitiesGranted = true;

    UE_LOG(TPSLog, Verbose, TEXT("%s GrantDefaultAbilities — %d개 부여"),
        *TPSNetDebug::TPSNetPrefix(this), DefaultAbilities.Num());
}

void ACommonCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    // 이제 서버뿐 아니라 클라이언트에서도 호출된다.
    // (AttributeSet 리플리케이션 + OnRep -> ASC 델리게이트 발화)
    if (Data.NewValue < Data.OldValue)   // 피격
    {
        if (AttributeSet->GetHealth() <= 0.0f)
        {
            /*
                사망 판정은 서버만 한다.

                이전에는 각 머신이 여기서 HandleDeath를 직접 불러 로컬 태그를 붙였다.
                이제 서버가 GE_Dead를 적용하고, 연출은 태그 복제를 받은
                OnDeadTagChanged가 각 머신에서 수행한다.
            */
            ServerHandleDeathAuthority();
        }
        else
        {
            float Damage = Data.OldValue - Data.NewValue;
            OnDamaged(Damage);
        }
    }
}

void ACommonCharacter::HandleWeaponHitConfirmed()
{
    // 서버가 원격 폰의 명중을 판정한 경우, 사격자 클라에게 전달한다.
    if (HasAuthority() && !IsLocallyControlled())
    {
        Client_NotifyHitConfirmed();
        return;
    }

    OnHitConfirmed.Broadcast();
}

void ACommonCharacter::Client_NotifyHitConfirmed_Implementation()
{
    OnHitConfirmed.Broadcast();
}

int32 ACommonCharacter::GetCurrentWeaponAmmo()
{
    AWeaponBase* Weapon = GetCurrentWeapon();
    if (!Weapon || !AbilitySystemComponent) return 0;

    const FGameplayAttribute Attr = Weapon->GetAmmoAttribute();
    if (!Attr.IsValid()) return 0;

    return FMath::RoundToInt(AbilitySystemComponent->GetNumericAttribute(Attr));
}

bool ACommonCharacter::HasCurrentWeaponAmmo()
{
    AWeaponBase* Weapon = GetCurrentWeapon();
    if (!Weapon) return false;

    // 무한 탄약 무기는 항상 발사 가능
    if (Weapon->IsInfiniteAmmo()) return true;

    return GetCurrentWeaponAmmo() > 0;
}

bool ACommonCharacter::IsCurrentWeaponAmmoFull()
{
    AWeaponBase* Weapon = GetCurrentWeapon();
    if (!Weapon) return true;

    if (Weapon->IsInfiniteAmmo()) return true;

    return GetCurrentWeaponAmmo() >= Weapon->GetMaxAmmo();
}

void ACommonCharacter::InitAmmoAttributes()
{
    // 탄약의 원본은 서버다. 클라는 복제로 받는다.
    if (!HasAuthority() || !AbilitySystemComponent || !WeaponManager) return;

    for (const TPair<EWeaponType, AWeaponBase*>& Pair : WeaponManager->GetWeapons())
    {
        AWeaponBase* Weapon = Pair.Value;
        if (!Weapon) continue;

        const FGameplayAttribute Attr = Weapon->GetAmmoAttribute();
        if (!Attr.IsValid()) continue;

        AbilitySystemComponent->SetNumericAttributeBase(Attr, (float)Weapon->GetMaxAmmo());
    }
}

void ACommonCharacter::HandleAmmoChanged(const FOnAttributeChangeData& Data)
{
    BroadcastAmmo();
}

void ACommonCharacter::GameplayCue_Weapon_Fire(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
    if (EventType != EGameplayCueEvent::Executed) return;

    if (AWeaponBase* Weapon = GetCurrentWeapon())
    {
        Weapon->ShowMuzzleFlash();
        Weapon->PlayFireSound();
    }
}

void ACommonCharacter::GameplayCue_Weapon_Impact(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
    if (EventType != EGameplayCueEvent::Executed) return;

    if (AWeaponBase* Weapon = GetCurrentWeapon())
    {
        // 탄착 위치/노멀은 판정한 쪽(서버) 또는 예측한 쪽(클라)이 파라미터로 실어 보낸다.
        Weapon->PlayImpactEffectAt(Parameters.Location, Parameters.Normal);
    }
}

void ACommonCharacter::ExecuteFireCue()
{
    if (!AbilitySystemComponent) return;

    FGameplayCueParameters Params;
    Params.Instigator = this;
    Params.SourceObject = GetCurrentWeapon();

    AbilitySystemComponent->ExecuteGameplayCue(TAG_Cue_Weapon_Fire, Params);
}

void ACommonCharacter::ExecuteImpactCue(const FVector& Location, const FVector& Normal)
{
    if (!AbilitySystemComponent) return;

    // 탄착 위치는 판정한 쪽(서버) 또는 예측한 쪽(클라)만 알기 때문에 파라미터로 전달한다.
    FGameplayCueParameters Params;
    Params.Instigator = this;
    Params.SourceObject = GetCurrentWeapon();
    Params.Location = Location;
    Params.Normal = Normal;

    AbilitySystemComponent->ExecuteGameplayCue(TAG_Cue_Weapon_Impact, Params);
}

void ACommonCharacter::BroadcastAmmo()
{
    if (WeaponManager == nullptr)
    {
        return;
    }

    if (AWeaponBase* CurrentWeapon = WeaponManager->GetCurrentWeapon())
    {
        OnAmmoChanged.Broadcast(GetCurrentWeaponAmmo(), CurrentWeapon->GetMaxAmmo());
    }
}

void ACommonCharacter::ChangeWeapon(EWeaponType WeaponType)
{
    BroadcastAmmo();
}
