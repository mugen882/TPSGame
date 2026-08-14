// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TPSGame : ModuleRules
{
	public TPSGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Niagara",
			"AIModule",
			"NavigationSystem",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
        });

        PublicIncludePaths.Add(ModuleDirectory);
    }
}
