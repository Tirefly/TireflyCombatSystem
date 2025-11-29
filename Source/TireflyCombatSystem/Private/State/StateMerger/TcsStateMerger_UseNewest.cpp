// Copyright Tirefly. All Rights Reserved.


#include "State/StateMerger/TcsStateMerger_UseNewest.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"



void UTcsStateMerger_UseNewest::Merge_Implementation(
	TArray<UTcsStateInstance*>& StatesToMerge,
	TArray<UTcsStateInstance*>& MergedStates)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	// 单个状态无需合并
	if (StatesToMerge.Num() == 1)
	{
		MergedStates.Add(StatesToMerge[0]);
		return;
	}

	// 获取第一个状态作为参考状态，用于获取StateDefId
	UTcsStateInstance* ReferenceState = StatesToMerge[0];
	const FName ReferenceStateDefId = ReferenceState->GetStateDefId();

	// 验证所有状态的StateDefId是否相同
	for (UTcsStateInstance* State : StatesToMerge)
	{
		if (State->GetStateDefId() != ReferenceStateDefId)
		{
			UE_LOG(LogTcsStateMerger, Error, TEXT("[%s] StateDefId mismatch."),
				*FString(__FUNCTION__));
			return;
		}
	}

	// 按时间戳排序，找出最新的状态（时间戳最大）
	UTcsStateInstance* NewestState = StatesToMerge[0];
	for (UTcsStateInstance* State : StatesToMerge)
	{
		if (State->GetApplyTimestamp() > NewestState->GetApplyTimestamp())
		{
			NewestState = State;
		}
	}

	// 只保留最新的状态
	MergedStates.Add(NewestState);

	// 将其他状态标记为待GC
	for (UTcsStateInstance* State : StatesToMerge)
	{
		if (State != NewestState)
		{
			State->MarkPendingGC();
		}
	}
}
