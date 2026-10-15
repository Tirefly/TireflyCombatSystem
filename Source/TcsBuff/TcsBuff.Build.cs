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
			"TcsState",
			// 主资产加载层（2026-10-15 Task 6 补）：本域状态定义资产自 TcsState 搬入后，
			// 旧的按类路径发现（GetAssetsByClass）静默返 0（实测 State.Run 由 26/0 退为 6/9）。
			// 依赖方向仍是「注册者 → 加载层」朝下，与 TcsEffect 已建的同款边一致。
			"TcsAssetButler"
		});
	}
}
