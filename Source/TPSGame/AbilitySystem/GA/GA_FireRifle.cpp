#include "AbilitySystem/GA/GA_FireRifle.h"
#include "Common/TPSGameplayTags.h"

UGA_FireRifle::UGA_FireRifle()
{
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(TAG_Input_Fire_Rifle);
    SetAssetTags(AssetTags);
}