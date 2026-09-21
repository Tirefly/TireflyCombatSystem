// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "TcsDefinitionSubsystem.generated.h"



class UTcsEffectChainDef;
struct FTcsEffectChain;



/**
 * 定义库（**概念名 DefLibrary**，沿用设计词汇）——M1 的宿主，**MUST 为 GameInstance 级**：
 * Const 定义内容跨 PIE 世界共享，图鉴/UI 等**无世界**场景要能查（`MEM-20260902-05` 生命周期分层判据：
 * 先枚举消费场景含无世界者，再定级别）。
 *
 * 职责边界（**只做资产发现与注册，不做执行**——执行归各领域门面）：
 * - **发现**：`IAssetRegistry::GetAssetsByClass` 按类扫描（**不依赖 `PrimaryAssetTypesToScan` 注册**
 *   ——该注册属 M6 轮，且未注册时 AssetManager 按类型查询会**静默返回空**，排障成本高，见台账 R7-3）；
 * - **校验**：双真相（`Chain.ChainId != ChainId`）/ 空 id / 重复登记 → 计入失败清单 + Error，不静默跳过；
 * - **按名解析**：链定义按 `ChainId` 缓存（`ResolveChain`）；
 * - **就绪状态机**：`OnDefinitionsReady` **单出口**（幂等）——M6 双层引导的最小版（06 §4 硬规则①）；
 * - **装配到世界**：见下条。
 *
 * **装配到世界（跨级方向纪律，2026-09-21）**：本子系统是 GameInstance 级（跨世界存活），而定义**消费方**
 * （`UTcsEffectSubsystem`）是 World 级（每世界重建）——故定义 MUST 在本类**缓存**，并在**每个世界初始化时**
 * 装配进该世界（订阅 `FWorldDelegates::OnPostWorldInitialization`，幂等）。"ready 时对当时的 World 登记一次"
 * 会在关卡切换后丢失全部登记（R3 单地图 PIE 测不出，宿主首次切关卡必撞）。时机已核：`UWorld::InitWorld`
 * 中 `InitializeSubsystems()`（`World.cpp:2447`）**早于** `OnPostWorldInitialization.Broadcast`（`:2601`），
 * 故装配时 EffectSubsystem 已可用。
 *
 * **MUST NOT 持可变运行态**（Const 面：缓存的是定义，不是运行态）。
 *
 * 与中央注册表的分工（**不重叠**，三层结构）：本类 = Const 定义（"有哪些定义、定义是什么"）；
 * 各领域子系统登记表 = 定义的运行期副本（"当前世界有哪些定义可用"）；中央注册表
 * （`UCombatWorldRegistrySubsystem`，M3/M6）= 可变运行态 per-unit 桶（"当前世界有哪些实体"）。
 * 门禁关系：中央注册表查本类 `IsRuntimeReady()`。
 */
UCLASS()
class TCSINTEGRATION_API UTcsDefinitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 初始化：发现定义资产 → 校验 → 缓存 → 标记就绪 → 单出口派发 → 装配到世界（若世界已存在）
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 反初始化：退订世界初始化委托，清空缓存与失败清单（确定性清理）
	virtual void Deinitialize() override;

#pragma endregion


// 就绪
#pragma region Readiness

public:
	/**
	 * 定义是否已就绪（**门禁查询点**：实体组件注册前查它）。
	 * 就绪 = 发现与校验已完成（失败清单非空不影响就绪——失败项被跳过、其余可用；
	 * 宿主可经 `GetFailureList` 决定是否放行）。
	 *
	 * @return 返回是否已就绪。
	 */
	bool IsRuntimeReady() const
	{
		return bRuntimeReady;
	}

	/**
	 * 取失败清单（资产路径 + 失败原因）——供宿主诊断与透传（06 §4 硬规则②"失败清单透传"）。
	 *
	 * @return 返回失败条目列表。
	 */
	const TArray<FString>& GetFailureList() const
	{
		return FailureList;
	}

#pragma endregion


// 解析
#pragma region Resolve

public:
	/**
	 * 按 id 解析已缓存的链定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * @param ChainId 链 id。
	 * @return 返回链定义；未缓存返回 nullptr。
	 */
	const FTcsEffectChain* ResolveChain(FName ChainId) const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 发现并加载全部链资产（扫描 → 逐资产校验 → 入缓存；失败项进清单）
	void DiscoverChainDefs();

	// 装配已缓存定义进指定世界（幂等：同一世界不重复登记）
	void SeedWorld(UWorld* World);

	// 世界初始化回调（新世界装配入口）
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	// 已装配的世界（幂等判据——同一世界不重复装配）
	TWeakObjectPtr<UWorld> SeededWorld;

	// 链定义缓存（Const 内容；键 = ChainId）——`TUniquePtr` 持有使解析返回的指针地址稳定
	TMap<FName, TUniquePtr<FTcsEffectChain>> ChainDefs;

	// 定义资产缓存（GC 锚定：缓存的是定义内容，但资产对象本身也需存活以免重载）
	UPROPERTY()
	TArray<TObjectPtr<UTcsEffectChainDef>> ChainDefAssets;

	// 失败清单（资产路径 + 原因）
	TArray<FString> FailureList;

	// 就绪标记（`OnDefinitionsReady` 的幂等依据）
	bool bRuntimeReady = false;

	// 世界初始化委托句柄（反初始化时退订）
	FDelegateHandle WorldInitDelegateHandle;

#pragma endregion
};
