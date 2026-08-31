// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/*
	데디케이티드 서버 타겟.

	PIE의 "Play As Client"도 데디케이티드 서버를 띄우지만 완전한 데디케이티드가 아니다.
	Run Under One Process면 서버 월드가 에디터 프로세스 안에서 돌아 렌더링 리소스가
	살아 있고, WITH_EDITOR 코드가 존재하며, 에셋이 쿠킹되지 않은 상태다.

	이 타겟으로 패키징해야 실제 배포 형태에서만 드러나는 문제를 잡을 수 있다.
	  - 애님 노티파이가 발생하지 않아 적이 사격하지 않는 문제
	  - 서버 빌드에서만 나는 컴파일 에러 (에디터 전용 헤더 참조 등)
	  - 쿠킹 차이 (서버는 텍스처/머티리얼/사운드를 대부분 제외한다)

	빌드 후 GenerateProjectFiles를 다시 실행해야 VS에 TPSGameServer 구성이 나타난다.
*/
public class TPSGameServerTarget : TargetRules
{
	public TPSGameServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		ExtraModuleNames.AddRange(new string[] { "TPSGame" });
	}
}
