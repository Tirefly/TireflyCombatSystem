// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsDamage : ModuleRules
{
	public TcsDamage(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / plan2 Task 0）：瞬时伤害流程（解释器/黑板/标准步骤库）——依赖 Core + Attribute + Effect；不内嵌 selector（目标消费 Context.Targets，D4-4 v2）
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsAttribute",
			"TcsEffect"
		});
	}
}
