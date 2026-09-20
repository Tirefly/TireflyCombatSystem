// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsEffect : ModuleRules
{
	public TcsEffect(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / plan2 Task 0）：效果链解释器与执行器注册表（M4b）——依赖 Core + Attribute
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsAttribute"
		});
	}
}
