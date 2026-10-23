// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsAttribute : ModuleRules
{
	public TcsAttribute(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / D5-18 v2）：引擎基础 + TcsCore + TcsNotation（修正器模板约定列与物化转换）
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsNotation",
			// 主资产加载层（2026-10-17 补，ARCH-04 D-2）：本域两个 Def 资产类经
			// UE_DEFINE_PRIMARY_ASSET_TYPE **向下注册**——"注册者 → 加载层"的边，方向朝下，
			// 与 TcsEffect / TcsBuff 已建的同款边一致（TcsAssetButler 是层 0，无回边，不成环）。
			// ⚠ 补上这条边只是**迁移前的硬前置**：`FTcsPrimaryAssetQuery` 对未登记类型直接返回
			// 失败 ⇒ 任何想改走新统一口的属性域发现路径 MUST 先有本注册（顺序 注册 → 迁移）。
			"TcsAssetButler"
		});
	}
}
