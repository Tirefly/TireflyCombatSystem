// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsInstanceHandle.h"
#include "Templates/TypeHash.h"

#include "TcsCastRunHandle.generated.h"



// 施法运行态句柄标签（仅供句柄模板做类型区分——与其他池句柄编译期防互串）
struct FTcsCastRunTag
{
};



/**
 * 施法运行态句柄：`FTcsLearnedSkillEntry::RunHandles` 的元素类型，亦是从账本条目反查
 * "该技能当前在飞的施法运行"的唯一身份词。
 *
 * **写入者自 Task 3 起存在**（激活路径建 run 后回填、终结时摘除）——本类型原为 Task 2 落地的
 * "纯句柄值类型、零写入者"，该状态已终结。
 *
 * **展平形态与 `int32` 字段**：照 `FTcsChainRunHandle`——模板句柄无法作 `UPROPERTY`，
 * 展平后可反射、脚本往返成立；`Index` 取 `int32`（UHT 不支持 `uint32` 作属性类型），
 * `-1` 与池的 `InvalidIndex`（`0xFFFFFFFF`）位模式相同。
 *
 * **`Index` 可能为 0** ⇒ 有效性判据是"`Index != -1`"（与 `FTcsChainRunHandle` 同款判定，
 * 因其代际由池在解析/释放时校验，本结构只判"是否曾被赋值"）。
 *
 * **`GetInner` / `SetInner` 是必需项（Task 3 补入）**：池的 `Allocate` / `Free` / `Resolve`
 * 收发的是 `TTcsInstanceHandle<FTcsCastRunTag>`，而本结构是**展平的**两个 `int32`——
 * 两者**位模式一致但不是同一类型** ⇒ 缺这对转换，本句柄**无法进池**（硬编译错误）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCastRunHandle
{
	GENERATED_BODY()

	// 池内槽位索引（-1 = 无效）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast")
	int32 Index = -1;

	// 代际计数（释放时 +1，旧句柄凭失配判悬空）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast")
	int32 Generation = 0;

	// 句柄有效性（代际校验由池在解析/释放时执行——本函数只判"是否曾被赋值"）
	bool IsValid() const
	{
		return Index != -1;
	}

	/**
	 * 转为池句柄（**唯一转换点**——避免各处自行拼装出语义不一致的句柄）。
	 *
	 * `static_cast` 保证位模式一致：`-1` → `0xFFFFFFFF`（池的 `InvalidIndex`）。
	 */
	TTcsInstanceHandle<FTcsCastRunTag> GetInner() const
	{
		TTcsInstanceHandle<FTcsCastRunTag> Inner;
		Inner.Index = static_cast<uint32>(Index);
		Inner.Generation = static_cast<uint32>(Generation);
		return Inner;
	}

	// 从池句柄赋值（**唯一转换点**，与 `GetInner` 对称）
	void SetInner(const TTcsInstanceHandle<FTcsCastRunTag>& Inner)
	{
		Index = static_cast<int32>(Inner.Index);
		Generation = static_cast<int32>(Inner.Generation);
	}

	// 相等比较（**身份 = Index + Generation**——在飞集合的配对摘除用）
	friend bool operator==(const FTcsCastRunHandle& A, const FTcsCastRunHandle& B)
	{
		return A.Index == B.Index && A.Generation == B.Generation;
	}

	// 哈希（TMap / TSet 键控用）
	friend uint32 GetTypeHash(const FTcsCastRunHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.Index), GetTypeHash(Handle.Generation));
	}
};
