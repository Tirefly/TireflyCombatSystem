// Copyright Tirefly. All Rights Reserved.

#include "Buff/BuffMerger/TcsBuffMerger_UseOldest.h"

#include "Buff/TcsBuffInstance.h"
#include "TcsLogChannels.h"



ETcsBuffMergeDependencyFlags UTcsBuffMerger_UseOldest::GetDependencyFlags_Implementation() const
{
	return ETcsBuffMergeDependencyFlags::MemberSet | ETcsBuffMergeDependencyFlags::ApplyTimestamp;
}



void UTcsBuffMerger_UseOldest::Merge_Implementation(
	TArray<UTcsBuffInstance*>& BuffsToMerge,
	TArray<UTcsBuffInstance*>& MergedBuffs,
	TArray<UTcsBuffInstance*>& MergedOutBuffs)
{
	MergedBuffs.Reset();
	MergedOutBuffs.Reset();

	if (BuffsToMerge.IsEmpty())
	{
		return;
	}

	// 单个状态无需合并
	if (BuffsToMerge.Num() == 1)
	{
		MergedBuffs.Add(BuffsToMerge[0]);
		return;
	}

	// 获取第一个状态作为参考状态，用于获取StateDefId
	UTcsBuffInstance* ReferenceBuff = BuffsToMerge[0];
	const FName ReferenceStateDefId = ReferenceBuff->GetStateDefId();

	// 验证所有状态的StateDefId是否相同
	for (UTcsBuffInstance* Buff : BuffsToMerge)
	{
		if (Buff->GetStateDefId() != ReferenceStateDefId)
		{
			UE_LOG(LogTcsBuffMerger, Error, TEXT("[%s] StateDefId mismatch."),
				*FString(__FUNCTION__));
			return;
		}
	}

	// 按时间戳排序，找出最旧的状态（时间戳最小）
	// NOTE: Instigator-agnostic; always keep the oldest instance by timestamp.
	UTcsBuffInstance* OldestBuff = BuffsToMerge[0];
	for (UTcsBuffInstance* Buff : BuffsToMerge)
	{
		if (Buff->GetApplyTimestamp() < OldestBuff->GetApplyTimestamp())
		{
			OldestBuff = Buff;
		}
	}

	// 只保留最旧的状态
	MergedBuffs.Add(OldestBuff);
	for (UTcsBuffInstance* Buff : BuffsToMerge)
	{
		if (Buff != OldestBuff)
		{
			MergedOutBuffs.Add(Buff);
		}
	}

	// 其它状态的移除/回收由 StateManagerSubsystem 负责
	// NOTE: Merger should not decide lifetime/GC; subsystem will remove unmerged instances.
}