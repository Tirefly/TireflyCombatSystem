// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/TypeHash.h"

#include "TcsSkillEntryHandle.generated.h"



/**
 * 技能账本条目句柄（`FTcsStateHandle` 同族）：在**某个单位的技能桶**内定位一条已学技能条目，
 * 是账本一切操作的入参。
 *
 * **为什么展平两字段而不内嵌模板句柄**（与 `FTcsStateHandle` / `FTcsChainRunHandle` 同款）：
 * 模板类型**无法作 `UPROPERTY`**，脚本层往返会得到空壳 ⇒ 读不到值 / 传回全零 ⇒ 代际失配。
 * 展平为可反射标量后往返成立（`Index` 用 `int32` 而非 `uint32`：UHT 不支持 `uint32` 作属性类型，
 * 且 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同，转换无损）。
 *
 * **句柄相对其发放方有义**：`Index` / `Generation` 的语义由**桶**（`FSkillBucket`）定义——
 * 下标是桶内槽位；代际由本模块的出线发号器统一分配（进程内不复用的奇数号）。
 * 不同桶或新世界即使复用同一下标，也不会拿到相同的条目身份；释放后该槽代际 +1 置为偶数。
 * 句柄仍 MUST NOT 跨世界重用；旧世界身份在新门面上无法命中。
 *
 * **`Index` 可能为 0**（首个槽位）⇒ 有效性判据是"`Generation` 非 0"，**不是**"下标非 0"。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillEntryHandle
{
	GENERATED_BODY()

	// 桶内槽位索引（-1 = 无效）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill")
	int32 Index = -1;

	// 进程内不复用的分配代际（奇数）；释放置偶数，0 = 从未分配
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill")
	int32 Generation = 0;

	// 句柄有效性（代际校验由注册表在解析/释放时执行——本函数只判"是否曾被赋值"）
	bool IsValid() const
	{
		return Index >= 0 && Generation > 0;
	}

	// 相等比较（**身份 = Index + Generation**——级联收集与容器键控用）
	friend bool operator==(const FTcsSkillEntryHandle& A, const FTcsSkillEntryHandle& B)
	{
		return A.Index == B.Index && A.Generation == B.Generation;
	}

	// 哈希（TMap / TSet 键控用）
	friend uint32 GetTypeHash(const FTcsSkillEntryHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.Index), GetTypeHash(Handle.Generation));
	}
};
