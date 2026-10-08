// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/TypeHash.h"

#include "TcsCastRunHandle.generated.h"



/**
 * 施法运行态句柄（**本轮只落纯句柄值类型**，池与运行态归后续任务）：
 * 它是 `FTcsLearnedSkillEntry::RunHandles` 的元素类型，用于从账本条目反查"该技能当前在飞的施法运行"。
 *
 * **本轮零写入者**（如实登记）：写入者是激活路径（`TryActivate` 建 run 后回填本数组），
 * 归后续任务；本类型此刻只承担"数组元素类型必须完整"这一编译期要求。
 * **池（`TTcsInstancePool<FTcsCastRun, FTcsCastRunTag>`）与 `FTcsCastRun` 运行态结构体 MUST NOT 在本轮建**——
 * 池含 `TArray<T> Instances` ⇒ **实例化要求 `T` 完整**，而 `T` 是后续任务的产物；
 * 提前建即"未声明标识符"（硬编译错误，同 `ParamChainRows` 那次的判据）。
 *
 * **展平形态与 `int32` 字段**：照 `FTcsChainRunHandle`（`TcsChainRun.h:52`）——模板句柄无法作 `UPROPERTY`，
 * 展平后可反射、脚本往返成立；`Index` 取 `int32`（UHT 不支持 `uint32` 作属性类型），
 * `-1` 与池的 `InvalidIndex` 位模式相同。
 *
 * **`Index` 可能为 0** ⇒ 有效性判据是"`Index != -1`"（与 `FTcsChainRunHandle` 同款判定，
 * 因其代际由池在解析/释放时校验，本结构只判"是否曾被赋值"）。
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
