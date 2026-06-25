// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TeamCarry : ModuleRules
{
	public TeamCarry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "CommonUI", "CommonInput", "GameplayTags", "CatchCharacter",
            "AIModule",         
	        "GameplayTasks",     
	        "NavigationSystem" });

        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });


        PublicIncludePaths.AddRange(new string[] 
        { 
            "TeamCarry",
        });

        // 모듈 루트(Source/TeamCarry)를 include 경로에 추가.
        // V7 빌드 세팅 + flat 레이아웃에서는 모듈 루트가 자동 등록되지 않아먀
        // "Level/Struct/BreakableProp.h" 같은 모듈루트 기준 include가 실패한다.
        PublicIncludePaths.Add(ModuleDirectory);


        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
