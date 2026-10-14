// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsBuff : ModuleRules
{
	public TcsBuff(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（PLN-REFACTOR-1 Task 4）：Buff 专属域——定义资产 / 五轴堆叠 / 时值周期 /
		// 行为片段 / 施加与移除门面（自 TcsState 拆出，2026-10-14）。
		// 依赖链第五层，与 TcsSkill 平级：
		// TcsState（共享层：两域共用的 Def 族 + 通用运行时机制 + 三组跨域读口）、
		// TcsEffect（链步骤落点：FTcsStepApplyState 与 FTcsBuffDef::Triggers 的内联触发行类型）。
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
