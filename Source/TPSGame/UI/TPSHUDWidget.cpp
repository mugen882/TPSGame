#include "UI/TPSHUDWidget.h"
#include "Character/PlayerCharacter.h"

#include "UI/AmmoWidget.h"
#include "UI/HealthBarWidget.h"
#include "UI/VignetteWidget.h"
#include "UI/CrosshairWidget.h"

void UTPSHUDWidget::InitFor(APlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}

	OwnerCharacter = Player;

	// 각 자식 위젯이 자기 필요한 델리게이트/ASC에 바인딩하도록 플레이어 참조만 넘긴다.
	if (Crosshair)
	{
		Crosshair->SetOwningCharacter(Player);
	}
	if (Ammo)
	{
		Ammo->SetOwningCharacter(Player);
	}
	if (HealthBar)
	{
		HealthBar->SetOwningCharacter(Player);
	}
	if (Vignette)
	{
		Vignette->SetOwningCharacter(Player);
	}
}
