#include "CoverPoint.h"
#include "Components/ArrowComponent.h"

ACoverPoint::ACoverPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(Root);
    DirectionArrow->ArrowSize = 1.5f;
    DirectionArrow->ArrowColor = FColor::Cyan;
    DirectionArrow->bIsEditorOnly = true;
#endif
}