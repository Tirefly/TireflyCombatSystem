// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsNotation : ModuleRules
{
	public TcsNotation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / D5-18 v2）：策划记法底座，与 TcsCore 平级互不依赖，仅引擎基础
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject"
		});
	}
}
