// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "Handle/TcsInstanceHandle.h"
#include "TcsCoreStats.h"

/**
 * 默认代际发号策略：**池内自增**（2026-10-14 提取为可注入策略，本档逐字保持既有行为）。
 *
 * **契约（注入方 MUST 遵守）**：`IssueAllocation` 返回该槽本次分配的代际。**MUST 为正奇数**
 * （奇 = 已分配 / 偶 = 空闲是本池的奇偶簿记，`ForEach` 与 `IsValid` 都依赖它）。
 * `PriorGeneration` = 该槽当前代际（**全新槽传 0**，否则传释放后的偶数值）。
 *
 * **为什么需要可注入**：**per-entity 桶**场景下，若各桶各用自己的池、又各从 1 起发号，
 * 则两个实体各自的首实例会得到**相同的 `(Index, Generation)`**；而句柄**不带实体段**，
 * "跨桶扫描式解析"（`FTcsStateOps::Find` 就是这种）会**命中错实体**——本仓 2026-10-05
 * 实测过的缺陷。⇒ 该场景 MUST 注入**进程级**发号器（见 `FTcsStateGenerationIssuer`）。
 *
 * **无状态（MUST）**：进程级计数器 MUST **出线**到模块 `.cpp`（匿名 namespace 变量），
 * **MUST NOT** 用头内联 `static` / inline 变量——模块化构建下每个引用它的 DLL 各留一份，
 * 于是各模块各自从 1 起发号（症状随构建配置漂移）。故本策略按**静态接口**约定，池不持有其实例。
 */
struct FTcsPoolLocalGenerationIssuer
{
	static uint32 IssueAllocation(uint32 PriorGeneration)
	{
		return PriorGeneration + 1;
	}
};



/**
 * 实例池模板（D0-2/D0-4）：稠密数组 + FreeList 复用 + 代际防悬空。
 * 单游戏线程假设——所有进入点断言（ensure(IsInGameThread())，D0-4）。
 *
 * 代际奇偶约定（内部簿记）：奇数 = 已分配，偶数 = 空闲——新建槽代际由发号策略给出（默认 1），
 * Free 时 +1 置偶、复用时由发号策略重新给出，ForEach 凭奇偶跳过空闲槽，无需额外位图。
 *
 * **`TGenerationIssuer`**（2026-10-14 新增第三模板参）：代际发号策略，默认 `FTcsPoolLocalGenerationIssuer`
 * = 池内自增 = **本参引入前的行为逐字不变** ⇒ 既有四个使用者（到期堆 / 事件总线 / 链运行态 /
 * 施法运行态）**零改动、零行为差异**。per-entity 桶 MUST 注入进程级发号器（见该策略的说明）。
 *
 * 注意：Free 不清零槽位内容——持有外部资源的实例由调用方在 Free 前自行清理（池零策略纪律）。
 */
template <typename TInstanceType, typename TTagType, typename TGenerationIssuer = FTcsPoolLocalGenerationIssuer>
class TTcsInstancePool
{
// 池操作
#pragma region PoolOps

public:
	// 分配槽位（优先复用 FreeList 尾部；新槽代际由发号策略给出，默认从 1 起）
	TTcsInstanceHandle<TTagType> Allocate()
	{
		ensure(IsInGameThread());

		uint32 SlotIndex;
		uint32 PriorGeneration;
		if (FreeList.Num() > 0)
		{
			SlotIndex = FreeList.Pop(EAllowShrinking::No);
			PriorGeneration = Generations[SlotIndex];
		}
		else
		{
			SlotIndex = static_cast<uint32>(Instances.Num());
			Instances.Emplace();
			Generations.Add(0);
			PriorGeneration = 0;
			FTcsCorePoolStats::AddSlotCount();
		}

		// **唯一产生"分配代际"的地方**（释放路径的 +1 是"置偶数"、不是分配身份——见 Free）
		Generations[SlotIndex] = TGenerationIssuer::IssueAllocation(PriorGeneration);

		TTcsInstanceHandle<TTagType> Handle;
		Handle.Index = SlotIndex;
		Handle.Generation = Generations[SlotIndex];
		return Handle;
	}

