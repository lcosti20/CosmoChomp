// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BurnPhase : ModuleRules
{
	public BurnPhase(ReadOnlyTargetRules Target) : base(Target)
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
			"BurnPhase",
			"BurnPhase/Variant_Platforming",
			"BurnPhase/Variant_Platforming/Animation",
			"BurnPhase/Variant_Combat",
			"BurnPhase/Variant_Combat/AI",
			"BurnPhase/Variant_Combat/Animation",
			"BurnPhase/Variant_Combat/Gameplay",
			"BurnPhase/Variant_Combat/Interfaces",
			"BurnPhase/Variant_Combat/UI",
			"BurnPhase/Variant_SideScrolling",
			"BurnPhase/Variant_SideScrolling/AI",
			"BurnPhase/Variant_SideScrolling/Gameplay",
			"BurnPhase/Variant_SideScrolling/Interfaces",
			"BurnPhase/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
