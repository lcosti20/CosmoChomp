// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CosmoChomp : ModuleRules
{
	public CosmoChomp(ReadOnlyTargetRules Target) : base(Target)
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
			"CosmoChomp",
			"CosmoChomp/Variant_Platforming",
			"CosmoChomp/Variant_Platforming/Animation",
			"CosmoChomp/Variant_Combat",
			"CosmoChomp/Variant_Combat/AI",
			"CosmoChomp/Variant_Combat/Animation",
			"CosmoChomp/Variant_Combat/Gameplay",
			"CosmoChomp/Variant_Combat/Interfaces",
			"CosmoChomp/Variant_Combat/UI",
			"CosmoChomp/Variant_SideScrolling",
			"CosmoChomp/Variant_SideScrolling/AI",
			"CosmoChomp/Variant_SideScrolling/Gameplay",
			"CosmoChomp/Variant_SideScrolling/Interfaces",
			"CosmoChomp/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
