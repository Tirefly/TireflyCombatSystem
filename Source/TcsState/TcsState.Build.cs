// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsState : ModuleRules
{
	public TcsState(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / PLN-R5 Task 1）：M3 状态层（Def 形状与资产、per-unit 状态桶、生命周期事件）
		// ——依赖链第四层，与 TcsDamage / TcsTargeting 平级：
		// TcsCore（句柄/池/总线/时钟/来源）、TcsNotation（参数行值约定列）、TcsAttribute（修正器模板引用）、
		// TcsEffect（内联触发行类型 FTcsEffectTriggerDef）
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsNotation",
			"TcsAttribute",
			"TcsEffect"
		});
	}
}
