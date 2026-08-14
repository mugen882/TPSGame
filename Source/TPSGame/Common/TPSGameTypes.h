#pragma once
#include "Engine/EngineTypes.h"
#include "NativeGameplayTags.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	None UMETA(Hidden),
    Rifle,
    RocketLauncher,
	MachineGun,
	MAX UMETA(Hidden)
};