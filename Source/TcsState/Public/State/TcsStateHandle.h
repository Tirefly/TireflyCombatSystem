// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/TypeHash.h"

#include "TcsStateHandle.generated.h"



// 状态实例句柄标签（仅供句柄模板做类型区分——与其他池句柄编译期防互串）
struct FTcsStateTag
{
};



/**
 * 状态实例句柄（D3-1）：在**某个单位的状态桶**内定位一个状态实例，是所有状态操作的入参。
 *
 * **为什么展平两字段而不内嵌 `TTcsInstanceHandle`**（与 `FTcsChainRunHandle` / `FTcsEffectTriggerInstance` 同款）：
 * 模板类型**无法作 `UPROPERTY`**，脚本层往返会得到空壳 ⇒ 读不到值 / 传回全零 ⇒ 代际失配。
 * 展平为可反射标量后往返成立（`Index` 用 `int32` 而非 `uint32`：UHT 不支持 `uint32` 作属性类型，
 * 且 **`-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**，转换无损）。
 *
 * **句柄相对其发放方有义**：`Index` / `Generation` 的语义由**桶**（`FStateBucket`）定义——
 * 下标是桶内槽位、代际是桶内槽位复用计数（分配与释放各 +1，奇数 = 已分配）。
 * 故 MUST NOT 把句柄跨世界或跨门面重用（`Generation` 是全局防御网而非世界标识）。
 *
 * **`Index` 可能为 0**（首个槽位）⇒ 有效性判据是"`Generation` 非 0"，**不是**"下标非 0"。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsStateHandle
{
	GENERATED_BODY()

	// 桶内槽位索引（-1 = 无效）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State")
	int32 Index = -1;

	// 代际计数（分配与释放各 +1，首个有效代际为 1；0 = 从未分配）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State")
	int32 Generation = 0;

	// 句柄有效性（代际校验由注册表在解析/释放时执行——本函数只判"是否曾被赋值"）
	bool IsValid() const
	{
		return Index >= 0 && Generation > 0;
	}

	// 相等比较（**身份 = Index + Generation**——配对清理与容器键控用）
	friend bool operator==(const FTcsStateHandle& A, const FTcsStateHandle& B)
	{
		return A.Index == B.Index && A.Generation == B.Generation;
	}

	// 哈希（TMap / TSet 键控用）
	friend uint32 GetTypeHash(const FTcsStateHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.Index), GetTypeHash(Handle.Generation));
	}
};
