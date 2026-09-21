// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsIntegration : ModuleRules
{
	public TcsIntegration(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（R0 §9 / plan2 Task 0）：CombatEntity 接线与定义加载（M6 横切）——依赖其下全部战斗模块
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"TcsCore",
			"TcsAttribute",
			"TcsEffect",
			"TcsTargeting",
			"TcsDamage",
			// 定义资产发现（UTcsDefinitionSubsystem 按类扫描）——Engine 里的那条是过渡期临时依赖
			// （Engine.Build.cs:98 "Here until we update all modules using AssetRegistry to add a dependency on it"），
			// 本模块显式声明，不靠 Engine 转发
			"AssetRegistry"
		});
	}
}