	// 释放槽位（代际 +1 使旧句柄悬空；无效/重复释放/悬空句柄 ensure 拦截）
	void Free(TTcsInstanceHandle<TTagType> Handle)
	{
		ensure(IsInGameThread());

		if (!ensureMsgf(IsValid(Handle), TEXT("TTcsInstancePool::Free: 悬空或无效句柄（Index=%u, Generation=%u）"), Handle.Index, Handle.Generation))
		{
			return;
		}

		// **这里只"置偶数"、不产生分配身份**（分配身份一律由 `Allocate` 经发号策略给出）：
		// 已分配代际恒为正奇数 ⇒ +1 即偶数，旧句柄凭"非奇数"与"数值失配"双重失效。
		Generations[Handle.Index] += 1;
		FreeList.Push(Handle.Index);
		FTcsCorePoolStats::RemoveSlotCount();
	}

	// 句柄解析（Resolve 动词纪律：句柄 → 对象）：悬空/无效 ensure（Development），Shipping 编译剔除后返回 nullptr
	TInstanceType* Resolve(TTcsInstanceHandle<TTagType> Handle)
	{
		ensure(IsInGameThread());

		if (!IsValid(Handle))
		{
			ensureMsgf(false, TEXT("TTcsInstancePool::Resolve: 悬空或无效句柄（Index=%u, Generation=%u）"), Handle.Index, Handle.Generation);
			return nullptr;
		}

		return &Instances[Handle.Index];
	}

	// 句柄有效性（代际校验，不取对象）
	bool IsValid(TTcsInstanceHandle<TTagType> Handle) const
	{
		return Handle.IsValid() &&
			Handle.Index < static_cast<uint32>(Generations.Num()) &&
			Generations[Handle.Index] == Handle.Generation &&
			// **奇偶校验也属"是否在册"**（2026-10-14 补）：代际相等**不足以**证明槽位在册——
			// 释放把代际 +1 置偶，而一个**手工拼装**的偶代际句柄（如 `{0, 2}`）会与空闲槽"恰好相等"
			// ⇒ 缺本条就会把空闲槽判成在册、并返回一个已释放（未清零）的实例。
			// 本仓手写的 per-entity 桶一直带这条判据（"偶数 = 空闲，与 TTcsInstancePool 同款"），
			// 池此前**缺**它 ⇒ 本次补齐使两者真正同款。对良构句柄**行为中性**（分配产物恒为奇）。
			(Generations[Handle.Index] & 1u) == 1u;
	}

	/**
	 * 该**槽位**当前是否在册（只按下标问，不需要句柄；2026-10-14 补）。
	 *
	 * **为什么需要它**：`ReleaseSlot(下标)` 这类"按下标释放"的口 MUST 对**已空闲 / 越界**的下标
	 * 保持**静默容忍**（既有手写桶的语义——重复释放不是契约违规，属正常的竞态收尾），
	 * 而 `Free` 对**句柄**口径是 `ensure` 拦截（"调用方给错句柄"是契约违规）。
	 * 两者口径刻意不同 ⇒ 需要这个"只要下标"的判据来分流。
	 *
	 * @param SlotIndex 槽位下标（允许越界——越界返回 false）。
	 * @return 返回该槽是否在册。
	 */
	bool IsSlotAllocated(uint32 SlotIndex) const
	{
		return SlotIndex < static_cast<uint32>(Generations.Num()) &&
			(Generations[SlotIndex] & 1u) == 1u;
	}

	/**
	 * 取该槽**当前代际**的句柄（只按下标拼，不需要已有句柄；2026-10-14 补）。
	 *
	 * **为什么需要它**：共享桶的 `ReleaseSlot(下标)` / `Reset` 是**按槽位下标**驱动的口，
	 * 而 `Free` 收的是**句柄** ⇒ 需要"由下标反推当前句柄"这一步。
	 *
	 * 越界返回**无效**句柄（`Index = InvalidIndex`）；调用方 SHOULD 先经 `IsSlotAllocated` 判定。
	 *
	 * @param SlotIndex 槽位下标。
	 * @return 返回该槽当前代际的句柄；越界返回无效句柄。
	 */
	TTcsInstanceHandle<TTagType> MakeHandleForSlot(uint32 SlotIndex) const
	{
		TTcsInstanceHandle<TTagType> Handle;
		if (SlotIndex < static_cast<uint32>(Generations.Num()))
		{
			Handle.Index = SlotIndex;
			Handle.Generation = Generations[SlotIndex];
		}

		return Handle;
	}

