// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeManagerSubsystem.h"

#include "TcsEntityInterface.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttribute.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"


void UTcsAttributeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	AttributeDefTable = UTcsGenericLibrary::GetAttributeDefTable();
	if (!IsValid(AttributeDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"),
			*FString(__FUNCTION__));
		return;
	}

	AttributeModifierDefTable = UTcsGenericLibrary::GetAttributeModifierDefTable();
	if (!IsValid(AttributeModifierDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable in TcsDevSettings is not valid"),
			*FString(__FUNCTION__));
	}
}

void UTcsAttributeManagerSubsystem::AddAttribute(
	AActor* CombatEntity,
	FName AttributeName,
	float InitValue)
{
	if (!IsValid(AttributeDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"),
			*FString(__FUNCTION__));
		return;
	}
	
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return;
	}

	const auto AttrDef = AttributeDefTable->FindRow<FTcsAttributeDefinition>(
		AttributeName,
		*FString(__FUNCTION__));
	if (!AttrDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable does not contain AttributeName %s"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
		return;
	}

	FTcsAttributeInstance AttrInst = FTcsAttributeInstance(*AttrDef, ++GlobalAttributeInstanceIdMgr, CombatEntity, InitValue);
	AttributeComponent->Attributes.Add(AttributeName, AttrInst);
}

void UTcsAttributeManagerSubsystem::AddAttributes(AActor* CombatEntity, const TArray<FName>& AttributeNames)
{
	if (!IsValid(AttributeDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"),
			*FString(__FUNCTION__));
		return;
	}
	
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return;
	}

	for (const FName AttributeName : AttributeNames)
	{
		const auto AttrDef = AttributeDefTable->FindRow<FTcsAttributeDefinition>(AttributeName, FString(__FUNCTION__));
		if (!AttrDef)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable does not contain AttributeName %s"),
				*FString(__FUNCTION__),
				*AttributeName.ToString());
			continue;
		}

		FTcsAttributeInstance AttrInst = FTcsAttributeInstance(*AttrDef, ++GlobalAttributeInstanceIdMgr, CombatEntity);
		AttributeComponent->Attributes.Add(AttributeName, AttrInst);
	}
}

UTcsAttributeComponent* UTcsAttributeManagerSubsystem::GetAttributeComponent(const AActor* CombatEntity)
{
	if (!IsValid(CombatEntity))
	{
		return nullptr;
	}

	if (CombatEntity->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetAttributeComponent(CombatEntity);
	}

	return CombatEntity->FindComponentByClass<UTcsAttributeComponent>();
}

bool UTcsAttributeManagerSubsystem::CreateAttributeModifier(
	FName ModifierId,
	FName SourceName,
	AActor* Instigator,
	AActor* Target,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	if (!IsValid(AttributeModifierDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable in TcsDevSettings is not valid"),
			*FString(__FUNCTION__));
		return false;
	}
	
	if (!IsValid(Instigator) || !IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator or Target is not valid"), *FString(__FUNCTION__));
		return false;
	}

	FTcsAttributeModifierDefinition* ModifierDef = AttributeModifierDefTable->FindRow<FTcsAttributeModifierDefinition>(ModifierId, FString(__FUNCTION__));
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable does not contain ModifierId %s"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	// TODO: 验证Source是否有效

	OutModifierInst = FTcsAttributeModifierInstance();
	OutModifierInst.ModifierDef = *ModifierDef;
	if (OutModifierInst.ModifierDef.Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, clamped to 0."),
			*FString(__FUNCTION__),
			*OutModifierInst.ModifierDef.ModifierName.ToString(),
			OutModifierInst.ModifierDef.Priority);
		OutModifierInst.ModifierDef.Priority = 0;
	}
	OutModifierInst.ModifierInstId = ++GlobalAttributeModifierInstanceIdMgr;
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = Target;
	OutModifierInst.Operands = ModifierDef->Operands;
	OutModifierInst.SourceName = SourceName;

	return true;
}

