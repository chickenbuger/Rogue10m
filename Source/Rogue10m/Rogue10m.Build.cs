// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Rogue10m : ModuleRules
{
	public Rogue10m(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"NavigationSystem",
			"UMG",
			"Slate",
			"SlateCore",
			"IKRig",
			"AnimationCore",
			"Niagara"
		});
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "SkeletalMeshDescription", "UnrealEd" });
		}
		PublicIncludePaths.AddRange(new string[] {
			"Rogue10m",
			"Rogue10m/Ability",
			"Rogue10m/Core",
			"Rogue10m/Character",
			"Rogue10m/Components",
			"Rogue10m/Data",
			"Rogue10m/Enemy",
			"Rogue10m/UI",
			"Rogue10m/UI/Widgets",
			"Rogue10m/World"
		});
	}
}
