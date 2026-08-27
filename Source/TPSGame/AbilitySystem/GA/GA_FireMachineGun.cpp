#include "AbilitySystem/GA/GA_FireMachineGun.h"
#include "Common/TPSGameplayTags.h"

UGA_FireMachineGun::UGA_FireMachineGun()
{
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(TAG_Input_Fire_MachineGun);
    SetAssetTags(AssetTags);
}
