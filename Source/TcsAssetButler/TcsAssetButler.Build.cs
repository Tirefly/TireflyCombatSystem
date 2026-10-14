// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TcsAssetButler : ModuleRules
{
	public TcsAssetButler(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 最小编译集（PLN-REFACTOR-1 Task 8；DEC-2026-10-13-asset-butler 裁定②）：
		// **零 Tcs 业务依赖**——本模块住得比所有注册者都低，各域（含层级最低的 TcsAttribute）
		// 才能经宏向它**向下注册**。一旦它 include 任一业务 Def 头，就必须住到那些类型之上
		// ⇒ 各域无法再向下注册（本会话已实测该环）。
		// DeveloperSettings：UTcsAssetButlerSettings 的基类 UDeveloperSettings 实际住该模块
		// （头文件路径仍为 Engine/DeveloperSettings.h）——引擎模块，不违反上述红线。
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags",
			// 定义资产按类发现（AssetManager 的扫描走 AssetRegistry；Engine 里那条是过渡期临时依赖，
			// 本模块显式声明，不靠 Engine 转发）
			"AssetRegistry"
		});
	}
}
