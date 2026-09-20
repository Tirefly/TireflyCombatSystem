// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsTargeting : ModuleRules
{
	public TcsTargeting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / plan2 Task 0）：目标选择策略契约与默认实现（M4c 的 R3 切片）——依赖 Core + Effect
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsEffect"
		});
	}
}
