#include "Common/TPSLog.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(TPSLog);

FString TPSNetPrefix(const AActor* Actor)
{
    if (!Actor)
    {
        return TEXT("[null]");
    }

    const TCHAR* AuthStr = Actor->HasAuthority() ? TEXT("SV") : TEXT("CL");

    const TCHAR* RoleStr = TEXT("?");
    switch (Actor->GetLocalRole())
    {
    case ROLE_Authority:       RoleStr = TEXT("Authority");  break;
    case ROLE_AutonomousProxy: RoleStr = TEXT("Autonomous"); break;
    case ROLE_SimulatedProxy:  RoleStr = TEXT("Simulated");  break;
    case ROLE_None:            RoleStr = TEXT("None");       break;
    default: break;
    }

    const TCHAR* ModeStr = TEXT("?");
    switch (Actor->GetNetMode())
    {
    case NM_Standalone:      ModeStr = TEXT("Standalone"); break;
    case NM_DedicatedServer: ModeStr = TEXT("DedServer");  break;
    case NM_ListenServer:    ModeStr = TEXT("Listen");     break;
    case NM_Client:          ModeStr = TEXT("Client");     break;
    default: break;
    }

    return FString::Printf(TEXT("[%s|%s|%s] %s"),
        AuthStr, RoleStr, ModeStr, *Actor->GetName());
}