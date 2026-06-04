// Copyright 2022 wevet works All Rights Reserved.

using UnrealBuildTool;

public class Velours : ModuleRules
{
	public Velours(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = true;
		bUseUnity = false;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Default;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreOnline",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"AIModule",
				"NavigationSystem",
				"PhysicsCore",
				"DataRegistry",
				"ReplicationGraph",
				"Json",
				"AnimationBudgetAllocator", // budget
				"SignificanceManager", // budget
				"ChaosVehicles", // vehicle system
				"PoseSearch",
				"AnimGraphRuntime",
				//"AnimationWarping",
				"AnimationWarpingRuntime",
				"ControlRig",
				"Chooser",
				"MotionWarping",
				"Chaos",
				"ChaosVehicles",
				"NetCore",
				"ChaosVehiclesCore",
				"StateTreeModule",
				"GameplayStateTreeModule",
				"SkeletalMerging",
				"HairStrandsCore",
				"MassActors",
				"HairStrandsCore",
				"ContextualAnimation",
				"Media",
				"MediaUtils",
				"MediaAssets",
				"AudioMixer",
				"GeometryCollectionEngine",
				"Gauntlet",
				"MassZoneGraphNavigation",
				"Niagara",
				"StateTreeModule",
				// custom plugin
				"WvAbilitySystem",
				"Mover",
				"NetworkPrediction",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"Slate",
				"SlateCore",
				"RenderCore",
				"DeveloperSettings",
				"EnhancedInput",
				"NetCore",
				"RHI",
				"Projects",
				"Gauntlet",
				"UMG",
				"CommonUI",
				"CommonInput",
			}
		);

		// For non-shipping build.
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			//	PublicDependencyModuleNames.AddRange
			//	(
			//		new string[]
			//		{
			//			"ImGui",
			//			"ApplicationCore",
			//		}
			//	);
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}

		//if (Target.Type == TargetType.Editor)
		//{
		//	PrivateDependencyModuleNames.AddRange
		//	(
		//		new string[]
		//		{
		//			"UnrealEnginePython",
		//			"UnrealEd",
		//			"SequenceRecorder",
		//		}
		//	);
		//}

		// Generate compile errors if using DrawDebug functions in test/shipping builds.
		PublicDefinitions.Add("SHIPPING_DRAW_DEBUG_ERROR=1");
	}
}
