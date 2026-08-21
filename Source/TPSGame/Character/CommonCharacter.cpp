#include "Character/CommonCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/TPSAttributeSet.h"
#include "Common/TPSGameplayTags.h"
#include "GameplayEffect.h"
#include "Weapon/WeaponBase.h"
#include "UI/HealthBarWidget.h"
#include "Weapon/WeaponManagerComponent.h"
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

    // TODO(M1): 무기 액터를 리플리케이트로 전환하면서 HasAuthority() 게이트를 건다.
    //           지금 게이트를 걸면 클라에 무기 메시가 아예 없는 중간 상태가 되므로
    //           무기 슬롯 배열 + OnRep_CurrentWeapon 작업과 함께 옮긴다.
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

void ACommonCharacter::HandleDeath()
{
    if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Dead))
    {
        return;
    }

    /*
        TODO(M3): AddLooseGameplayTag는 리플리케이트되지 않는다.

        지금은 각 머신에서 Health가 0으로 복제되는 순간 HandleHealthChanged가
        로컬로 HandleDeath를 호출하므로 결과적으로 태그가 각자 붙는다.
        하지만 늦게 접속한 클라이언트나 부활/힐 경로에서는 상태가 어긋난다.
        Dead 태그를 부여하는 GE를 서버에서 적용하는 방식으로 교체할 것.
    */
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(TAG_State_Dead);
        AbilitySystemComponent->CancelAllAbilities();
    }

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
    /*
        TODO(M2): 현재는 "피해자 ASC로 스펙을 만들어 자기 자신에게 적용"하는 구조다.
                  코옵에서는 킬 어트리뷰션과 소스 기반 계수가 전부 피해자 기준이 되어버린다.
                  SourceASC->MakeOutgoingSpec() -> ApplyGameplayEffectSpecToTarget(TargetASC)로 바꾸고
                  서버 전용으로 게이트할 것. (히트스캔 서버 권위화와 같은 작업 단위)
    */
    if (!Target || !DamageEffectClass) return;

    ACommonCharacter* HitChar = Cast<ACommonCharacter>(Target);
    if (!HitChar) return;

    UAbilitySystemComponent* TargetASC = HitChar->GetAbilitySystemComponent();
    if (!TargetASC) return;

    FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
    Context.AddSourceObject(SourceActor);
    Context.AddInstigator(SourceActor, SourceActor);   // 가해자 등록 (방향성 비네트용)

    FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage, Damage);
        TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    }
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
            if (!IsDead())   // 아직 사망 태그 없을 때
            {
                HandleDeath();
            }
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
