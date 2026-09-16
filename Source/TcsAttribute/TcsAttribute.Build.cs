// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsAttribute : ModuleRules
{
	public TcsAttribute(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / D5-18 v2）：引擎基础 + TcsCore + TcsNotation（修正器模板约定列与物化转换）
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsNotation"
		});
	}
}