bool UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands(
	FName ModifierId,
	FName SourceName,
	AActor* Instigator,
	AActor* Target,
	const TMap<FName, float>& Operands,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	if (!IsValid(Instigator) || !IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator or Target is not valid"),
			*FString(__FUNCTION__));
		return false;
	}
	
	if (!IsValid(AttributeModifierDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable in TcsDevSettings is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	FTcsAttributeModifierDefinition* ModifierDef = AttributeModifierDefTable->FindRow<FTcsAttributeModifierDefinition>(ModifierId, FString(__FUNCTION__));
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable does not contain ModifierId %s"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	// TODO: 验证Source是否有效
	
	// 验证Operands是否正确
	if (ModifierDef->Operands.IsEmpty())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] ModifierDef does not contain Operands"),
			*FString(__FUNCTION__));
		return false;
	}
	for (const TPair<FName, float>& Operand : ModifierDef->Operands)
	{
		if (!Operands.Contains(Operand.Key))
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Operand %s is not found in Operands"),
				*FString(__FUNCTION__),
				*Operand.Key.ToString());
			return false;
		}
	}

	OutModifierInst = FTcsAttributeModifierInstance();
	OutModifierInst.ModifierDef = *ModifierDef;
	if (OutModifierInst.ModifierDef.Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, clamped to 0."),
			*FString(__FUNCTION__),
			*OutModifierInst.ModifierDef.ModifierName.ToString(),
			OutModifierInst.ModifierDef.Priority);
		OutModifierInst.ModifierDef.Priority = 0;
	}
	OutModifierInst.ModifierInstId = ++GlobalAttributeModifierInstanceIdMgr;
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = Target;
	OutModifierInst.Operands = Operands;
	OutModifierInst.SourceName = SourceName;

	return true;
}

void UTcsAttributeManagerSubsystem::ApplyModifier(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent) || Modifiers.IsEmpty())
	{
		return;
	}

	TArray<FTcsAttributeModifierInstance> ModifiersToExecute;// 对属性Base值执行操作的属性修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToApply;// 对属性Current值应用的属性修改器
	const int64 BatchId = ++GlobalAttributeModifierChangeBatchIdMgr;
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();

	// 区分好修改属性Base值和Current值的两种修改器
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		switch (Modifier.ModifierDef.ModifierMode)
		{
		case ETcsAttributeModifierMode::AMM_BaseValue:
			{
				// BaseValue modifiers are executed immediately (not persisted), but we still stamp them for
				// deterministic merging policies (e.g., UseNewest/UseOldest) and debugging.
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;

				// 添加到执行列表
				ModifiersToExecute.Add(Modifier);
				break;
			}
		case ETcsAttributeModifierMode::AMM_CurrentValue:
			{
				// 添加到应用列表并设置应用时间戳
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToApply.Add(Modifier);
				break;
			}
		}
	}

	// 先执行针对属性Base值的修改器
	if (!ModifiersToExecute.IsEmpty())
	{
		RecalculateAttributeBaseValues(CombatEntity, ModifiersToExecute);
	}
	
	// 再执行针对属性Current值的修改器
	if (!ModifiersToApply.IsEmpty())
	{
		TArray<FTcsAttributeModifierInstance> NewlyAddedModifiers;

		// 把已经用过但有改变的属性修改器更新一下，并从待应用列表中移除
		for (FTcsAttributeModifierInstance& Modifier : AttributeComponent->AttributeModifiers)
		{
			if (ModifiersToApply.Contains(Modifier))
			{
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToApply.Remove(Modifier);
			}
		}

		// 剩余的 ModifiersToApply 即为新增的修改器
		NewlyAddedModifiers = ModifiersToApply;

		// 添加新修改器并更新索引
		for (const FTcsAttributeModifierInstance& Modifier : ModifiersToApply)
		{
			FTcsAttributeModifierInstance ModifierToStore = Modifier;
			ModifierToStore.LastTouchedBatchId = BatchId;
			int32 NewIndex = AttributeComponent->AttributeModifiers.Add(ModifierToStore);

			// 更新 SourceHandle 索引
			if (ModifierToStore.SourceHandle.IsValid())
			{
				TArray<int32>& Indices = AttributeComponent->SourceHandleIdToModifierIndices.FindOrAdd(ModifierToStore.SourceHandle.Id);
				Indices.Add(NewIndex);
			}
		}

		// 广播新增事件
		for (const FTcsAttributeModifierInstance& Added : NewlyAddedModifiers)
		{
			AttributeComponent->BroadcastAttributeModifierAddedEvent(Added);
		}
	}
	
	// 无论如何，都要重新计算属性Current值
	RecalculateAttributeCurrentValues(CombatEntity, BatchId);
}

