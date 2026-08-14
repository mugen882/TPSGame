#include "UI/AmmoWidget.h"
#include "Character/PlayerCharacter.h"
#include "Components/TextBlock.h"

void UAmmoWidget::SetOwningCharacter(APlayerCharacter* InPlayer)
{
	if (!InPlayer)
	{
		return;
	}

	// 재빙의 등으로 중복 바인딩되지 않도록 이전 구독 정리
	Unbind();

	OwnerCharacter = InPlayer;
	InPlayer->OnAmmoChanged.AddDynamic(this, &UAmmoWidget::HandleAmmoChanged);
	InPlayer->OnWeaponChanged.AddDynamic(this, &UAmmoWidget::HandleWeaponChanged);

	// 바인딩 직후 현재 값으로 1회 갱신
	InPlayer->BroadcastAmmo();
}

void UAmmoWidget::NativeDestruct()
{
	Super::NativeDestruct();

	Unbind();
}


void UAmmoWidget::Unbind()
{
	if (APlayerCharacter* Player = OwnerCharacter.Get())
	{
		Player->OnWeaponChanged.RemoveDynamic(this, &UAmmoWidget::HandleWeaponChanged);
		Player->OnAmmoChanged.RemoveDynamic(this, &UAmmoWidget::HandleAmmoChanged);
	}
}


void UAmmoWidget::HandleWeaponChanged(EWeaponType WeaponType)
{
	FString WeaponStr = StaticEnum<EWeaponType>()->GetNameStringByValue((int64)WeaponType);
	WeaponTypeText->SetText(FText::FromString(WeaponStr));
}

void UAmmoWidget::HandleAmmoChanged(int32 NewCurrentAmmo, int32 NewMaxAmmo)
{
	if (AmmoText)
	{
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), NewCurrentAmmo, NewMaxAmmo)));
	}
}
