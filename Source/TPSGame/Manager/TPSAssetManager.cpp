#include "TPSAssetManager.h"
#include "AbilitySystemGlobals.h"

UTPSAssetManager& UTPSAssetManager::Get()
{
    UTPSAssetManager* Singleton = Cast<UTPSAssetManager>(GEngine->AssetManager);
    checkf(Singleton, TEXT("AssetManagerClassName이 TPSAssetManager로 설정되지 않았습니다."));
    return *Singleton;
}

void UTPSAssetManager::StartInitialLoading()
{
    Super::StartInitialLoading();
    UAbilitySystemGlobals::Get().InitGlobalData();
}