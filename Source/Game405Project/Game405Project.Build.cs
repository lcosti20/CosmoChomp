// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Game405Project : ModuleRules
{
	public Game405Project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Game405Project",
			"Game405Project/Variant_Platforming",
			"Game405Project/Variant_Platforming/Animation",
			"Game405Project/Variant_Combat",
			"Game405Project/Variant_Combat/AI",
			"Game405Project/Variant_Combat/Animation",
			"Game405Project/Variant_Combat/Gameplay",
			"Game405Project/Variant_Combat/Interfaces",
			"Game405Project/Variant_Combat/UI",
			"Game405Project/Variant_SideScrolling",
			"Game405Project/Variant_SideScrolling/AI",
			"Game405Project/Variant_SideScrolling/Gameplay",
			"Game405Project/Variant_SideScrolling/Interfaces",
			"Game405Project/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
