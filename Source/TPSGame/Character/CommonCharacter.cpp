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

ACommonCharacter::ACommonCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AttributeSet = CreateDefaultSubobject<UTPSAttributeSet>(TEXT("AttributeSet"));

    WeaponManager = CreateDefaultSubobject<UWeaponManagerComponent>(TEXT("WeaponManager"));

    OnWeaponChanged.AddDynamic(this, &ACommonCharacter::ChangeWeapon);
}

UAbilitySystemComponent* ACommonCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACommonCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    
    if (AbilitySystemComponent && AttributeSet)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        
        InitAbilities();

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
            .AddUObject(this, &ACommonCharacter::HandleHealthChanged);
    }
}

void ACommonCharacter::BeginPlay()
{
    Super::BeginPlay();

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

    // 사망 태그 부여
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
}

void ACommonCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACommonCharacter::InitAbilities()
{
    if (bAbilitiesGranted || !AbilitySystemComponent) return;

    for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
    {
        if (Ability)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1));
        }
    }

    bAbilitiesGranted = true;
}

void ACommonCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
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

void ACommonCharacter::BroadcastAmmo()
{
    if (WeaponManager == nullptr)
    {
        return;
    }

    if (AWeaponBase* CurrentWeapon = WeaponManager->GetCurrentWeapon())
    {
        OnAmmoChanged.Broadcast(WeaponManager->GetCurrentWeapon()->GetCurrentAmmo(), CurrentWeapon->GetMaxAmmo());
    }
}

void ACommonCharacter::ChangeWeapon(EWeaponType WeaponType)
{
    BroadcastAmmo();
}