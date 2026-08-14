#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarWidget.generated.h"

class APlayerCharacter;
class ACommonCharacter;
class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

/*
	체력바 위젯 클래스.
	- 플레이어 HUD 와 적 HP바에서 공용으로 사용한다.
	- 적   : SetAbilitySystem(EnemyASC) 으로 ASC 직접 전달
	- 플레이어 HUD : SetOwningCharacter(Player) → 내부에서 ASC 추출 후 동일 경로
	모두 ASC 의 Health / MaxHealth 속성 변경에 바인딩한다(GAS).
 */
UCLASS()
class TPSGAME_API UHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 임의의 ASC 에 바인딩. (적 캐릭터가 직접 호출)
	void SetAbilitySystem(UAbilitySystemComponent* InASC);

	// 플레이어 HUD 경로: 플레이어에서 ASC 를 추출해 SetAbilitySystem 으로 위임.
	void SetOwningCharacter(ACommonCharacter* InPlayer);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(VisibleAnyWhere, BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthBarFill;

	UPROPERTY(VisibleAnyWhere, BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> HealthText;

private:
	// GAS 속성 변경 콜백 (AddUObject 바인딩 → UFUNCTION 아님)
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);

	void UpdateBar();
	void UpdateText();
	void Unbind();

	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

	float CachedHealth = 0.0f;
	float CachedMaxHealth = 0.0f;
};
