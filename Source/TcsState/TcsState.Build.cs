// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsState : ModuleRules
{
	public TcsState(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（PLN-REFACTOR-1 Task 4 后）：**共享层**——两域共用的 Def 族
		// （`FTcsStateDefBase` / `UTcsStateDef` / `FTcsNumericParamRow` / `FTcsDescriptionEntry` /
		// `TcsStateParamKeys`）+ 通用运行时机制（基底 `FTcsStateInstance` / 快照 / 桶 / 注册表 /
		// 代际发号 / 广播守卫 / 重申请决策接口）+ 共享门面（参数表读口）。
		// **★ 依赖已去掉 `TcsEffect`**（裁定⑫：Buff 专属层连同内联触发行与链步骤已整体搬 `TcsBuff`）
		// ⇒ 共享层对 `TcsEffect` 的 include 为零（可 grep 复核）。
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsNotation",
			"TcsAttribute"
		});
	}
}
