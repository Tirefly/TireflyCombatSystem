// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsSkill : ModuleRules
{
	public TcsSkill(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 技能层只编排参数、状态与效果链；领域步骤经执行器注册表分派。
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsNotation",
			"TcsAttribute",
			"TcsEffect",
			"TcsState"
		});
	}
}
