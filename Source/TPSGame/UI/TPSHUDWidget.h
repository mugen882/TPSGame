#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TPSHUDWidget.generated.h"

class APlayerCharacter;

class UAmmoWidget;
class UHealthBarWidget;
class UVignetteWidget;
class UCrosshairWidget;

/*
	크로스헤어/탄약/비네트/체력바를 하나로 묶는 단일 HUD.
	- 소유/생성/뷰포트 추가는 PlayerController가 담당
	- 자식 위젯은 UMG 디자이너(WBP_HUD) 트리에서 BindWidget으로 연결
	- 실제 데이터 바인딩은 각 자식 위젯이 SetOwningCharacter 안에서 수행
 */
UCLASS()
class TPSGAME_API UTPSHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 빙의 시점에 컨트롤러가 호출. 플레이어 참조를 자식들에게 분배
	void InitFor(APlayerCharacter* Player);

	APlayerCharacter* GetOwningCharacter() const { return OwnerCharacter.Get(); }

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCrosshairWidget> Crosshair;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAmmoWidget> Ammo;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHealthBarWidget> HealthBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UVignetteWidget> Vignette;

private:
	TWeakObjectPtr<APlayerCharacter> OwnerCharacter;
};
