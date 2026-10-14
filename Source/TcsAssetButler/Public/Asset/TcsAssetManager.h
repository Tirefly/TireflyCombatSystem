// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"

#include "TcsAssetManager.generated.h"



/**
 * TCS 主资产管理器（裁定① / E4）。
 *
 * 宿主只需在**项目级** ini 写一行（E1：`AssetManagerClassName` 是 `globalconfig`，
 * 插件设不了、只有项目 ini 能设）：
 * ```
 * [/Script/Engine.Engine]
 * AssetManagerClassName=/Script/TcsAssetButler.TcsAssetManager
 * ```
 * 类名无效时引擎是 **`Fatal`**（E2，`UnrealEngine.cpp:3826`），不是静默回退——故本类所在模块的
 * `LoadingPhase` 必须先于该解析点（见 §6 T3 实测）。
 *
 * **扫描绕开 ini**（E4 + 裁定⑤）：类型清单来自各域代码自注册
 * （`UE_DEFINE_PRIMARY_ASSET_TYPE`），**MUST NOT 用 ini 提供**——类型名走 `StaticClass()`，
 * 零字符串面，规避"改名漏改 ini"陷阱。
 *
 * **与基类的关系**：`Super::StartInitialLoading()` 内跑 `ScanPrimaryAssetTypesFromConfig()`
 * （读 `UAssetManagerSettings` 的 `PrimaryAssetTypesToScan`）并广播
 * `OnAssetManagerCreatedDelegate`；本类在其**之后**补登记表驱动的扫描。两条来源共用引擎
 * 同一张 `AssetTypeMap`，故若同一个 `PrimaryAssetType` 两边都配，引擎会 ensure
 * （`ScanPathsForPrimaryAssets` 的类型参数校验）——本仓宿主 ini 保持零 `PrimaryAssetTypesToScan`。
 */
UCLASS()
class TCSASSETBUTLER_API UTcsAssetManager : public UAssetManager
{
	GENERATED_BODY()

// 启动扫描
#pragma region InitialLoading

public:
	/**
	 * 覆写引擎启动扫描（E3）：`Super::` 之后遍历本仓注册表补扫。
	 *
	 * **时序已实测**（§6 T1，见实施记录）：本函数在 `UEngine::InitializeObjectReferences()`
	 * 内被调用，而 `AssetManager->FinishInitialLoading()`（广播 `OnCompletedInitialScan` 处）
	 * 在同一次 `GEngine->Init()` 的**更晚**位置 ⇒ 补扫不会延误初始扫描完成广播。
	 * 且 `Super::` 已 `GInitialBulkScan.StartOnce(this)`（bulk scan 窗口开到 `OnPostEngineInit`）
	 * ⇒ 补扫落在同一批次内，不会错过 bulk scan。
	 */
	virtual void StartInitialLoading() override;

#pragma endregion
};
