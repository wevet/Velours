// Copyright 2022 wevet works All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class VeloursTarget : TargetRules
{
	public VeloursTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		CppStandard = CppStandardVersion.Cpp20;

		// 異なるオブジェクトファイル間での最適化
		bAllowLTCG = true;

		bPGOProfile = false;
		bPGOOptimize = false;

#if false
		if (Target.Configuration == UnrealTargetConfiguration.Test)
		{
			// 計測パッケージ作成時
			bPGOProfile = true;
			// PGO適用時
			bPGOOptimize = true;
		}
#endif
		ExtraModuleNames.AddRange( new string[] { "Velours" } );
	}
}
