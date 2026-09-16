// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsCore : ModuleRules
{
	public TcsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9）：仅引擎基础，禁依赖任何兄弟/上层 Tcs 模块
		// DeveloperSettings：UDeveloperSettings 实际住该模块（头文件路径仍为 Engine/DeveloperSettings.h）
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags"
		});
	}
}