void UTcsAttributeManagerSubsystem::RemoveModifier(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const int64 BatchId = ++GlobalAttributeModifierChangeBatchIdMgr;
	bool bModified = false;
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		// 查找并移除修改器
		int32 RemovedIndex = AttributeComponent->AttributeModifiers.IndexOfByKey(Modifier);
		if (RemovedIndex != INDEX_NONE)
		{
			// 从索引中移除
			if (Modifier.SourceHandle.IsValid())
			{
				TArray<int32>* IndicesPtr = AttributeComponent->SourceHandleIdToModifierIndices.Find(Modifier.SourceHandle.Id);
				if (IndicesPtr)
				{
					IndicesPtr->Remove(RemovedIndex);
					if (IndicesPtr->Num() == 0)
					{
						AttributeComponent->SourceHandleIdToModifierIndices.Remove(Modifier.SourceHandle.Id);
					}
				}
			}

			// 移除修改器
			AttributeComponent->AttributeModifiers.RemoveAt(RemovedIndex);

			// 更新后续索引 (因为数组元素前移)
			for (auto& Pair : AttributeComponent->SourceHandleIdToModifierIndices)
			{
				for (int32& Index : Pair.Value)
				{
					if (Index > RemovedIndex)
					{
						Index--;
					}
				}
			}

			AttributeComponent->BroadcastAttributeModifierRemovedEvent(Modifier);
			bModified = true;
		}
	}

	// 如果确实有属性修改器被移除，则更新属性的当前值
	if (bModified)
	{
		RecalculateAttributeCurrentValues(CombatEntity, BatchId);
	}
}

