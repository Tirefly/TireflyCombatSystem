// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "State/TcsStateHandle.h"
#include "State/TcsStateInstance.h"



/**
 * 单个单位的状态桶（`FStateBucket`）：一个单位的全部在册实例 + 槽位代际 + 空闲槽栈。
 *
 * **为什么自建槽位而不用 TcsCore 的 `TTcsInstancePool`**：那个池是"一个池 = 一个句柄空间"，
 * 而状态要的是**每单位一个句柄空间**（句柄 `Index` 在单位内定位槽位即可，无需全局唯一）。
 * 强行套用只有两条路且都更差——① 一桶一池：句柄 `Index` 全局唯一但语义绕（同一件事两套身份），
 * 且每单位多一份池对象与统计；② 全局一池：桶得另存槽位下标，句柄与单位解耦后
 * `GetState(Handle)` 无法直接定位。故照 `FTcsTriggerRegistry` 的**槽位 + 代际**手法自建，
 * 语义（分配优先复用空闲槽、释放使代际 +1、新槽代际从 1 起）**刻意与池一致**，
 * 使"代际失配 = 悬空"这条判据在全插件只有一种读法。
 *
 * **刻意不带导出宏**（2026-10-05 实测）：本类型持 `TUniquePtr` 元素 ⇒ 不可复制；
 * 加 `TCSSTATE_API` 会让 MSVC 强制实例化 `TMap` 的复制路径并报 `C2280`
 * （`TSparseSetElement` 复制到已删除的函数）。跨模块可达性由**门面**承担——
 * `UTcsStateSubsystem` 带 `TCSSTATE_API`，它作为成员持有本类型，消费方经门面访问即可。
 */
struct FStateBucket
{
// 槽位
#pragma region Slots

public:
	/**
	 * 分配槽位（优先复用空闲槽；新槽代际从 1 起）。
	 *
	 * @return 返回新槽的句柄（`Index` 为桶内下标，`Generation` 为当前代际）。
	 */
	FTcsStateHandle AllocateSlot();

	/**
	 * 释放槽位（代际 +1 使旧句柄悬空；槽位入空闲栈）。
	 *
	 * @param SlotIndex 桶内槽位下标。
	 */
	void ReleaseSlot(uint32 SlotIndex);

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄解析实例（**代际校验**：失配 / 越界返回 nullptr）。
	 *
	 * @param Handle 状态实例句柄。
	 * @return 返回实例指针；脏句柄返回 nullptr。
	 */
	FTcsStateInstance* Find(const FTcsStateHandle& Handle);

	/**
	 * 按句柄解析实例（只读重载）。
	 *
	 * @param Handle 状态实例句柄。
	 * @return 返回实例指针；脏句柄返回 nullptr。
	 */
	const FTcsStateInstance* Find(const FTcsStateHandle& Handle) const;

	/**
	 * 句柄是否指向在册实例（代际校验，返回 false 而不 ensure）。
	 *
	 * @param Handle 状态实例句柄。
	 * @return 返回是否在册。
	 */
	bool IsValidHandle(const FTcsStateHandle& Handle) const;

	/**
	 * 遍历在册实例（**按槽位下标升序**——稳定序，同输入同输出是可复现验收的前提）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **遍历期间 MUST NOT 增删实例**（增删会改动槽位/空闲栈）：需要增删时先自行收集句柄，
	 * 遍历结束后再动（同 `FTcsTriggerRegistry::UnregisterRowsBySource` 的两段式手法）。
	 * **就地改字段是允许的**（改 `ExpiryEntry` 之类的锚点不动槽位）——门面 `Deinitialize`
	 * 的"逐条撤时间条目"正靠这一条。
	 *
	 * @param Visitor 访问者（返回是否继续）。
	 */
	void ForEach(TFunctionRef<bool(const FTcsStateInstance&)> Visitor) const;

	// 遍历在册实例（可写重载——仅供"就地改字段"的场景，见只读重载的纪律说明）
	void ForEach(TFunctionRef<bool(FTcsStateInstance&)> Visitor);

	/**
	 * 在册实例数（观测与统计用）。
	 *
	 * @return 返回在册实例数。
	 */
	int32 Num() const;

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	/**
	 * 清空桶（**逐槽归还**：代际 +1 使全部旧句柄失效，空闲栈重建）——单位注销与门面清理的收口。
	 */
	void Reset();

#pragma endregion


// 内核
#pragma region Core

private:
	/**
	 * 由槽位下标 + 当前代际拼句柄（**唯一拼装点**——与 `Handle.Index` 的位模式转换同处一处，
	 * 避免各处自行拼装出语义不一致的句柄）。
	 *
	 * @param SlotIndex 桶内槽位下标。
	 * @return 返回该槽当前代际的句柄。
	 */
	FTcsStateHandle MakeHandle(uint32 SlotIndex) const;

