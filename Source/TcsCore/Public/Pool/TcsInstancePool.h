// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "Handle/TcsInstanceHandle.h"
#include "TcsCoreStats.h"

/**
 * 实例池模板（D0-2/D0-4）：稠密数组 + FreeList 复用 + 代际防悬空。
 * 单游戏线程假设——所有进入点断言（ensure(IsInGameThread())，D0-4）。
 *
 * 代际奇偶约定（内部簿记）：奇数 = 已分配，偶数 = 空闲——新建槽从 1 起，Free 与复用时各 +1，
 * ForEach 凭奇偶跳过空闲槽，无需额外位图。
 *
 * 注意：Free 不清零槽位内容——持有外部资源的实例由调用方在 Free 前自行清理（池零策略纪律）。
 */
template <typename TInstanceType, typename TTagType>
class TTcsInstancePool
{
// 池操作
#pragma region PoolOps

public:
	// 分配槽位（优先复用 FreeList 尾部；新槽代际从 1 起）
	TTcsInstanceHandle<TTagType> Allocate()
	{
		ensure(IsInGameThread());

		uint32 SlotIndex;
		if (FreeList.Num() > 0)
		{
			SlotIndex = FreeList.Pop(EAllowShrinking::No);
			Generations[SlotIndex] += 1;
		}
		else
		{
			SlotIndex = static_cast<uint32>(Instances.Num());
			Instances.Emplace();
			Generations.Add(1);
			FTcsCorePoolStats::AddSlotCount();
		}

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
			Generations[Handle.Index] == Handle.Generation;
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