void UTcsAttributeManagerSubsystem::HandleModifierUpdated(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	bool bModified = false;
	const int64 BatchId = ++GlobalAttributeModifierChangeBatchIdMgr;
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		if (AttributeComponent->AttributeModifiers.Contains(Modifier))
		{
			const int32 ModifierInstId = AttributeComponent->AttributeModifiers.Find(Modifier);
			Modifier.UpdateTimestamp = UtcNowTicks;
			Modifier.LastTouchedBatchId = BatchId;
			AttributeComponent->AttributeModifiers[ModifierInstId] = Modifier;

			AttributeComponent->BroadcastAttributeModifierUpdatedEvent(Modifier);
			
			bModified = true;
		}
	}

	if (bModified)
	{
		RecalculateAttributeCurrentValues(CombatEntity, BatchId);
	}
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeBaseValues(
	const AActor* CombatEntity,
	const TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(CombatEntity, Modifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef.ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__), 
				*Modifier.ModifierDef.ModifierName.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}

		// 缓存属性基础值的上一次修改最终值
		TMap<FName, float> LastModifiedResults = BaseValues;

		// 执行修改器
		auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, BaseValues);

		// 记录属性修改过程
		for (const TPair<FName, float>& LastPair : LastModifiedResults)
		{
			const float& NewValue = BaseValues.FindRef(LastPair.Key);
			if (NewValue != LastPair.Value)
			{
				FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
				Payload.AttributeName = LastPair.Key;
				float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
				PayloadValue += NewValue - LastPair.Value;
			}
		}
	}
	
	// 对修改后的属性基础值进行范围修正，然后更新属性基础值
	for (TPair<FName, float>& Pair : BaseValues)
	{
		if (FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->BaseValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 记录属性修改事件的最终结果
			if (FTcsAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
			{
				Payload->NewValue = Pair.Value;
				Payload->OldValue = Attribute->BaseValue;
			}

			// 把属性基础值的最终修改赋值
			const float OldValue = Attribute->BaseValue;
			Attribute->BaseValue = Pair.Value;

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				AttributeComponent->BroadcastAttributeReachedBoundaryEvent(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// 属性基础值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(Payloads);
	}
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeCurrentValues(const AActor* CombatEntity, int64 ChangeBatchId)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(CombatEntity, AttributeComponent->AttributeModifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;

	// 获取属性基础值，用于计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	// 重新计算属性当前新值前，保存属性旧值
	TMap<FName, float> OldCurrentValues = AttributeComponent->GetAttributeValues();
	// 用于更新计算的临时属性值容器，基于属性的基础值
	TMap<FName, float> CurrentValuesToCalc = BaseValues;

	// 执行属性修改器的修改计算
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef.ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__), 
				*Modifier.ModifierDef.ModifierName.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}

		// 缓存属性当前值的上一次修改最终值
		TMap<FName, float> LastModifiedResults = CurrentValuesToCalc;

		// 执行修改器
		auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, CurrentValuesToCalc);

		// 记录属性修改过程，需要属性修改器的更新时间为最新
		for (const TPair<FName, float>& LastPair : LastModifiedResults)
		{
			const float& NewValue = CurrentValuesToCalc.FindRef(LastPair.Key);
			if (NewValue != LastPair.Value && (ChangeBatchId < 0 || Modifier.LastTouchedBatchId == ChangeBatchId))
			{
				FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
				Payload.AttributeName = LastPair.Key;
				float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
				PayloadValue += NewValue - LastPair.Value;
			}
		}
	}

	// 对修改后的属性当前值进行范围修正，然后更新属性当前值
	for (TPair<FName, float>& Pair : CurrentValuesToCalc)
	{
		if (FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->CurrentValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 记录属性修改事件的最终结果，因为修改器移除导致的属性当前值变更，不会有修改记录，所以需要在此处查漏补缺
			FTcsAttributeChangeEventPayload & Payload = ChangeEventPayloads.FindOrAdd(Pair.Key);
			Payload.AttributeName = Pair.Key;
			Payload.NewValue = Pair.Value;
			Payload.OldValue = Attribute->CurrentValue;

			// 把属性当前值的最终修改赋值
			const float OldValue = Attribute->CurrentValue;
			Attribute->CurrentValue = Pair.Value;

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				AttributeComponent->BroadcastAttributeReachedBoundaryEvent(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}
	
	// 属性当前值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		AttributeComponent->BroadcastAttributeValueChangeEvent(Payloads);
	}
}

void UTcsAttributeManagerSubsystem::MergeAttributeModifiers(
	const AActor* CombatEntity,
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TMap<FName, TArray<FTcsAttributeModifierInstance>> ModifiersToMerge;
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName).Add(Modifier);
	}
	
	// 执行修改器合并
	for (TPair<FName, TArray<FTcsAttributeModifierInstance>>& Pair : ModifiersToMerge)
	{
		if (Pair.Value.IsEmpty())
		{
			continue;
		}
		
		// No merger: do not merge, keep all instances.
		if (!Pair.Value[0].ModifierDef.MergerType)
		{
			MergedModifiers.Append(Pair.Value);
			continue;
		}

		auto Merger = Pair.Value[0].ModifierDef.MergerType->GetDefaultObject<UTcsAttributeModifierMerger>();
		Merger->Merge(Pair.Value, MergedModifiers);
	}
}

void UTcsAttributeManagerSubsystem::ClampAttributeValueInRange(
	UTcsAttributeComponent* AttributeComponent,
	const FName& AttributeName,
	float& NewValue,
	float* OutMinValue,
	float* OutMaxValue)
{
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
	if (!Attribute)
	{
		return;
	}
	const FTcsAttributeRange& Range = Attribute->AttributeDef.AttributeRange;

	// 计算属性范围的最小值
	float MinValue = TNumericLimits<float>::Lowest();
	switch (Range.MinValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		{
			break;
		}
	case ETcsAttributeRangeType::ART_Static:
		{
			MinValue = Range.MinValue;
			break;
		}
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			// DESIGN NOTE:
			// Dynamic range is resolved against the component's current committed attribute values (previous results),
			// not against any in-flight recalculation map in this function call.
			if (!AttributeComponent->GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
				return;
			}
			break;
		}
	}

	// 计算属性范围的最大值
	float MaxValue = TNumericLimits<float>::Max();
	switch (Range.MaxValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		{
			break;
		}
	case ETcsAttributeRangeType::ART_Static:
		{
			MaxValue = Range.MaxValue;
			break;
		}
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			// DESIGN NOTE:
			// Dynamic range is resolved against the component's current committed attribute values (previous results),
			// not against any in-flight recalculation map in this function call.
			if (!AttributeComponent->GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
				return;
			}
			break;
		}
	}

	// 属性值范围修正
	NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);

	if (OutMinValue)
	{
		*OutMinValue = MinValue;
	}
	if (OutMaxValue)
	{
		*OutMaxValue = MaxValue;
	}
}