	// 槽位数组（值语义；下标即句柄 `Index`）
	TArray<FTcsStateInstance> Instances;

	// 每槽代际（0 = 从未分配；分配与释放各 +1——奇数 = 已分配，偶数 = 空闲）
	TArray<uint32> SlotGenerations;

	// 空闲槽位栈（释放后入栈，分配时复用）
	TArray<uint32> FreeSlots;

#pragma endregion
};



/**
 * per-unit 状态注册表（D3-1/D3-3）：`单位 → 桶` 的登记表，持全部在册状态实例。
 *
 * **为什么用 `TUniquePtr` 间接层**：`TMap` 扩容会**搬移元素**，若桶按值存放，
 * 遍历期间为另一个单位建桶就可能让手上的桶指针悬空。间接层使**桶地址稳定**（只有指针搬家），
 * 遍历/嵌套修改因此安全。
 *
 * **脏句柄一律拒绝 + `Warning`（不 `ensure`）**：代际失配 / 下标越界 / 单位未注册都是
 * **时序竞态**语义（调用方手上的句柄过期了），不是配置错误——与"非法阶段迁移"的 `ensure` 口径
 * 刻意分开。拒绝 MUST NOT 影响同桶其它实例。
 *
 * **导出宏理由同 `FStateBucket`**（二者是同一处组合的两个类型；加宏会强制实例化 `TMap` 复制路径）。
 */
class FTcsStateRegistry
{
// 分配
#pragma region Allocation

public:
	/**
	 * 取或建单位桶（不存在则新建）。
	 *
	 * **返回引用是安全的**：桶经 `TUniquePtr` 持有，本次调用之后的 `TMap` 扩容只搬指针、不搬桶。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回该单位的桶。
	 */
	FStateBucket& FindOrAddBucket(FTcsCombatEntityHandle Unit);

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄解析实例（先定位单位桶，再代际校验；任一步失败返回 nullptr）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Handle 状态实例句柄。
	 * @return 返回实例指针；脏句柄 / 单位未注册返回 nullptr。
	 */
	FTcsStateInstance* Find(FTcsCombatEntityHandle Unit, const FTcsStateHandle& Handle);

	// 按句柄解析实例（只读重载；语义同上）
	const FTcsStateInstance* Find(FTcsCombatEntityHandle Unit, const FTcsStateHandle& Handle) const;

	// 取单位桶（未注册返回 nullptr——正常查询路径，不 ensure）
	FStateBucket* FindBucket(FTcsCombatEntityHandle Unit);

	// 取单位桶（只读重载）
	const FStateBucket* FindBucket(FTcsCombatEntityHandle Unit) const;

	// 是否已为该单位建桶
	bool ContainsUnit(FTcsCombatEntityHandle Unit) const;

	// 已建桶数（观测用）
	int32 NumBuckets() const;

	// 全部在册实例数（跨单位合计；观测与统计用）
	int32 NumInstances() const;

	/**
	 * 遍历全部桶（**按单位句柄的 `TMap` 序**——不定序，MUST NOT 依赖它做可复现断言）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **为什么给这个口**：`FTcsStateOps::Find` 要按"桶内是否有该句柄的在册实例"做代际校验式定位
	 * （句柄不带单位段 ⇒ 只能扫桶）。把它做成公开遍历而不是 friend 掉整个 `Buckets`：
	 * 前者是**一个受控的读口**，后者会把"桶怎么存"变成外部可依赖的事实。
	 *
	 * @param Visitor 访问者（单位 + 该单位桶；返回是否继续）。
	 */
	void ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, const FStateBucket&)> Visitor) const;

	// 遍历全部桶（可写重载——仅供"就地改实例字段"的场景，如门面 `Deinitialize` 逐条撤条目）
	void ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, FStateBucket&)> Visitor);

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	/**
	 * 注销单位：清空并删除该单位的桶（该单位全部旧句柄随之失效）。
	 *
	 * **调用方 MUST 先完成该单位每条实例的撤销流程**（广播 + 外部资源摘除）——
	 * 本函数只管登记表，不代为广播、不代为撤销外部资源（同 `TTcsInstancePool::Reset` 的零策略纪律）。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回是否确实删掉了一个桶（未注册返回 false——正常路径，不 ensure）。
	 */
	bool RemoveBucket(FTcsCombatEntityHandle Unit);

	// 全量清空（门面 `Deinitialize` 的确定性清空口——全部旧句柄失效）
	void Reset();

#pragma endregion


// 内核
#pragma region Core

private:
	// 单位 → 桶（TUniquePtr 间接层使桶地址不随登记表扩容而失效）
	TMap<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>> Buckets;

#pragma endregion
};
