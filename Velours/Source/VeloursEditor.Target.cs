// Copyright 2022 wevet works All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class VeloursEditorTarget : TargetRules
{
	public VeloursEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// BuildEnvironment = TargetBuildEnvironment.Unique;
		ExtraModuleNames.AddRange( new string[] { "Velours" } );
	}
}
