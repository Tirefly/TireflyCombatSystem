// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/PrimaryAssetId.h"



/**
 * 按 Def 基类发现主资产的**统一入口**（2026-10-15，PLN-REFACTOR-1 Task 6）。
 *
 * ## 为什么要这个口（一次实测回归的直接产物）
 *
 * 既有的五条 `Discover*` 与宿主装置都走 `IAssetRegistry::GetAssetsByClass(类路径)`。
 * 该路径在**类换过模块**之后**静默失效**：`CoreRedirects` 的 `ClassRedirects` 只在**加载期**
 * 把旧路径映射到新类，**不改写 AssetRegistry 索引里记录的类路径**；而 `GetAssetsByClass` 拿
 * **当前**类路径去查索引 ⇒ 查不到。实测（2026-10-15，无头与编辑器 PIE **两条路线一致**）：
 *
 * | Def 类 | 搬迁史 | `GetAssetsByClass` 命中 |
 * |---|---|---|
 * | `UTcsSkillDef` / `UTcsAttrModDef` | 未搬 | 5 / 1（正常） |
 * | `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` | Task 8 搬（`TcsIntegration`→`TcsEffect`） | **0 / 0** |
 * | `UTcsBuffDefAsset` | Task 4 搬（`TcsState`→`TcsBuff`） | **0** |
 *
 * ⇒ 这是重定向手段的**第四层**：它救**加载**，不救**按类路径的索引查询**
 * （前三层见 `Config/DefaultEngine.ini` 的 `[CoreRedirects]` 注释段）。
 *
 * 本口改走 `UAssetManager::GetPrimaryAssetIdList`——它按 `FPrimaryAssetType` 匹配，
 * **与类路径解耦**，故搬家不影响。类型名由 Def 类的反射名派生（`FTcsPrimaryAssetRegistry::ResolveEntry`
 * 的同一套口径），**零字符串面**。
 *
 * ## 与旧路径的差异（MUST 读，别当成等价替换）
 *
 * ```
 * 旧：GetAssetsByClass(类路径)            —— 直接问 AssetRegistry 索引
 * 新：GetPrimaryAssetIdList(主资产类型)   —— 只覆盖**已登记 + 已被扫描**的类型
 * ```
 *
 * ⇒ **未登记的主资产类型查不到任何资产**。这正是本仓 2026-10-06 的既有顾虑
 * （`TcsDefinitionSubsystem.h` 的"未注册时 AssetManager 按类型查询会静默返回空，排障成本高"）。
 * 本口**显式消解**该顾虑：未登记与已登记但空**分开报告**，前者留 `Warning` 而非静默返回 false。
 *
 * @param DefClass       Def 资产基类（用 `StaticClass()`）；为空直接失败
 * @param OutAssets      输出的资产数据（形态与 `GetAssetsByClass` 一致，便于逐处替换）
 * @param OutDiagnostic  失败/退化原因（供调用方进失败清单；空表示正常）
 * @return 是否至少发现一个资产
 */
class TCSASSETBUTLER_API FTcsPrimaryAssetQuery
{
public:
	/**
	 * 按 Def 基类取出全部主资产。
	 *
	 * @param DefClass       Def 资产基类（用 `StaticClass()`）；为空直接失败
	 * @param OutAssets      输出的资产数据（形态与 `GetAssetsByClass` 一致，便于逐处替换）
	 * @param OutDiagnostic  失败/退化原因（供调用方进失败清单；空表示正常）
	 * @return 是否至少发现一个资产
	 */
	static bool GetAssetsForDefClass(
		UClass* DefClass,
		TArray<FAssetData>& OutAssets,
		FString* OutDiagnostic = nullptr);
};
