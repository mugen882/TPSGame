#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_TargetActor.generated.h"

/*
    블랙보드에 저장된 타깃 액터를 EQS 쿼리의 기준점으로 넘겨주는 클래스
*/
UCLASS()
class TPSGAME_API UEnvQueryContext_TargetActor : public UEnvQueryContext
{
    GENERATED_BODY()

public:
    virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};