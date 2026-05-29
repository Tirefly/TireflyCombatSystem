// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeComponent.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"


void UTcsAttributeComponent::BroadcastAttributeStateDiffs(
	const TMap<FName, float>& PreviousBaseValues,
	const TMap<FName, float>& PreviousCurrentValues)
{
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BaseChangePayloads.Reserve(Attributes.Num());
	CurrentChangePayloads.Reserve(Attributes.Num());
	BoundaryPayloads.Reserve(Attributes.Num());

	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		const FName AttributeName = Pair.Key;
		const FTcsAttributeInstance& Attribute = Pair.Value;

		const float PreviousBase = PreviousBaseValues.FindRef(AttributeName);
		if (!FMath::IsNearlyEqual(PreviousBase, Attribute.BaseValue))
		{
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = PreviousBase;
			Payload.NewValue = Attribute.BaseValue;
			BaseChangePayloads.Add(Payload);

			float BoundaryCandidate = Attribute.BaseValue;
			float RangeMin = BoundaryCandidate;
			float RangeMax = BoundaryCandidate;
			ClampAttributeValueInRange(AttributeName, BoundaryCandidate, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(Attribute.BaseValue, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Attribute.BaseValue, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				BoundaryPayloads.Emplace(
					AttributeName,
					bReachedMax,
					PreviousBase,
					Attribute.BaseValue,
					bReachedMax ? RangeMax : RangeMin);
			}
		}

		const float PreviousCurrent = PreviousCurrentValues.FindRef(AttributeName);
		if (!FMath::IsNearlyEqual(PreviousCurrent, Attribute.CurrentValue))
		{
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = PreviousCurrent;
			Payload.NewValue = Attribute.CurrentValue;
			CurrentChangePayloads.Add(Payload);

			float BoundaryCandidate = Attribute.CurrentValue;
			float RangeMin = BoundaryCandidate;
			float RangeMax = BoundaryCandidate;
			ClampAttributeValueInRange(AttributeName, BoundaryCandidate, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				BoundaryPayloads.Emplace(
					AttributeName,
					bReachedMax,
					PreviousCurrent,
					Attribute.CurrentValue,
					bReachedMax ? RangeMax : RangeMin);
			}
		}
	}

	if (!BaseChangePayloads.IsEmpty())
	{
		BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}

	if (!CurrentChangePayloads.IsEmpty())
	{
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}

	BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
}

