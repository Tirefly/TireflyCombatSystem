// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventBus/TcsEventBus.h"
#include "GameplayTagContainer.h"

#include "Trigger/TcsEffectTriggerInstance.h"



class UTcsEventBusSubsystem;
class UTcsTriggerEvaluator;



/**
 * 触发行登记表（M4a 订阅侧；**纯逻辑类**，非 UObject——与 `FTcsEventBus` 同款分工：
 * 内核住纯 C++ 类，UObject 门面只做反射面与生命周期）。
 *
 * **职责**：行数据的持有（值语义）+ 按 `Def.EventTag` 的总线订阅装配（**计数配对**）
 * + 行级点灯集 + 求值期随机流。求值逻辑不住这里（住 `UTcsTriggerEvaluator`）。
 *
 * **订阅计数配对（核心纪律）**：同一 `EventTag` 的多行**共用一条订阅**——首次出现该 Tag 时
 * 订阅一次，该 Tag 的行数归零时才退订。**MUST NOT** 每行各订一次：那会让总线订阅表随行数膨胀
 * 且退订易漏（漏一条 = 总线留一个永不命中的死订阅，且求值器被弱引用持着）。
 * 行数经**扫描登记表**得出（登记是加载期动作、行数在数十量级——O(N) 扫描比维护一份
 * 会与真相同步漂移的行句柄索引表更可靠）。
 *
 * **持有形态（值语义 `TArray` + 空闲槽位表 + 每槽代际）**：
 * - 触发行无高频增删、无挂起语义，故**不引入 `TTcsInstancePool` 类型**（池的挂起锚与占用统计
 *   在此是零收益的复杂度）；
 * - **但槽位复用 MUST 配代际校验**：摘除后槽位可被复用，若无代际则**陈旧句柄会静默改指另一行**
 *   （`UnregisterTriggerRow(旧句柄)` 摘掉无辜的行）。故本表自持代际计数并恒填进 `Self.Generation`
 *   ——`TTcsInstanceHandle` 携带 `Generation` 段，恒填 0 等于让类型对自身语义说谎；
 * - **代价**：`TArray` 扩容会搬移元素地址，故**MUST NOT 跨帧持有实例指针**——求值器每次按句柄
 *   重解析（与链解释器"每步入器前重解析"同款纪律）。
 *
 * 单游戏线程访问（D0-4）。
 */
class TCSEFFECT_API FTcsTriggerRegistry
{
// 装配
#pragma region Setup

public:
	/**
	 * 装配共享求值器（订阅的 Handler）。由门面在创建求值器后调用一次——
	 * **本表只持弱引用**（与总线订阅表同款），求值器的强引用归门面 `UPROPERTY`。
	 *
	 * @param InEvaluator 求值器（门面持有其生命周期）。
	 */
	void SetEvaluator(UTcsTriggerEvaluator* InEvaluator);

#pragma endregion


// 登记
#pragma region Registration

public:
	/**
	 * 登记一行触发行（调用方填 `Def` 与 `Source`；`Self` 由本表填入）。
	 *
	 * 拒绝面（调用方 ensure + 返回无效句柄）：`Def.EventTag` 或 `Def.EffectChainId` 无效。
	 * 总线不可得（世界拆解期）时拒绝 + Warning（不 ensure——那是时序而非配置错误）。
	 *
	 * @param Instance 触发实例（`Def` + `Source`；`Self` 被本表覆写）。
	 * @param Bus 事件总线门面（订阅装配用；可为空 = 拒绝登记）。
	 * @return 返回行句柄；拒绝时返回无效句柄。
	 */
	FTcsEffectTriggerHandle RegisterRow(const FTcsEffectTriggerInstance& Instance, UTcsEventBusSubsystem* Bus);

	/**
	 * 摘除单行（**先校验代际**——失配即拒 + 返回 false，不 ensure：陈旧句柄是正常竞态）。
	 *
	 * @param Handle 行句柄。
	 * @param Bus 事件总线门面（订阅计数归零时退订；可为空 = 只摘行不退订）。
	 * @return 返回是否摘除成功。
	 */
	bool UnregisterRow(FTcsEffectTriggerHandle Handle, UTcsEventBusSubsystem* Bus);

	/**
	 * 按来源全量摘除（级联退订锚点，与 M2 `RemoveBySource` 同款语义）。
	 *
	 * @param Source 来源句柄。
	 * @param Bus 事件总线门面。
	 * @return 返回摘除的行数。
	 */
	int32 UnregisterRowsBySource(const FTcsSourceHandle& Source, UTcsEventBusSubsystem* Bus);

	/**
	 * 全量清空 + 退订全部订阅（门面 `Deinitialize` 的确定性清理口）。
	 *
	 * @param Bus 事件总线门面（可为空——空时只清表，依赖总线自身 `Reset` 收尾）。
	 */
	void Reset(UTcsEventBusSubsystem* Bus);

#pragma endregion


// 查询
#pragma region Query

public:
	// 登记行数（观测/装置断言用）
	int32 GetRowCount() const;

	/**
	 * 收集某事件 Tag 的全部行句柄（**快照**：求值器遍历期间被摘除的行按代际校验跳过，
	 * 期间新登记的行不入本轮）。
	 *
	 * @param EventTag 事件 Tag。
	 * @param OutHandles 出参：命中的行句柄（登记序）。
	 */
	void CollectRowsForTag(FGameplayTag EventTag, TArray<FTcsEffectTriggerHandle>& OutHandles) const;

