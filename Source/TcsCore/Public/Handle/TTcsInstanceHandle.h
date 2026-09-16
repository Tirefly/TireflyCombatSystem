// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



/**
 * 实例句柄模板（D0-2）：Index + Generation 强类型句柄。
 * TTagType 仅作类型区分——不同池的句柄类型不可互换，零运行时开销；
 * 代际校验需池上下文（见 TTcsInstancePool::IsValid），本结构只判"是否曾被赋值"。
 */
template <typename TTagType>
struct TTcsInstanceHandle
{
	// 无效索引常量
	static constexpr uint32 InvalidIndex = 0xFFFFFFFF;

	// 池内槽位索引（InvalidIndex = 无效）
	uint32 Index = InvalidIndex;

	// 代际计数：Free 时 +1，旧句柄凭代际失配判悬空
	uint32 Generation = 0;

	// 句柄有效性（不校验代际——无池上下文）
	bool IsValid() const
	{
		return Index != InvalidIndex;
	}

	// 相等比较（订阅配对/句柄清理用）
	friend bool operator==(const TTcsInstanceHandle& A, const TTcsInstanceHandle& B)
	{
		return A.Index == B.Index && A.Generation == B.Generation;
	}
};
