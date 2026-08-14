#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AmmoWidget.generated.h"

class APlayerCharacter;
class UTextBlock;

UCLASS()
class TPSGAME_API UAmmoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HUD가 빙의 시점에 호출.
	void SetOwningCharacter(APlayerCharacter* InPlayer);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(VisibleAnyWhere, BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> WeaponTypeText;

	UPROPERTY(VisibleAnyWhere, BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> AmmoText;

	void Unbind();

	UFUNCTION()
	void HandleWeaponChanged(EWeaponType WeaponType);

private:
	UFUNCTION()
	void HandleAmmoChanged(int32 NewCurrentAmmo, int32 NewMaxAmmo);

	TWeakObjectPtr<APlayerCharacter> OwnerCharacter;
};