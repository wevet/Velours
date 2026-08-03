// Copyright 2022 wevet works All Rights Reserved.

using UnrealBuildTool;

public class VeloursEditor : ModuleRules
{
	public VeloursEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bLegacyPublicIncludePaths = true;
		OverridePackageType = PackageOverrideType.GameUncookedOnly;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		CppStandard = CppStandardVersion.Cpp20;


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"Engine",
				"UnrealEd",
				"PropertyEditor",
				"AnimGraphRuntime",
				"AnimGraph",
				"GraphEditor",
				"BlueprintGraph",
				"DesktopPlatform",
				"AnimationModifiers",
				"AnimationBlueprintLibrary",
				"InputCore",
				"Velours",
				"RHI",
				"Landscape",
				"LevelEditor",
				"PlacementMode",
				"DeveloperSettings",
				"EditorScriptingUtilities",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorStyle",
				"AssetRegistry",
				"Slate",
				"SlateCore",
				"AssetTools",
				"EditorStyle",
			}
		);

	}
}
