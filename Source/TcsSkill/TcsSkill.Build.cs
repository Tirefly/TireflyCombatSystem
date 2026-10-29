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
			"TcsState",
			// 主资产类型自注册（2026-10-26）：UTcsSkillDef 在自己的 .cpp 里
			// UE_DEFINE_PRIMARY_ASSET_TYPE，需要该宏与 FTcsPrimaryAssetEntry 的定义。
			// 依赖方向合法（AssetButler 住在所有域之下、零 Tcs 业务依赖），不成环。
			"TcsAssetButler"
		});
	}
}
