// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsInstanceHandle.h"
#include "Pool/TcsInstancePool.h"
#include "TcsCoreLogChannel.h"
#include "TcsCoreStats.h"

/**
 * 到期堆条目句柄标签（仅供句柄模板做类型区分——与其他池句柄编译期防互串）
 */
struct FTcsExpiryTag
{
};



// 到期堆条目句柄（取消锚点；句柄配对清理——复用 TTcsInstancePool 机制）
struct FTcsTimeEntryHandle
{
	// 池内句柄
	TTcsInstanceHandle<FTcsExpiryTag> Inner;

	// 句柄有效性（代际校验由到期堆在取消/出队时执行）
	bool IsValid() const
	{
		return Inner.IsValid();
	}
};



// 到期堆条目（池内实例：到期时刻 + 归属 + 回调——释放前由堆负责清回调，防捕获悬挂）
struct FTcsExpiryEntry
{
	// 到期时刻（FTcsClock.Elapsed 时轴上的绝对时刻）
	double DueTime = 0.0;

	// 宿主侧归属 Id（堆零领域词汇——回调按 OwnerId 归还归属）
	uint64 OwnerId = 0;

	// 入堆序号（同到期时刻的稳定序键，D0-1）
	uint64 Sequence = 0;

	// 到期回调（一次性——回调完成后条目释放、句柄失效）
	TFunction<void(uint64)> OnDue;
};



/**
 * 到期最小堆（01 §2.3 / D3-6）：按到期时刻排序，帧成本 = 到期项数（无到期时仅看堆顶）。
 * 条目池化（TTcsInstancePool——代际校验防悬空回调），Cancel 惰性（代际失效后出队跳过）。
 * 稳定序（D0-1）：到期时刻升序，同刻按入堆序回调。
 * 到期回调内新入堆的条目归下一次 AdvanceTo（与总线冲洗换出同纪律——防自激、帧成本有界）。
 * 单游戏线程假设——进入点断言（D0-4）。
 */
class FTcsExpiryHeap
{
// 堆操作
#pragma region HeapOps

public:
	// 入堆：返回条目句柄（一次性——回调完成后失效；DueTime 为 Elapsed 时轴绝对时刻）
	FTcsTimeEntryHandle Push(double DueTime, uint64 OwnerId, TFunction<void(uint64)> OnDue)
	{
		ensure(IsInGameThread());

		FTcsTimeEntryHandle Handle;
		Handle.Inner = EntryPool.Allocate();

		FTcsExpiryEntry* Entry = EntryPool.Resolve(Handle.Inner);
		Entry->DueTime = DueTime;
		Entry->OwnerId = OwnerId;
		Entry->Sequence = NextSequence++;
		Entry->OnDue = MoveTemp(OnDue);

		HeapItems.Push(FTcsHeapItem{DueTime, Entry->Sequence, Handle});
		SiftUp(HeapItems.Num() - 1);

		FTcsCoreHeapStats::AddDepth();
		return Handle;
	}

	// 惰性取消：即刻回收槽位（代际 +1），堆内残留条目出队时凭代际失配跳过（句柄配对清理——悬空/重复取消 ensure 拦截）
	void Cancel(FTcsTimeEntryHandle Handle)
	{
		ensure(IsInGameThread());

		if (!ensureMsgf(EntryPool.IsValid(Handle.Inner),
			TEXT("FTcsExpiryHeap::Cancel: 悬空或无效句柄（Index=%u, Generation=%u）"), Handle.Inner.Index, Handle.Inner.Generation))
		{
			return;
		}

		FTcsExpiryEntry* Entry = EntryPool.Resolve(Handle.Inner);
		Entry->OnDue = nullptr;
		Entry->OwnerId = 0;
		EntryPool.Free(Handle.Inner);
		FTcsCoreHeapStats::RemoveDepth();
	}

	// 推进：弹出全部 DueTime <= Now 的条目并按（到期时刻, 入堆序）升序回调
	void AdvanceTo(double Now)
	{
		ensure(IsInGameThread());

		// 无到期仅看堆顶 O(1)（帧成本 = 到期项数）
		if (HeapItems.Num() == 0 || HeapItems[0].DueTime > Now)
		{
			return;
		}

		// 先换出当前到期集再逐项回调：回调内新入堆的条目（无论是否已到期）归下一次 AdvanceTo
		TArray<FTcsHeapItem> DueItems;
		while (HeapItems.Num() > 0 && HeapItems[0].DueTime <= Now)
		{
			DueItems.Add(TakeRoot());
		}

		for (const FTcsHeapItem& Item : DueItems)
		{
			FireDueItem(Item, Now);
		}
	}

