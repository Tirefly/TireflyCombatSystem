// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/TypeHash.h"

#include "Runtime/TcsInstanceHandleOps.h"

#include "TcsBuffInstanceHandle.generated.h"



// 状态实例句柄标签（仅供句柄模板做类型区分——与其他池句柄编译期防互串）
struct FTcsBuffInstanceTag
{
};



/**
 * 状态实例句柄（D3-1）：在**某个实体的状态桶**内定位一个状态实例，是所有状态操作的入参。
 *
 * **为什么展平两字段而不内嵌 `TTcsInstanceHandle`**（与 `FTcsChainRunHandle` / `FTcsEffectTriggerInstance` 同款）：
 * 模板类型**无法作 `UPROPERTY`**，脚本层往返会得到空壳 ⇒ 读不到值 / 传回全零 ⇒ 代际失配。
 * 展平为可反射标量后往返成立（`Index` 用 `int32` 而非 `uint32`：UHT 不支持 `uint32` 作属性类型，
 * 且 **`-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**，转换无损）。
 *
 * **句柄相对其发放方有义**：`Index` / `Generation` 的语义由**桶**（`FStateBucket`）定义——
 * 下标是桶内槽位；代际由 TcsState 出线发号器统一分配（进程内不复用的奇数号）。
 * 不同桶或新世界即使复用同一下标，也不会拿到相同的实例身份；释放后该槽代际 +1 置为偶数。
 * 句柄仍 MUST NOT 跨世界重用；旧世界身份在新门面上无法命中。
 *
 * **`Index` 可能为 0**（首个槽位）⇒ 有效性判据是"`Generation` 非 0"，**不是**"下标非 0"。
 */
USTRUCT(BlueprintType)
struct TCSBUFF_API FTcsBuffInstanceHandle
{
	GENERATED_BODY()

	// 桶内槽位索引（-1 = 无效）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State")
	int32 Index = -1;

	// 进程内不复用的分配代际（奇数）；释放置偶数，0 = 从未分配
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State")
	int32 Generation = 0;

	// 句柄有效性（**判据委托 `FTcsInstanceHandleOps`**——三句柄语义单点，本型不自行实现）
	bool IsValid() const
	{
		return FTcsInstanceHandleOps::IsValid(Index, Generation);
	}

	// 转为池句柄（**唯一转换点**的委托口——语义在 `FTcsInstanceHandleOps::ToInner`）
	TTcsInstanceHandle<FTcsBuffInstanceTag> GetInner() const
	{
		return FTcsInstanceHandleOps::ToInner<FTcsBuffInstanceTag>(Index, Generation);
	}

	// 从池句柄赋值（**唯一转换点**的委托口，与 `GetInner` 对称）
	void SetInner(const TTcsInstanceHandle<FTcsBuffInstanceTag>& Inner)
	{
		FTcsInstanceHandleOps::FromInner(Inner, Index, Generation);
	}

	// 相等比较（**身份 = Index + Generation**——配对清理与容器键控用；语义同单点）
	friend bool operator==(const FTcsBuffInstanceHandle& A, const FTcsBuffInstanceHandle& B)
	{
		return FTcsInstanceHandleOps::IsSameIdentity(A.Index, A.Generation, B.Index, B.Generation);
	}

	// 哈希（TMap / TSet 键控用；语义同单点）
	friend uint32 GetTypeHash(const FTcsBuffInstanceHandle& Handle)
	{
		return FTcsInstanceHandleOps::Hash(Handle.Index, Handle.Generation);
	}
};
