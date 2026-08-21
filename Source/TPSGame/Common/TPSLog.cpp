#include "Common/TPSLog.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY(TPSLog);

namespace TPSNetDebug
{
    FString TPSNetPrefix(const AActor* Actor)
    {
        if (!Actor)
        {
            return TEXT("[null]");
        }

        const TCHAR* AuthStr = Actor->HasAuthority() ? TEXT("SV") : TEXT("CL");

        const TCHAR* RoleStr = NetRoleToString(Actor->GetLocalRole());

        const TCHAR* ModeStr = NetModeToString(Actor->GetNetMode());

        return FString::Printf(TEXT("[%s|%s|%s] %s"),
            AuthStr, RoleStr, ModeStr, *Actor->GetName());
    }

    FString DescribeNet(const AActor* Actor)
    {
        if (!Actor) return TEXT("NoActor");
        return FString::Printf(TEXT("%s|%s"),
            NetModeToString(Actor->GetNetMode()),
            NetRoleToString(Actor->GetLocalRole()));
    }

    double GetSyncedTime(const AActor* Actor)
    {
        if (!Actor) return 0.0;
        const UWorld* World = Actor->GetWorld();
        if (!World) return 0.0;
        if (const AGameStateBase* GS = World->GetGameState())
        {
            return GS->GetServerWorldTimeSeconds();
        }
        return World->GetTimeSeconds();
    }
}