	// 清空（子系统 Deinitialize 确定性清理）：清回调释放捕获 + 统计回落 + 堆数组清空
	void Reset()
	{
		ensure(IsInGameThread());

		int32 LiveCount = 0;
		EntryPool.ForEach([&LiveCount](FTcsExpiryEntry& Entry)
		{
			Entry.OnDue = nullptr;
			Entry.OwnerId = 0;
			++LiveCount;
		});
		for (int32 Index = 0; Index < LiveCount; ++Index)
		{
			FTcsCoreHeapStats::RemoveDepth();
		}

		HeapItems.Empty();
	}

#pragma endregion

// 堆内部
#pragma region HeapInternal

private:
	// 堆数组元素（排序键 + 句柄；条目本体住池）
	struct FTcsHeapItem
	{
		// 到期时刻（排序首键，升序）
		double DueTime = 0.0;

		// 入堆序（同刻稳定序键，升序）
		uint64 Sequence = 0;

		// 条目句柄（代际校验防悬空回调）
		FTcsTimeEntryHandle Handle;
	};

	// 最小堆排序键：到期时刻升序，同刻按入堆序（D0-1 稳定序）
	static bool IsBefore(const FTcsHeapItem& A, const FTcsHeapItem& B)
	{
		return A.DueTime < B.DueTime || (A.DueTime == B.DueTime && A.Sequence < B.Sequence);
	}

	// 取出堆顶（尾元素补位 + 下沉）
	FTcsHeapItem TakeRoot()
	{
		FTcsHeapItem Root = MoveTemp(HeapItems[0]);
		HeapItems[0] = MoveTemp(HeapItems.Last());
		HeapItems.Pop();
		if (HeapItems.Num() > 0)
		{
			SiftDown(0);
		}
		return Root;
	}

	void SiftUp(int32 Index)
	{
		while (Index > 0)
		{
			const int32 Parent = (Index - 1) / 2;
			if (!IsBefore(HeapItems[Index], HeapItems[Parent]))
			{
				break;
			}
			Swap(HeapItems[Index], HeapItems[Parent]);
			Index = Parent;
		}
	}

	void SiftDown(int32 Index)
	{
		const int32 Count = HeapItems.Num();
		while (true)
		{
			const int32 Left = Index * 2 + 1;
			if (Left >= Count)
			{
				break;
			}
			const int32 Right = Left + 1;
			const int32 Best = (Right < Count && IsBefore(HeapItems[Right], HeapItems[Left])) ? Right : Left;
			if (!IsBefore(HeapItems[Best], HeapItems[Index]))
			{
				break;
			}
			Swap(HeapItems[Index], HeapItems[Best]);
			Index = Best;
		}
	}

	// 出队一笔到期条目并回调（惰性取消的残留条目凭代际失配跳过）
	void FireDueItem(const FTcsHeapItem& Item, double Now)
	{
		if (!EntryPool.IsValid(Item.Handle.Inner))
		{
			return;
		}

		FTcsExpiryEntry* Entry = EntryPool.Resolve(Item.Handle.Inner);
		const uint64 OwnerId = Entry->OwnerId;

		UE_LOG(LogTcsCore, Log, TEXT("FTcsExpiryHeap: 到期回调 Owner=%llu DueTime=%.6f Now=%.6f"),
			OwnerId, Item.DueTime, Now);

		// 拷出回调再触发：回调内可能入堆（条目数组扩容/槽位复用），触发后原条目指针失效
		TFunction<void(uint64)> Callback = MoveTemp(Entry->OnDue);
		Callback(OwnerId);

		// 槽位未被回调内 Cancel/复用动过才由本路径释放（动过则代际已失配——回收在 Cancel/复用赋值时完成）
		if (EntryPool.IsValid(Item.Handle.Inner))
		{
			EntryPool.Free(Item.Handle.Inner);
			FTcsCoreHeapStats::RemoveDepth();
		}
	}

#pragma endregion

// 堆存储
#pragma region Storage

private:
	// 条目池（代际校验防悬空回调）
	TTcsInstancePool<FTcsExpiryEntry, FTcsExpiryTag> EntryPool;

	// 最小堆数组（堆顶 = 最小排序键；条目本体住池）
	TArray<FTcsHeapItem> HeapItems;

	// 入堆序号发生器（稳定序键）
	uint64 NextSequence = 0;

#pragma endregion
};
