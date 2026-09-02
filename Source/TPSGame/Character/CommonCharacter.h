#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "Weapon/WeaponManagerComponent.h"
#include "CommonCharacter.generated.h"

class UAbilitySystemComponent;
class UTPSAttributeSet;
class AWeaponBase;
class ULagCompensationComponent;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHitConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, EWeaponType, WeaponType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAmmoReloaded);
// 탄약 변경 델리게이트 (현재 탄약, 최대 탄약을 전달)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, int32, NewCurrentAmmo, int32, NewMaxAmmo);

/*
	공용 캐릭터 클래스
	적 / 플레이어의 공통 로직을 담고 있음
	UAbilitySystemComponent, UTPSAttributeSet을 들고있음

	[네트워크 노트]
	ASC는 캐릭터에 그대로 둔다. 적(AEnemyCharacter)이 같은 베이스를 쓰는데
	PlayerState가 없기 때문에, ASC를 PlayerState로 올리면 플레이어/적 경로가 갈라진다.
	웨이브 디펜스 코옵에서는 리스폰 시 어트리뷰트를 다시 초기화하면 되므로
	PlayerState 이관의 이점이 크지 않다고 판단.

	데디케이티드 서버에서 애님 노티파이가 발생하지 않아 적이 사격하지 못할 것을
	우려해 VisibilityBasedAnimTickOption을 명시한 적이 있으나, 패키징 서버 실측
	결과 엔진 기본값(AlwaysTickPoseAndRefreshBones)이 이미 이를 보장하고 있었다.
	누군가 이 값을 OnlyTickPoseWhenRendered 등으로 바꾸면 그때 다시 문제가 된다.
*/