void UTcsAttributeComponent::RecalculateAttributeBaseValues(
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RecalculateAttributeBaseValues);

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(Modifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d InputModifiers=%d MergedModifiers=%d"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		Modifiers.Num(),
		MergedModifiers.Num());

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;
	ChangeEventPayloads.Reserve(Attributes.Num());

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	// 这些临时容器在整个循环里复用，避免每个 Modifier 都重新分配。
	TArray<FName> TouchedAttributes;
	TouchedAttributes.Reserve(4);
	TArray<float> TouchedOldValues;
	TouchedOldValues.Reserve(4);
	TMap<FName, float> FallbackLastModifiedResults;
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has null ModifierDef. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;
		if (!ModDef->ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}

		// 执行修改器
		auto Execution = ModDef->ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		TouchedAttributes.Reset();
		TouchedOldValues.Reset();
		// touched-report 只收窄“记录差异”的采样范围，不改变 Execute 的真实写入行为。
		const bool bHasTouchedReport = Execution->CollectTouchedAttributes(Modifier, TouchedAttributes);
		if (bHasTouchedReport)
		{
			// 先过滤掉无效名字，再只为声明 touched 的属性截取旧值。
			// 这样后面比较差异时就不需要复制整张 BaseValues。
			TouchedAttributes.RemoveAll([](const FName AttributeName) { return AttributeName.IsNone(); });
			TouchedOldValues.Reserve(TouchedAttributes.Num());
			for (const FName AttributeName : TouchedAttributes)
			{
				TouchedOldValues.Add(BaseValues.FindRef(AttributeName));
			}
		}
		else
		{
			// 未声明 touched 集时，保留原来的全表差异检测路径，确保行为正确。
			FallbackLastModifiedResults = BaseValues;
		}

		// BaseValue 重算阶段把同一张 BaseValues 同时作为 Base/Current 视图传入 Execute。
		// 这样复用现有执行器接口，但不会引入独立的 CurrentValue 计算状态。
		Execution->Execute(Modifier, BaseValues, BaseValues);

		// Execute 之后只做差异归因：把本次执行真正改动的值累计到 SourceHandle 对应的记录里。
		if (bHasTouchedReport)
		{
			for (int32 Index = 0; Index < TouchedAttributes.Num(); ++Index)
			{
				const FName AttributeName = TouchedAttributes[Index];
				const float OldValue = TouchedOldValues.IsValidIndex(Index) ? TouchedOldValues[Index] : BaseValues.FindRef(AttributeName);
				const float NewValue = BaseValues.FindRef(AttributeName);
				if (!FMath::IsNearlyEqual(NewValue, OldValue))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(AttributeName);
					Payload.AttributeName = AttributeName;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - OldValue;
				}
			}
		}
		else
		{
			for (const TPair<FName, float>& LastPair : FallbackLastModifiedResults)
			{
				const float& NewValue = BaseValues.FindRef(LastPair.Key);
				if (!FMath::IsNearlyEqual(NewValue, LastPair.Value))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
					Payload.AttributeName = LastPair.Key;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - LastPair.Value;
				}
			}
		}
	}

	// 这一轮先处理“属性自身定义的直接 Clamp”，并同步收集真实发生变化的属性。
	// 多跳依赖传播（例如 MaxHP 变化后 HP 再次受限）留给后面的 Enforce 阶段统一处理。
	TSet<FName> ChangedAttributes;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BoundaryPayloads.Reserve(BaseValues.Num());
	for (TPair<FName, float>& Pair : BaseValues)
	{
		if (FTcsAttributeInstance* Attribute = Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->BaseValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 前面如果已经按 SourceHandle 记录过贡献，这里只负责把最终 old/new 补齐到事件载荷里。
			if (FTcsAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
			{
				Payload->NewValue = Pair.Value;
				Payload->OldValue = Attribute->BaseValue;
			}

			// 把属性基础值的最终修改赋值
			const float OldValue = Attribute->BaseValue;
			Attribute->BaseValue = Pair.Value;
			ChangedAttributes.Add(Pair.Key);

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// BaseValue 广播发生在直接写回之后、范围传播之前。
	// 这里表达的是“基础值第一阶段结算结果”，而不是多跳 Clamp 完成后的全局稳定态。
	if (bBroadcastEvents && !ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}

	// 只有真实改过的属性才作为传播种子，避免无意义地从整张属性表重新出发。
	if (!ChangedAttributes.IsEmpty())
	{
		EnforceAttributeRangeConstraints(ChangedAttributes, bBroadcastEvents);
	}
}

void UTcsAttributeComponent::RecalculateAttributeCurrentValues(int64 ChangeBatchId, bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RecalculateAttributeCurrentValues);

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(AttributeModifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d StoredModifiers=%d MergedModifiers=%d BatchId=%lld"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		AttributeModifiers.Num(),
		MergedModifiers.Num(),
		ChangeBatchId);

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;
	ChangeEventPayloads.Reserve(Attributes.Num());

	// 获取属性基础值，用于计算
	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	// CurrentValue 的执行既可能读取 BaseValue，也可能在 CurrentValue 结果上继续叠加，
	// 因此这里显式保留 BaseValues 和 CurrentValuesToCalc 两张表。
	TMap<FName, float> CurrentValuesToCalc = BaseValues;
	// 这些容器专门服务于 touched-report 的局部差异记录。
	TArray<FName> TouchedAttributes;
	TouchedAttributes.Reserve(4);
	TArray<float> TouchedOldValues;
	TouchedOldValues.Reserve(4);
	TMap<FName, float> FallbackLastModifiedResults;

	// 执行属性修改器的修改计算
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has null ModifierDef. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;
		if (!ModDef->ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}

		// 执行修改器
		auto Execution = ModDef->ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		TouchedAttributes.Reset();
		TouchedOldValues.Reset();
		// CurrentValue 重算沿用同一套 touched-report 机制，未知执行器继续走全表 fallback。
		const bool bHasTouchedReport = Execution->CollectTouchedAttributes(Modifier, TouchedAttributes);
		if (bHasTouchedReport)
		{
			// 这里记录的是 CurrentValuesToCalc 的旧值，因为真正要统计的是当前值阶段的增量贡献。
			TouchedAttributes.RemoveAll([](const FName AttributeName) { return AttributeName.IsNone(); });
			TouchedOldValues.Reserve(TouchedAttributes.Num());
			for (const FName AttributeName : TouchedAttributes)
			{
				TouchedOldValues.Add(CurrentValuesToCalc.FindRef(AttributeName));
			}
		}
		else
		{
			// 未声明 touched 集时，保留原来的全表差异检测路径，确保行为正确。
			FallbackLastModifiedResults = CurrentValuesToCalc;
		}

		// BaseValues 作为只读输入参与计算，CurrentValuesToCalc 承接每个执行器的叠加结果。
		Execution->Execute(Modifier, BaseValues, CurrentValuesToCalc);

		// 只有命中本轮 BatchId 的修改器，才会把差异记进本次事件归因。
		// 这样可以避免旧修改器在全量重算时被误报成“本轮新增变化来源”。
		if (bHasTouchedReport)
		{
			for (int32 Index = 0; Index < TouchedAttributes.Num(); ++Index)
			{
				const FName AttributeName = TouchedAttributes[Index];
				const float OldValue = TouchedOldValues.IsValidIndex(Index) ? TouchedOldValues[Index] : CurrentValuesToCalc.FindRef(AttributeName);
				const float NewValue = CurrentValuesToCalc.FindRef(AttributeName);
				if (!FMath::IsNearlyEqual(NewValue, OldValue) && (ChangeBatchId < 0 || Modifier.LastTouchedBatchId == ChangeBatchId))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(AttributeName);
					Payload.AttributeName = AttributeName;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - OldValue;
				}
			}
		}
		else
		{
			for (const TPair<FName, float>& LastPair : FallbackLastModifiedResults)
			{
				const float& NewValue = CurrentValuesToCalc.FindRef(LastPair.Key);
				if (!FMath::IsNearlyEqual(NewValue, LastPair.Value) && (ChangeBatchId < 0 || Modifier.LastTouchedBatchId == ChangeBatchId))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
					Payload.AttributeName = LastPair.Key;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - LastPair.Value;
				}
			}
		}
	}

	// 这里先完成“每个属性自身”的当前值 Clamp 和写回。
	// 由于动态范围可能存在多跳依赖，真正的全局稳定化仍然交给后面的传播阶段。
	TSet<FName> ChangedAttributes;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BoundaryPayloads.Reserve(CurrentValuesToCalc.Num());
	for (TPair<FName, float>& Pair : CurrentValuesToCalc)
	{
		if (FTcsAttributeInstance* Attribute = Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->CurrentValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 修改器移除、合并等情况可能不会留下逐条 SourceHandle 差异记录，
			// 因此这里统一用最终 old/new 补齐当前值变化事件，避免漏报。
			FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(Pair.Key);
			Payload.AttributeName = Pair.Key;
			Payload.NewValue = Pair.Value;
			Payload.OldValue = Attribute->CurrentValue;

			// 把属性当前值的最终修改赋值
			const float OldValue = Attribute->CurrentValue;
			Attribute->CurrentValue = Pair.Value;
			ChangedAttributes.Add(Pair.Key);

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// 当前值广播反映的是本轮执行器求值后的直接结果；
	// 如果后续范围传播又把值压回去了，会再由传播阶段补发对应事件。
	if (bBroadcastEvents && !ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		BroadcastAttributeValueChangeEvent(Payloads);
	}

	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}

	// 增量批次更新时，只有真实变更过的属性才作为传播种子；全量重算仍走保守路径。
	if (ChangeBatchId >= 0 && !ChangedAttributes.IsEmpty())
	{
		EnforceAttributeRangeConstraints(ChangedAttributes, bBroadcastEvents);
	}
	else
	{
		EnforceAttributeRangeConstraints(bBroadcastEvents);
	}
}