	/**
	 * 按句柄解析行（**代际校验**；悬空/已摘除返回 nullptr——不 ensure，正常竞态）。
	 *
	 * @param Handle 行句柄。
	 * @return 返回行实例指针；无效句柄返回 nullptr。
	 */
	const FTcsEffectTriggerInstance* FindRow(FTcsEffectTriggerHandle Handle) const;

#pragma endregion


// 行序
#pragma region Ordering

public:
	/**
	 * 行句柄按求值顺序原地排序：`Def.Priority` **降序**（大者先，与覆盖带 `OverridePriority` 同向）；
	 * 同 `Priority` 按**登记序**（槽位下标升序——确定性，MUST NOT 依赖容器哈希序）。
	 *
	 * @param Handles 待排序的行句柄数组（原地排序）。
	 */
	void SortRowsByPriority(TArray<FTcsEffectTriggerHandle>& Handles) const;

#pragma endregion


// 点灯
#pragma region Gates

public:
	// 点灯/灭灯（行级开关；`GateTags` **全部**点亮才通过该道门）
	void SetGateTagLit(FGameplayTag GateTag, bool bLit);

	// 开关是否点亮
	bool IsGateTagLit(FGameplayTag GateTag) const;

#pragma endregion


// 随机流
#pragma region Random

public:
	/**
	 * 设置求值随机流种子（D0-1 确定性纪律：概率条件的随机值**由本流供给**，
	 * 条件求值器内部 MUST NOT 取随机数——否则同输入不同输出、回放失效）。
	 *
	 * 默认种子 0 = 完全确定的固定序列（可复现）；宿主若要真实随机，须在**世界起时**
	 * 用自己的源播种一次（本类不自行取时间/熵——那会让可复现性无从保证）。
	 *
	 * @param Seed 随机流种子。
	 */
	void SetRandomSeed(int32 Seed);

	// 取下一个 [0,1) 随机值（仅概率类条件用；每行各取一次）
	double NextRandomValue();

#pragma endregion


// GC 补引用
#pragma region GC

public:
	/**
	 * GC 引用收集（**必需**，2026-09-23 实证：本表是 UObject 的**非 `UPROPERTY` 成员**）。
	 *
	 * **为什么必须自己实现（plan3 此处原文的理由是错的，实施时纠正）**：plan3 注记写
	 * "值语义 `TArray` → 无需 ARO 覆写"——**判据错了**。是否需要 ARO 与"值语义还是指针语义"
	 * **无关**，只取决于**容器是否 GC 可见**：`FTcsTriggerRegistry` 是 `UTcsEffectSubsystem` 的
	 * 普通 C++ 成员（非 `UPROPERTY`），GC 的 `RefLink` 遍历**根本走不到它**——故行内的
	 * `Def.Conditions` / `Def.EventPayloadFilter` 是 `FInstancedStruct`，其**内层内存里可以放
	 * 宿主自定义 struct 的 `UPROPERTY` 对象引用**（D4-16 条件/步骤类型不设限），GC 看不见
	 * → **静默回收**（条件跑到那里取到空引用，而非崩溃）。这正是 T-8 的同一类缺口
	 * （缺口在容器，不在载荷——见 `instanced-struct-aro-and-container-gc-gaps` 记忆卡）。
	 *
	 * 手法与 `ChainDefs` 完全同款（逐行调 `AddPropertyReferencesWithStructARO`——
	 * 引擎 `UDataTable::AddReferencedObjects` 对 `RowMap` 用的同一招）：
	 * 递归走 `FTcsEffectTriggerDef` 的反射属性，对带 `WithAddStructReferencedObjects` 的
	 * `FInstancedStruct`（`InstancedStruct.h:283`）会继续进内层实例内存。
	 *
	 * @param Collector GC 引用收集器。
	 * @param ReferencingObject 引用方（本表的属主，即门面）。
	 */
	void AddReferencedObjects(FReferenceCollector& Collector, UObject* ReferencingObject);

#pragma endregion


// 内核
#pragma region Core

private:
	// 分配槽位（优先复用空闲槽；新槽代际从 1 起）
	TTcsInstanceHandle<FTcsEffectTriggerTag> AllocateSlot();

	// 释放槽位（代际 +1 使旧句柄悬空）
	void FreeSlot(TTcsInstanceHandle<FTcsEffectTriggerTag> Inner);

	// 登记表内是否还有该 Tag 的行（订阅计数归零判定）
	bool HasRowForTag(FGameplayTag EventTag) const;

	// 按 Tag 装配订阅（已有订阅则复用——计数配对的"首次出现才订"）
	void EnsureSubscription(FGameplayTag EventTag, UTcsEventBusSubsystem* Bus);

	// 按 Tag 退订（该 Tag 行数归零时调用）
	void DropSubscriptionIfUnused(FGameplayTag EventTag, UTcsEventBusSubsystem* Bus);

	// 登记表（值语义；槽位下标即句柄 Index）
	TArray<FTcsEffectTriggerInstance> Rows;

	// 每槽代际（0 = 从未分配；分配与释放各 +1——奇数 = 已分配，偶数 = 空闲）
	TArray<uint32> RowGenerations;

	// 空闲槽位表（摘除后入栈，分配时复用）
	TArray<uint32> FreeSlots;

	// Tag → 订阅句柄（同 Tag 共用一个订阅；行数归零时移除）
	TMap<FGameplayTag, FTcsEventSubscriptionHandle> TagSubscriptions;

	// 点灯集（行级开关；`GateTags` 全部命中才通过）
	TSet<FGameplayTag> LitGateTags;

	// 求值随机流（种子由宿主设定；默认 0 = 确定的固定序列）
	FRandomStream RandomStream;

	// 共享求值器（订阅的 Handler；**弱引用**——强引用归门面 UPROPERTY，本表不阻止 GC）
	TWeakObjectPtr<UTcsTriggerEvaluator> Evaluator;

#pragma endregion
};
