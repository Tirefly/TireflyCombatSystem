// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <atomic>



/**
 * 归属来源标识（R0 §9：系统级通用归属机制，非战斗本体论）。
 * 进程内唯一 Id，0 保留为无效值；作为级联撤销锚点（D2-2：来源注销 → 消费方按 Source 级联移除）。
 */
struct TCSCORE_API FTcsSourceHandle
{
	// 来源唯一 Id（0 = 无效）
	uint64 Id = 0;

	// 来源有效性
	bool IsValid() const
	{
		return Id != 0;
	}

	// 相等比较（来源配对/级联过滤用）
	friend bool operator==(const FTcsSourceHandle& A, const FTcsSourceHandle& B)
	{
		return A.Id == B.Id;
	}
};



/**
 * 来源分配器：进程内原子递增发号（首个分配为 1）。
 * 释放/级联清理由需要方驱动（D2-2——本类只发号，不做注销登记）。
 */
struct TCSCORE_API FTcsSourceHandleRegistry
{
	// 分配新来源 Id（原子递增，进程内唯一）
	FTcsSourceHandle Allocate()
	{
		FTcsSourceHandle Handle;
		Handle.Id = NextId.fetch_add(1, std::memory_order_relaxed) + 1;
		return Handle;
	}

private:
	// 下一来源 Id（原子计数；0 保留无效）
	std::atomic<uint64> NextId{0};
};