void UTcsAttributeComponent::MergeAttributeModifiers(
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	MergedModifiers.Reset();
	MergedModifiers.Reserve(Modifiers.Num());

	// 按 ModifierId 整理所有属性修改器，方便后续执行修改器合并
	TMap<FName, TArray<FTcsAttributeModifierInstance>> ModifiersToMerge;
	ModifiersToMerge.Reserve(Modifiers.Num());
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		ModifiersToMerge.FindOrAdd(Modifier.ModifierId).Add(Modifier);
	}

	// 执行修改器合并
	for (TPair<FName, TArray<FTcsAttributeModifierInstance>>& Pair : ModifiersToMerge)
	{
		if (Pair.Value.IsEmpty())
		{
			continue;
		}

		if (!Pair.Value[0].ModifierDef)
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ModifierDef is null for ModifierId: %s"),
				*FString(__FUNCTION__),
				*Pair.Value[0].ModifierId.ToString());
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Pair.Value[0].ModifierDef;

		// No merger: do not merge, keep all instances.
		if (!ModDef->MergerType)
		{
			MergedModifiers.Append(Pair.Value);
			continue;
		}

		auto Merger = ModDef->MergerType->GetDefaultObject<UTcsAttributeModifierMerger>();
		Merger->Merge(Pair.Value, MergedModifiers);
	}
}