	/**
	 * 当前**在册**槽位数（观测用；2026-10-14 补）。
	 * 判据与 `ForEach` 的可见集**逐字相同**（凭代际奇偶跳过空闲槽）。
	 */
	int32 NumAllocated() const
	{
		int32 Count = 0;
		for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Generations.Num()); ++SlotIndex)
		{
			if ((Generations[SlotIndex] & 1u) == 1u)
			{
				++Count;
			}
		}

		return Count;
	}

	/**
	 * 把某槽**置为空闲**并归还（**不**走 `Free` 的句柄校验路径；2026-10-14 补）。
	 *
	 * **与 `Free` 的分工 MUST 分清**：`Free` 收句柄、对悬空/重复释放 **ensure** 拦截
	 * （"调用方给错句柄"= 契约违规）；本函数收**下标**、对已空闲/越界**静默返回 false**
	 * （重复释放属正常竞态收尾）——共享桶的 `ReleaseSlot` 需要后者这种口径。
	 *
	 * @param SlotIndex 槽位下标。
	 * @return 返回是否确实释放了一个在册槽（已空闲/越界返回 false）。
	 */
	bool ReleaseSlotByIndex(uint32 SlotIndex)
	{
		ensure(IsInGameThread());

		if (!IsSlotAllocated(SlotIndex))
		{
			return false;
		}

		// 与 Free 同款：只"置偶数"、不产生分配身份（分配身份一律由 Allocate 经发号策略给出）
		Generations[SlotIndex] += 1;
		FreeList.Push(SlotIndex);
		FTcsCorePoolStats::RemoveSlotCount();
		return true;
	}

	/**
	 * 直接把某槽内容**重置为默认值**（2026-10-14 补；共享桶的 `ReleaseSlot` 要"槽位内容不跨
	 * 生命周期残留"的语义——既有手写桶在释放时都会 `= TInstanceType()`）。
	 *
	 * 调用方 MUST 先确认该槽在册且**即将释放**（本函数不改代际、不动 FreeList）。
	 */
	void ResetSlotContent(uint32 SlotIndex)
	{
		if (SlotIndex < static_cast<uint32>(Instances.Num()))
		{
			Instances[SlotIndex] = TInstanceType();
		}
	}

	/**
	 * 句柄解析（**只读、不 ensure** 的重载；2026-10-14 补）。
	 *
	 * **为什么需要它**：查询面（"时序竞态 ⇒ 静默返回 nullptr"语义，如 `FTcsCastOps::ResolveInstance` /
	 * 共享桶的 `Find`）持**常量**门面，而上面那个 `Resolve` 是**非常量**且对悬空句柄 **ensure**
	 * （那是"调用方给错句柄"的契约违规口径，与查询面刻意分开）。缺本重载就只能
	 * `const_cast` 或自行重实现一遍代际判定——后者正是"第二套判定"。
	 *
	 * 判据与非常量重载的**前置检查**逐字相同（都走 `IsValid`）；差别只在**不 ensure**。
	 *
	 * @return 返回实例指针；无效/悬空/空闲槽句柄返回 nullptr。
	 */
	const TInstanceType* Resolve(TTcsInstanceHandle<TTagType> Handle) const
	{
		return IsValid(Handle) ? &Instances[Handle.Index] : nullptr;
	}

	// 稳定序遍历（Index 升序，凭代际奇偶跳过空闲槽）
	void ForEach(TFunctionRef<void(TInstanceType&)> Fn)
	{
		ensure(IsInGameThread());

		for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Instances.Num()); ++SlotIndex)
		{
			if ((Generations[SlotIndex] & 1u) == 1u)
			{
				Fn(Instances[SlotIndex]);
			}
		}
	}

	/**
	 * 稳定序遍历（**只读重载**；2026-10-14 补，供共享桶的 `const ForEach` 使用）。
	 *
	 * **它 `ForEachForGC` 的区别 MUST 分清（这是两条不同的口）**：
	 * 本重载**断言游戏线程**（同可变重载——它是**常规业务遍历**，只是不写），
	 * 而 `ForEachForGC` **不断言**且**唯一合法调用者是 ARO**。
	 * ⇒ 业务侧要只读遍历 MUST 用本重载，**MUST NOT** 借用 `ForEachForGC` 绕开线程断言。
	 * 判据与可变重载逐字相同（Index 升序 + 代际奇偶跳过空闲槽）。
	 */
	void ForEach(TFunctionRef<void(const TInstanceType&)> Fn) const
	{
		ensure(IsInGameThread());

		for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Instances.Num()); ++SlotIndex)
		{
			if ((Generations[SlotIndex] & 1u) == 1u)
			{
				Fn(Instances[SlotIndex]);
			}
		}
	}

	/**
	 * GC 引用收集专用的**只读遍历**（2026-10-08 新增；起因 = 一处真实缺陷）。
	 *
	 * **为什么必须有它**：GC 的引用收集**不在游戏线程上**，而本池的**每一个**"正常"进入点
	 * （`Allocate` / `Free` / `Resolve` / `IsValid` / `ForEach` / `Reset`）都无条件
	 * `ensure(IsInGameThread())`（D0-4 单游戏线程假设）⇒ **宿主门面在 `AddReferencedObjects` 里
	 * 遍历本池会每次 GC 留一条红字**（实测：`FRealtimeGC::CollectReferencesForGC` →
	 * `AddReferencedObjects` → `ForEach` → `Ensure condition failed: IsInGameThread()`，
	 * **与池空不空无关**，且 `ensure` 每站点每进程只报一次 ⇒ 症状是"偶尔一条红字"，极易被忽略）。
	 *
	 * **它 MUST 保持的三条纪律**：① **纯读**——MUST NOT 在此改任何状态（GC 期改状态是另一类错误）；
	 * ② **判据与 `ForEach` 逐字相同**（按代际奇偶跳过空闲槽、按槽位 Index 升序），MUST NOT 引入
	 * 第二套"哪些槽在册"的判定；③ **MUST NOT 代替 `ForEach` 被用于常规逻辑**——它**没有**线程断言，
	 * 拿它做业务遍历等于静默放弃 D0-4 的单线程纪律。**用途只有一个：GC 的 ARO。**
	 *
	 * @param Fn 访问者（只读实例）。
	 */
	void ForEachForGC(TFunctionRef<void(const TInstanceType&)> Fn) const
	{
		for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Instances.Num()); ++SlotIndex)
		{
			if ((Generations[SlotIndex] & 1u) == 1u)
			{
				Fn(Instances[SlotIndex]);
			}
		}
	}

	/**
	 * 重置（宿主子系统 Deinitialize 的确定性清空口，与 FTcsExpiryHeap::Reset 同款纪律）：
	 * 按"当前已分配槽位数"一次性回落占用统计后清空三数组——旧句柄全部代际失配。
	 * 不逐槽清理实例内容：**持有外部资源的实例由调用方在 Reset 前自行清理**（与 Free 同款零策略）。
	 */
	void Reset()
	{
		ensure(IsInGameThread());

		int32 LiveCount = 0;
		for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Generations.Num()); ++SlotIndex)
		{
			if ((Generations[SlotIndex] & 1u) == 1u)
			{
				++LiveCount;
			}
		}
		for (int32 Index = 0; Index < LiveCount; ++Index)
		{
			FTcsCorePoolStats::RemoveSlotCount();
		}

		Instances.Reset();
		Generations.Reset();
		FreeList.Reset();
	}

#pragma endregion

// 池存储
#pragma region Storage

private:
	// 稠密实例数组（含空闲洞，Index 即槽位）
	TArray<TInstanceType> Instances;

	// 每槽代际（奇 = 已分配，偶 = 空闲）
	TArray<uint32> Generations;

	// 空闲槽位栈（Allocate 复用尾部）
	TArray<uint32> FreeList;

#pragma endregion
};