FTcsSourceHandle UTcsAttributeManagerSubsystem::CreateSourceHandle(
	const FDataTableRowHandle& SourceDefinition,
	FName SourceName,
	const FGameplayTagContainer& SourceTags,
	AActor* Instigator)
{
	// 生成全局唯一ID
	++GlobalSourceHandleIdMgr;

	// 创建并返回SourceHandle
	return FTcsSourceHandle(GlobalSourceHandleIdMgr, SourceDefinition, SourceName, SourceTags, Instigator);
}

FTcsSourceHandle UTcsAttributeManagerSubsystem::CreateSourceHandleSimple(
	FName SourceName,
	AActor* Instigator)
{
	// 生成全局唯一ID
	++GlobalSourceHandleIdMgr;

	// 创建并返回简化的SourceHandle
	return FTcsSourceHandle(GlobalSourceHandleIdMgr, SourceName, Instigator);
}

bool UTcsAttributeManagerSubsystem::ApplyModifierWithSourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle,
	const TArray<FName>& ModifierIds,
	TArray<FTcsAttributeModifierInstance>& OutModifiers)
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return false;
	}

	OutModifiers.Empty();

	// 为每个ModifierId创建修改器实例
	for (const FName& ModifierId : ModifierIds)
	{
		FTcsAttributeModifierInstance ModifierInst;
		if (CreateAttributeModifier(ModifierId, SourceHandle.SourceName, SourceHandle.Instigator.Get(), CombatEntity, ModifierInst))
		{
			// 设置SourceHandle
			ModifierInst.SourceHandle = SourceHandle;
			// 保持SourceName同步
			ModifierInst.SourceName = SourceHandle.SourceName;

			OutModifiers.Add(ModifierInst);
		}
	}

	if (OutModifiers.Num() > 0)
	{
		ApplyModifier(CombatEntity, OutModifiers);
		return true;
	}

	return false;
}

bool UTcsAttributeManagerSubsystem::RemoveModifiersBySourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle)
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return false;
	}

	// 使用索引快速查找匹配的修改器
	TArray<int32>* IndicesPtr = AttributeComponent->SourceHandleIdToModifierIndices.Find(SourceHandle.Id);
	if (!IndicesPtr || IndicesPtr->Num() == 0)
	{
		return false;
	}

	// 收集要移除的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (int32 Index : *IndicesPtr)
	{
		if (AttributeComponent->AttributeModifiers.IsValidIndex(Index))
		{
			ModifiersToRemove.Add(AttributeComponent->AttributeModifiers[Index]);
		}
	}

	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(CombatEntity, ModifiersToRemove);
		return true;
	}

	return false;
}

bool UTcsAttributeManagerSubsystem::GetModifiersBySourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsAttributeModifierInstance>& OutModifiers) const
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return false;
	}

	OutModifiers.Empty();

	// 使用索引快速查找匹配的修改器
	const TArray<int32>* IndicesPtr = AttributeComponent->SourceHandleIdToModifierIndices.Find(SourceHandle.Id);
	if (!IndicesPtr || IndicesPtr->Num() == 0)
	{
		return false;
	}

	// 收集匹配的修改器
	for (int32 Index : *IndicesPtr)
	{
		if (AttributeComponent->AttributeModifiers.IsValidIndex(Index))
		{
			OutModifiers.Add(AttributeComponent->AttributeModifiers[Index]);
		}
	}

	return OutModifiers.Num() > 0;
}
