// Copyright Tirefly. All Rights Reserved.


#include "State/StateMerger/TcsStateMerger_StackByInstigator.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"



void UTcsStateMerger_StackByInstigator::Merge_Implementation(
	TArray<UTcsStateInstance*>& StatesToMerge,
	TArray<UTcsStateInstance*>& MergedStates)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	// 获取第一个状态作为参考状态，用于获取StateDefId
	UTcsStateInstance* ReferenceState = StatesToMerge[0];
	const FName ReferenceStateDefId = ReferenceState->GetStateDefId();

	// 按Instigator分组
	TMap<AActor*, TArray<UTcsStateInstance*>> StatesByInstigator;
	for (UTcsStateInstance* State : StatesToMerge)
	{
		// 验证StateDefId是否相同
		if (State->GetStateDefId() != ReferenceStateDefId)
		{
			UE_LOG(LogTcsStateMerger, Error, TEXT("[%s] StateDefId mismatch."),
				*FString(__FUNCTION__));
			return;
		}

		AActor* Instigator = State->GetInstigator();
		if (!StatesByInstigator.Contains(Instigator))
		{
			StatesByInstigator.Add(Instigator, TArray<UTcsStateInstance*>());
		}
		StatesByInstigator[Instigator].Add(State);
	}

	// 对每个Instigator的状态进行合并
	for (auto& InstigatorStates : StatesByInstigator)
	{
		TArray<UTcsStateInstance*>& States = InstigatorStates.Value;
		
		/**
		 * 按时间戳排序，最旧的状态在数组最后，也就是栈顶，
		 * 之所以选择最旧的状态实例作为栈顶基础状态，因为
		 * 该状态更早被应用，状态树已经开始执行，剩余持续
		 * 时间也已经开始计时
		 */
		States.Sort([](
			const UTcsStateInstance& A,
			const UTcsStateInstance& B) {
			return A.GetApplyTimestamp() > B.GetApplyTimestamp();
		});

		// 计算该Instigator的总叠层数
		int32 TotalStackCount = 0;
		for (UTcsStateInstance* State : States)
		{
			const int32 StateStackCount = State->GetStackCount();
			if (StateStackCount < 0)
			{
				UE_LOG(LogTcsStateMerger, Error,
					TEXT("[%s] State '%s' has invalid StackCount (%d). "
						 "Check DataTable config: MaxStackCount should be > 0 when using StackByInstigator Merger."),
					*FString(__FUNCTION__),
					*State->GetStateDefId().ToString(),
					StateStackCount);
				return;
			}
			TotalStackCount += StateStackCount;
		}

		// 从排序后的状态中弹出最新的状态作为基础状态
		UTcsStateInstance* BaseState = States.Pop();
		
		// 设置基础状态的叠层数
		BaseState->SetStackCount(TotalStackCount);
		MergedStates.Add(BaseState);
	}
} 
