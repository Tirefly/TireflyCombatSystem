// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsInstanceHandle.h"
#include "Templates/TypeHash.h"

#include "Runtime/TcsInstanceHandleOps.h"

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
 * **`Index` 可能为 0** ⇒ 有效性判据不是"下标非 0"。
 *
 * **★ 判据已于 2026-10-14 统一（本注释同批改述，MUST 读）**：原先判 `Index != -1`
 * （**不看 `Generation`**），现与另两个句柄一同**委托** `FTcsInstanceHandleOps::IsValid`
 * ⇒ 判据 = `Index >= 0 && Generation > 0`。
 *
 * **为什么改**：原文自述"**因其代际由池在解析/释放时校验，本结构只判『是否曾被赋值』**"——
 * 该理由只解释了"本结构不做**代际校验**"（那确实仍归池），**不能推出"可以完全无视 `Generation`"**。
 * `Generation == 0` 的语义是"**该槽从未分配过**"，而 `Index >= 0` 与它的组合只可能出现在
 * **手工拼装的畸形句柄**上 ⇒ 那种句柄会在解析处才失败，症状离成因很远。判据 MUST 在**句柄自己
 * 身上**就拒绝它。（代际**校验**仍归池——`IsValid` 判的是"这个句柄是否**可能**有效"。）
 *
 * **行为中性（MUST 记录，防误读为行为变更）**：对良构句柄（`Allocate` 产物 `Generation >= 1`、
 * 默认构造产物 `Index = -1`）两套判据**结论逐例相同**；仅"手工构造 `Index >= 0` 且
 * `Generation == 0`"这一**从不出现的形态**由"有效"变为"无效"。
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

	// 句柄有效性（**判据委托 `FTcsInstanceHandleOps`**——三句柄语义单点，本型不自行实现）
	bool IsValid() const
	{
		return FTcsInstanceHandleOps::IsValid(Index, Generation);
	}

	/**
	 * 转为池句柄（**唯一转换点**——避免各处自行拼装出语义不一致的句柄）。
	 *
	 * 语义实现在 `FTcsInstanceHandleOps::ToInner`（与另两个句柄同一处）；
	 * `static_cast` 保证位模式一致：`-1` → `0xFFFFFFFF`（池的 `InvalidIndex`）。
	 */
	TTcsInstanceHandle<FTcsCastRunTag> GetInner() const
	{
		return FTcsInstanceHandleOps::ToInner<FTcsCastRunTag>(Index, Generation);
	}

	// 从池句柄赋值（**唯一转换点**，与 `GetInner` 对称）
	void SetInner(const TTcsInstanceHandle<FTcsCastRunTag>& Inner)
	{
		FTcsInstanceHandleOps::FromInner(Inner, Index, Generation);
	}

	// 相等比较（**身份 = Index + Generation**——在飞集合的配对摘除用；语义同单点）
	friend bool operator==(const FTcsCastRunHandle& A, const FTcsCastRunHandle& B)
	{
		return FTcsInstanceHandleOps::IsSameIdentity(A.Index, A.Generation, B.Index, B.Generation);
	}

	// 哈希（TMap / TSet 键控用；语义同单点）
	friend uint32 GetTypeHash(const FTcsCastRunHandle& Handle)
	{
		return FTcsInstanceHandleOps::Hash(Handle.Index, Handle.Generation);
	}
};
