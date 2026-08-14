#include "Character/TPSPlayerController.h"
#include "UI/TPSHUDWidget.h"
#include "PlayerCharacter.h"
#include "Common/TPSGameDefine.h"

void ATPSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 화면에 그리는 UI는 로컬 컨트롤러에서만 생성
	if (!IsLocalController())
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(InPawn);
	if (!PlayerCharacter)
	{
		return;
	}

	// HUD는 단 한 번만 생성하고, 이후 빙의 때는 참조만 다시 연결한다.
	if (!HUDWidget && HUDClass)
	{
		HUDWidget = CreateWidget<UTPSHUDWidget>(this, HUDClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}

	if (HUDWidget)
	{
		HUDWidget->InitFor(PlayerCharacter);
	}
}
