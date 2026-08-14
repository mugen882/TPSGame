#include "AbilitySystem/GA/GA_FireRocketLauncher.h"
#include "Common/TPSGameplayTags.h"

UGA_FireRocketLauncher::UGA_FireRocketLauncher()
{
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(TAG_Input_Fire_RocketLauncher);
    SetAssetTags(AssetTags);
}