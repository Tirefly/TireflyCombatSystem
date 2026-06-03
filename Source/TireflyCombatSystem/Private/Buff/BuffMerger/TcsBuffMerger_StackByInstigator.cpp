// Copyright Tirefly. All Rights Reserved.

#include "Buff/BuffMerger/TcsBuffMerger_StackByInstigator.h"

#include "Buff/TcsBuffInstance.h"
#include "TcsLogChannels.h"



namespace
{
	// 按发起者缓存合并时需要的中间数据，避免分组完成后再额外扫描一轮。
	struct FTcsInstigatorMergeGroup
	{
		TArray<UTcsBuffInstance*> Buffs;
		UTcsBuffInstance* OldestBuff = nullptr;
		int64 OldestApplyTimestamp = TNumericLimits<int64>::Max();
		int32 TotalStackCount = 0;
	};
}



ETcsBuffMergeDependencyFlags UTcsBuffMerger_StackByInstigator::GetDependencyFlags_Implementation() const
{
	return ETcsBuffMergeDependencyFlags::MemberSet
		| ETcsBuffMergeDependencyFlags::Instigator
		| ETcsBuffMergeDependencyFlags::RuntimeStack;
}



void UTcsBuffMerger_StackByInstigator::Merge_Implementation(
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

	// 获取第一个状态作为参考状态，用于获取StateDefId
	UTcsBuffInstance* ReferenceBuff = BuffsToMerge[0];
	const FName ReferenceStateDefId = ReferenceBuff->GetStateDefId();

	// 按Instigator分组
	TMap<AActor*, FTcsInstigatorMergeGroup> StatesByInstigator;
	for (UTcsBuffInstance* Buff : BuffsToMerge)
	{
		// 验证StateDefId是否相同
		if (Buff->GetStateDefId() != ReferenceStateDefId)
		{
			UE_LOG(LogTcsBuffMerger, Error, TEXT("[%s] StateDefId mismatch."),
				*FString(__FUNCTION__));
			return;
		}

		const int32 StateStackCount = Buff->GetStackCount();
		if (StateStackCount < 0)
		{
			UE_LOG(LogTcsBuffMerger, Error,
				TEXT("[%s] State '%s' has invalid StackCount (%d). "
					 "Check DataTable config: MaxStackCount should be > 0 when using StackByInstigator Merger."),
				*FString(__FUNCTION__),
				*Buff->GetStateDefId().ToString(),
				StateStackCount);
			return;
		}

		AActor* Instigator = Buff->GetInstigator();
		FTcsInstigatorMergeGroup& MergeGroup = StatesByInstigator.FindOrAdd(Instigator);
		MergeGroup.Buffs.Add(Buff);

		const int64 ApplyTimestamp = Buff->GetApplyTimestamp();
		if (!MergeGroup.OldestBuff || ApplyTimestamp < MergeGroup.OldestApplyTimestamp)
		{
			MergeGroup.OldestBuff = Buff;
			MergeGroup.OldestApplyTimestamp = ApplyTimestamp;
		}

		// StackCount为0的状态即将被移除，跳过叠层累计，但仍保留在分组中以便后续淘汰。
		if (StateStackCount == 0)
		{
			UE_LOG(LogTcsBuffMerger, Verbose,
				TEXT("[%s] Skipping state '%s' with StackCount=0 (pending removal)"),
				*FString(__FUNCTION__),
				*Buff->GetStateDefId().ToString());
			continue;
		}

		MergeGroup.TotalStackCount += StateStackCount;
	}

	// 对每个Instigator的状态进行合并
	for (auto& InstigatorStates : StatesByInstigator)
	{
		FTcsInstigatorMergeGroup& MergeGroup = InstigatorStates.Value;
		UTcsBuffInstance* BaseBuffState = MergeGroup.OldestBuff;
		if (!BaseBuffState)
		{
			UE_LOG(LogTcsBuffMerger, Error, TEXT("[%s] Failed to resolve base buff instance."), *FString(__FUNCTION__));
			return;
		}

		// 设置基础状态的叠层数
		BaseBuffState->SetStackCount(MergeGroup.TotalStackCount);
		MergedBuffs.Add(BaseBuffState);
		MergeGroup.Buffs.RemoveSingle(BaseBuffState);
		MergedOutBuffs.Append(MergeGroup.Buffs);
	}
}