UCLASS()
class TPSGAME_API ACommonCharacter : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	ACommonCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void OnFireNotify()
		PURE_VIRTUAL(ACommonCharacter::OnFireNotify);

	virtual FVector GetAimPoint() const
		PURE_VIRTUAL(ACommonCharacter::GetAimPoint, return FVector::ZeroVector;);

	virtual void OnReloadFinished()
		PURE_VIRTUAL(ACommonCharacter::OnReloadFinished, );

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool IsDead() const;

	bool IsReloading() const;
	bool IsSwapping() const;

	/*
		대상에게 데미지 GE 적용 (무기·투사체 공용). 서버 전용.

		bAffectsFriendly는 무기의 성격을 선언한다.
		  false (기본) : 히트스캔·직격 — 아군에게는 들어가지 않는다
		  true         : 폭발 — 반경 안이면 아군도 맞는다
	*/
	static void ApplyDamageEffect(
		AActor* Target,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Damage,
		AActor* SourceActor,
		bool bAffectsFriendly = false);

	/*
		적대 관계 판정.

		둘 중 하나라도 ACommonCharacter가 아니면 적대로 간주한다.
		팀 개념이 없는 대상(파괴 가능한 오브젝트 등)을 막지 않기 위해서다.
	*/
	static bool AreHostile(const AActor* A, const AActor* B);

	uint8 GetTeamId() const { return TeamId; }

	void NotifyDamageFrom(AActor* DamageInstigator);

	TObjectPtr<UAnimMontage> GetFireMontage() { return FireMontage; }

	FORCEINLINE AWeaponBase* GetCurrentWeapon()
	{
		return WeaponManager ? WeaponManager->GetCurrentWeapon() : nullptr;
	}

	UWeaponManagerComponent* GetWeaponManager() { return WeaponManager; }

	void SpawnWeapons();

	void BroadcastAmmo();

	/*
		발사 연출 GameplayCue 실행.

		서버에서 호출하면 모든 클라이언트로 멀티캐스트되고,
		소유 클라이언트에서 예측 키와 함께 호출하면 로컬에서만 재생된다.
		같은 예측 키를 쓰면 GAS가 중복 재생을 막아준다.
	*/
	void ExecuteFireCue();
	void ExecuteImpactCue(const FVector& Location, const FVector& Normal);

	/*
		GameplayCue 핸들러 — IGameplayCueInterface

		함수 이름이 곧 태그다. GameplayCue.Weapon.Fire -> GameplayCue_Weapon_Fire.
		GameplayCueManager가 리플렉션으로 찾아 호출하므로
		Cue Notify 애셋도, 스캔 경로도, 애셋 이름 규칙도 필요 없다.
	*/
	UFUNCTION(BlueprintCallable, Category = "GameplayCue", meta = (BlueprintProtected = "true"))
	void GameplayCue_Weapon_Fire(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue", meta = (BlueprintProtected = "true"))
	void GameplayCue_Weapon_Impact(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);


	/*
		현재 무기의 탄약 조회.

		탄약이 UTPSAttributeSet으로 이관되었으므로 무기 액터가 아니라 ASC에서 읽는다.
		호출부(TryFire, GA_Reload, 머신건 연사 루프, BT)가 무기 종류를 몰라도 되도록
		무기의 GetAmmoAttribute()를 경유한다.
	*/
	int32 GetCurrentWeaponAmmo();
	bool  HasCurrentWeaponAmmo();
	bool  IsCurrentWeaponAmmoFull();

	// 서버 전용. 소유한 모든 무기의 탄약을 MaxAmmo로 초기화한다.
	void InitAmmoAttributes();

	FORCEINLINE UTPSAttributeSet* GetAttributeSet() { return AttributeSet; }

public:
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnHitConfirmed OnHitConfirmed;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnWeaponChanged OnWeaponChanged;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnAmmoReloaded OnAmmoReloaded;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnAmmoChanged OnAmmoChanged;

protected:
	// 서버 전용 초기화 경로
	virtual void PossessedBy(AController* NewController) override;

	// 클라이언트 초기화 경로 — PlayerState가 복제된 시점
	virtual void OnRep_PlayerState() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/*
		ASC ActorInfo 초기화 + 어트리뷰트 델리게이트 바인딩

		서버(PossessedBy) / 플레이어 클라(OnRep_PlayerState) / AI 프록시(BeginPlay)
		세 경로에서 모두 호출되며, 중복 호출에 안전하도록 작성했다.
	*/
	void InitAbilityActorInfoAndBind(const TCHAR* CallSite);

	/*
		State.Dead 태그 변화 구독.

		이전에는 각 머신이 체력 0을 관측해 로컬로 AddLooseGameplayTag를 했다.
		Loose 태그는 복제되지 않아 늦게 접속한 클라이언트나 부활 경로에서 어긋났다.
		이제 서버가 GE_Dead(Infinite, GrantedTags: State.Dead)를 적용하고
		각 머신은 태그 변화에만 반응한다.
	*/
	UFUNCTION()
	void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 서버 전용. 사망 판정과 GE_Dead 적용.
	void ServerHandleDeathAuthority();

	// 서버 전용. DefaultAbilities 부여
	void GrantDefaultAbilities();

	bool TargetIsDead(AActor* Actor);

	/*
		사망 연출. 태그 변화에 반응해 각 머신에서 로컬로 실행된다.

		서버가 GE_Dead를 적용하면 State.Dead 태그가 전원에게 복제되고,
		각 머신의 태그 이벤트가 이 함수를 부른다.
		늦게 접속한 클라이언트도 초기 복제 시점에 태그를 받아 래그돌이 적용된다.
	*/
	virtual void HandleDeath();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void HandleHealthChanged(const struct FOnAttributeChangeData& Data);

	// 탄약 어트리뷰트 변경 → 탄약 UI 갱신 (예측 차감과 서버 확정 양쪽에서 발화)
	void HandleAmmoChanged(const struct FOnAttributeChangeData& Data);

	virtual void OnDamaged(float InDamage)
		PURE_VIRTUAL(ACommonCharacter::OnDamaged, );

	float GetAimRange() const { return AimRange; }

	virtual void EquipInitialWeapon()
		PURE_VIRTUAL(ACommonCharacter::EquipInitialWeapon, );

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UWeaponManagerComponent> WeaponManager;

	/*
		랙 보상용 히트박스 히스토리.

		서버에서만 기록하며, 발사 판정 시 사격자의 지연만큼 되감는 데 쓰인다.
		적과 플레이어 모두 되감기 대상이므로 공용 베이스에 둔다.
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Network")
	TObjectPtr<ULagCompensationComponent> LagCompensation;

	/*
		가해자. 방향성 비네트와 적 AI의 반격 판단에 쓰인다.

		PostGameplayEffectExecute는 서버에서만 실행되므로 서버만 이 값을 안다.
		클라이언트는 체력 복제로 피격은 알지만 "어디서 맞았는지"를 알 수 없어
		방향성 비네트가 나오지 않았다. 그래서 복제한다.

		COND_OwnerOnly: 남이 누구에게 맞았는지는 알 필요가 없다.
	*/
	UPROPERTY(Replicated)
	TObjectPtr<AActor> LastDamageInstigator = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/*
		팀 식별자. 0 = 플레이어, 1 = 적.

		서버에서만 판정에 쓰이므로 복제하지 않는다.
	*/
	UPROPERTY(EditDefaultsOnly, Category="Team")
	uint8 TeamId = 0;

private:
	UFUNCTION()
	void HandleWeaponHitConfirmed();

	/*
		히트마커.

		명중 판정은 서버에서만 일어나므로(FireInternal) OnHitConfirmed도
		서버에서만 발화한다. 사격자의 크로스헤어에 표시하려면 소유 클라에게
		따로 알려야 한다.
	*/
	UFUNCTION(Client, reliable)
	void Client_NotifyHitConfirmed();

	UFUNCTION()
	void ChangeWeapon(EWeaponType WeaponType);

private:
	const float AimRange = 10000.f;

	UPROPERTY(EditAnywhere, Category="Combat")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY()
	TObjectPtr<UTPSAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

	// State.Dead 태그를 부여하는 Infinite GE. 서버에서만 적용한다.
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> DeadEffectClass;

	bool bAbilitiesGranted = false;

	// 델리게이트 중복 바인딩 방지
	bool bAttributeDelegatesBound = false;

	// 사망 연출 1회 보장. 태그 이벤트가 중복 발화해도 래그돌을 다시 걸지 않는다.
	bool bDeathPresented = false;
};
