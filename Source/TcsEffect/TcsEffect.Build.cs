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
			"TcsAttribute",
			// 资产加载层（PLN-REFACTOR-1 Task 8）：本模块收下的两个 Def 资产类经
			// UE_DEFINE_PRIMARY_ASSET_TYPE **向下注册**——"注册者 → 加载层"的边，方向朝下（裁定②）
			"TcsAssetButler"
		});
	}
}