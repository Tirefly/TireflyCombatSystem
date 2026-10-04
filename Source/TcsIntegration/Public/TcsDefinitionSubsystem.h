// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Handle/TcsSourceHandle.h"

#include "TcsDefinitionSubsystem.generated.h"



class FReferenceCollector;
class UTcsBuffDefAsset;
class UTcsEffectChainDef;
class UTcsEffectTriggerDefAsset;
struct FTcsBuffDef;
struct FTcsEffectChain;
struct FTcsEffectTriggerDef;



/**
 * 定义库（**概念名 DefLibrary**，沿用设计词汇）——M1 的宿主，**MUST 为 GameInstance 级**：
 * Const 定义内容跨 PIE 世界共享，图鉴/UI 等**无世界**场景要能查（`MEM-20260902-05` 生命周期分层判据：
 * 先枚举消费场景含无世界者，再定级别）。
 *
 * 职责边界（**只做资产发现与注册，不做执行**——执行归各领域门面）：
 * - **发现**（**三条按类路径**）：链资产 `UTcsEffectChainDef`（`DiscoverChainDefs`）、触发定义资产
 *   `UTcsEffectTriggerDefAsset`（`DiscoverTriggerDefs`）、状态定义资产 `UTcsBuffDefAsset`
 *   （`DiscoverStateDefs`，**2026-10-04 R5 Task 1**）——均走 `IAssetRegistry::GetAssetsByClass`
 *   （**不依赖 `PrimaryAssetTypesToScan` 注册**——该注册属 M6 轮，且未注册时 AssetManager 按类型
 *   查询会**静默返回空**，排障成本高，见台账 INTEG-3）；
 * - **校验**：双真相（`Chain.ChainId != ChainId`）/ 空 id / 重复登记 → 计入失败清单 + Error，不静默跳过；
 *   触发定义侧另有四条（空 `TriggerTag` / 空 `Def.EventTag` / 空 `Def.EffectChainId` / 身份重复）；
 *   **状态定义侧的四条是身份级**（加载失败或类型不符 / 空 `DefTag` / 空 `Def.StatusTag` / `DefTag` 重复）
 *   ——内容级规则（参数键重复、内联触发行缺项、时值来源为空、描述缺项）全归资产 `IsDataValid`，
 *   发现期只判"这条资产能不能被按 tag 寻址"；
 * - **按名解析**：链定义按 `ChainId` 缓存（`ResolveChain`）、触发定义按 `TriggerTag` 缓存
 *   （`ResolveTriggerDef`）、状态定义按 `DefTag` 缓存（`ResolveStateDef`）；
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
 * **触发行装配（2026-10-04）**：链装配**之后**，每条触发定义经 `UTcsEffectSubsystem::RegisterTriggerRow`
 * 登记成该世界的触发行，`Source` = 本类自持的来源句柄（`Initialize` 时发放一次）——`effect-trigger` 规格的
 * "定义加载期登记（全局常驻规则）"由此成立。引用链在该世界不可解析时留 Warning 并**仍登记**
 * （宿主可在运行期自行 `RegisterChain`，"跳过登记"会让该规则永久静默失效）。
 *
 * **状态定义不做世界装配（2026-10-04 R5 Task 1）**：状态定义只进缓存、由 `UTcsStateSubsystem` 按需解析，
 * MUST NOT 在 `OnPostWorldInitialization` 里为它登记任何每世界结构——链与触发定义有"每世界登记一次"的语义，
 * 状态定义没有（故 `SeedWorld` 的"无内容即返回"判据仍只看前两者）。
 *
 * **MUST NOT 持可变运行态**（Const 面：缓存的是定义，不是运行态；触发行本身住世界级子系统）。
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


// GC 引用收集
#pragma region GC

public:
	/**
	 * GC 引用收集（**三份定义缓存的 GC 可见持有**，2026-10-04）——**必需**。
	 *
	 * 为什么必须自己实现：`ChainDefs` / `TriggerDefs` / `StateDefs` 都是本类的**非 `UPROPERTY` 成员**
	 * （`TMap<FGameplayTag, TUniquePtr<...>>`），GC 的 `RefLink` 遍历**走不到它们**；而三份缓存的内容里
	 * 都有 `FInstancedStruct`（链 = `Steps`、触发定义 = `Conditions` / `EventPayloadFilter`、
	 * 状态定义 = 参数行的数值来源 / 描述的视图载荷），其**内层内存可以放宿主自定义 struct 的
	 * `UPROPERTY` 对象引用**（D4-16 类型不设限）⇒ 不补引用即**静默回收**
	 * （表现为"步骤/条件/参数行里那个对象变成空引用"，不是崩溃）。
	 *
	 * **判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——计划注记曾以"值语义 struct ⇒ 无需 ARO"
	 * 为由判断不需要，该判据是错的（同 `FTcsTriggerRegistry::AddReferencedObjects` 的纠正）。
	 *
	 * 手法与 `UTcsEffectSubsystem::AddReferencedObjects` 完全同款：逐条调
	 * `FReferenceCollector::AddPropertyReferencesWithStructARO`（引擎 `UDataTable::AddReferencedObjects`
	 * 对 `RowMap` 用的同一招，`DataTable.cpp:300`）。
	 *
	 * @param InThis 本对象（引擎静态 ARO 签名约定，须自行 Cast）。
	 * @param Collector GC 引用收集器。
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

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
	const FTcsEffectChain* ResolveChain(FGameplayTag ChainId) const;

	/**
	 * 按身份解析已缓存的触发定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * 与 `ResolveChain` 同款：返回**缓存内容的裸指针**（`TUniquePtr` 持有使地址稳定），
	 * 故无反射面（UHT 不支持 struct 指针作反射返回）——宿主脚本层的登记入口是
	 * `UTcsEffectSubsystem::RegisterTriggerRow`（它自己带定义数据，不需要本方法）。
	 *
	 * @param TriggerTag 触发身份（`EffectTriggerDef` 根）。
	 * @return 返回触发定义；未缓存返回 nullptr。
	 */
	const FTcsEffectTriggerDef* ResolveTriggerDef(FGameplayTag TriggerTag) const;

	/**
	 * 按身份解析已缓存的状态定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * 与 `ResolveTriggerDef` 同款（裸指针 + 不 ensure）；状态定义的消费者是 `UTcsStateSubsystem`
	 * 的施加路径（R5 Task 2 起）——**本类不做世界装配**，状态定义在哪个世界用由消费方按需取。
	 *
	 * @param DefTag 定义身份（状态词表根）。
	 * @return 返回 buff 定义；未缓存返回 nullptr。
	 */
	const FTcsBuffDef* ResolveStateDef(FGameplayTag DefTag) const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 发现并加载全部链资产（扫描 → 逐资产校验 → 入缓存；失败项进清单）
	void DiscoverChainDefs();

	// 发现并加载全部触发定义资产（扫描 → 逐资产四校验 → 按身份去重 → 入缓存；失败项进清单）
	void DiscoverTriggerDefs();

	// 发现并加载全部状态定义资产（扫描 → 逐资产四校验 → 按身份去重 → 入缓存；失败项进清单）
	void DiscoverStateDefs();

	// 装配已缓存定义进指定世界（幂等：同一世界不重复登记；链在前、触发行在后；状态定义不参与）
	void SeedWorld(UWorld* World);

	// 世界初始化回调（新世界装配入口）
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	// 已装配的世界（幂等判据——同一世界不重复装配）
	TWeakObjectPtr<UWorld> SeededWorld;

	// 链定义缓存（Const 内容；键 = ChainId）——`TUniquePtr` 持有使解析返回的指针地址稳定
	TMap<FGameplayTag, TUniquePtr<FTcsEffectChain>> ChainDefs;

	// 触发定义缓存（Const 内容；键 = TriggerTag）——持有形态与失效判据同链缓存
	TMap<FGameplayTag, TUniquePtr<FTcsEffectTriggerDef>> TriggerDefs;

	// 状态定义缓存（Const 内容；键 = DefTag）——持有形态与失效判据同上
	TMap<FGameplayTag, TUniquePtr<FTcsBuffDef>> StateDefs;

	// 定义资产缓存（GC 锚定：缓存的是定义内容，但资产对象本身也需存活以免重载）
	UPROPERTY()
	TArray<TObjectPtr<UTcsEffectChainDef>> ChainDefAssets;

	// 触发定义资产缓存（GC 锚定，理由同上）
	UPROPERTY()
	TArray<TObjectPtr<UTcsEffectTriggerDefAsset>> TriggerDefAssets;

	// 状态定义资产缓存（GC 锚定，理由同上）
	UPROPERTY()
	TArray<TObjectPtr<UTcsBuffDefAsset>> StateDefAssets;

	// 来源句柄分配器（进程内原子发号；本类只在自己 `Initialize` 时用一次）
	FTcsSourceHandleRegistry TriggerSourceRegistry;

	// 触发行来源句柄（装配期登记触发行时的 `Source`——"定义库来源"这一级联退订锚点）
	FTcsSourceHandle TriggerSeedSource;

	// 失败清单（资产路径 + 原因）
	TArray<FString> FailureList;

	// 就绪标记（`OnDefinitionsReady` 的幂等依据）
	bool bRuntimeReady = false;

	// 世界初始化委托句柄（反初始化时退订）
	FDelegateHandle WorldInitDelegateHandle;

#pragma endregion
};
