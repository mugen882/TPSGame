#pragma once
#include "Engine/AssetManager.h"
#include "TPSAssetManager.generated.h"

UCLASS()
class TPSGAME_API UTPSAssetManager : public UAssetManager
{
    GENERATED_BODY()
public:
    static UTPSAssetManager& Get();
    virtual void StartInitialLoading() override;
};