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

	// 构建 AttributeTag -> AttributeName 映射
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	TArray<FName> RowNames = AttributeDefTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FTcsAttributeDefinition* AttrDef = AttributeDefTable->FindRow<FTcsAttributeDefinition>(
			RowName,
			*FString(__FUNCTION__));
		if (!AttrDef)
		{
			continue;
		}

		// 检查 AttributeTag 是否有效
		if (!AttrDef->AttributeTag.IsValid())
		{
			// 空或无效 Tag，记录 Warning 但不影响运行
			if (AttrDef->AttributeTag != FGameplayTag::EmptyTag)
			{
				UE_LOG(LogTcsAttribute, Warning,
					TEXT("[%s] Attribute '%s' has invalid AttributeTag, skipping Tag mapping"),
					*FString(__FUNCTION__),
					*RowName.ToString());
			}
			continue;
		}

		// 检查是否重复
		if (AttributeTagToName.Contains(AttrDef->AttributeTag))
		{
			const FName ExistingName = AttributeTagToName[AttrDef->AttributeTag];
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Duplicate AttributeTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
				*FString(__FUNCTION__),
				*AttrDef->AttributeTag.ToString(),
				*ExistingName.ToString(),
				*RowName.ToString());
			continue;
		}

		// 添加到映射
		AttributeTagToName.Add(AttrDef->AttributeTag, RowName);
		AttributeNameToTag.Add(RowName, AttrDef->AttributeTag);
	}

	UE_LOG(LogTcsAttribute, Log,
		TEXT("[%s] Built AttributeTag mappings: %d valid tags registered"),
		*FString(__FUNCTION__),
		AttributeTagToName.Num());
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

	// 防止覆盖已存在的属性
	if (AttributeComponent->Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute '%s' already exists on CombatEntity '%s', skipping add"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
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

	// Clamp initialization values to the configured range (static or dynamic).
	if (FTcsAttributeInstance* Added = AttributeComponent->Attributes.Find(AttributeName))
	{
		float Clamped = Added->BaseValue;
		ClampAttributeValueInRange(AttributeComponent, AttributeName, Clamped);
		Added->BaseValue = Clamped;
		Added->CurrentValue = Clamped;
	}
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
		// 防止覆盖已存在的属性
		if (AttributeComponent->Attributes.Contains(AttributeName))
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute '%s' already exists on CombatEntity '%s', skipping add"),
				*FString(__FUNCTION__),
				*AttributeName.ToString(),
				*CombatEntity->GetName());
			continue;
		}

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

		// Clamp initialization values to the configured range (static or dynamic).
		if (FTcsAttributeInstance* Added = AttributeComponent->Attributes.Find(AttributeName))
		{
			float Clamped = Added->BaseValue;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, Clamped);
			Added->BaseValue = Clamped;
			Added->CurrentValue = Clamped;
		}
	}
}
bool UTcsAttributeManagerSubsystem::AddAttributeByTag(
	AActor* CombatEntity,
	const FGameplayTag& AttributeTag,
	float InitValue)
{
	FName AttributeName;
	if (!TryResolveAttributeNameByTag(AttributeTag, AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Failed to resolve AttributeTag '%s' to AttributeName"),
			*FString(__FUNCTION__),
			*AttributeTag.ToString());
		return false;
	}

	// 检查属性是否已存在
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (IsValid(AttributeComponent) && AttributeComponent->Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Attribute '%s' already exists on CombatEntity '%s', skipping add"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return false;
	}

	AddAttribute(CombatEntity, AttributeName, InitValue);

	// 验证是否真的添加成功
	return IsValid(AttributeComponent) && AttributeComponent->Attributes.Contains(AttributeName);
}
bool UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag(
	const FGameplayTag& AttributeTag,
	FName& OutAttributeName) const
{
	if (!AttributeTag.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Invalid AttributeTag provided"),
			*FString(__FUNCTION__));
		return false;
	}

	const FName* FoundName = AttributeTagToName.Find(AttributeTag);
	if (FoundName)
	{
		OutAttributeName = *FoundName;
		return true;
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("[%s] AttributeTag '%s' not found in mapping. Make sure the tag is registered in AttributeDefTable."),
		*FString(__FUNCTION__),
		*AttributeTag.ToString());
	return false;
}
bool UTcsAttributeManagerSubsystem::TryGetAttributeTagByName(
	FName AttributeName,
	FGameplayTag& OutAttributeTag) const
{
	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Invalid AttributeName provided"),
			*FString(__FUNCTION__));
		return false;
	}

	const FGameplayTag* FoundTag = AttributeNameToTag.Find(AttributeName);
	if (FoundTag)
	{
		OutAttributeTag = *FoundTag;
		return true;
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("[%s] AttributeName '%s' not found in mapping or has no AttributeTag configured."),
		*FString(__FUNCTION__),
		*AttributeName.ToString());
	return false;
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
	// 严格输入校验
	if (SourceName == NAME_None)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] SourceName cannot be NAME_None"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	// 验证 Instigator 和 Target 实现 ITcsEntityInterface
	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	if (!Target->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Target->GetName());
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
	// 严格输入校验
	if (SourceName == NAME_None)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] SourceName cannot be NAME_None"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	// 验证 Instigator 和 Target 实现 ITcsEntityInterface
	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	if (!Target->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Target->GetName());
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
		TArray<FTcsAttributeModifierInstance> UpdatedExistingModifiers;
		NewlyAddedModifiers.Reserve(ModifiersToApply.Num());

		// 把已经用过但有改变的属性修改器更新一下，并从待应用列表中移除
		{
			TArray<FTcsAttributeModifierInstance> IncomingToAdd;
			IncomingToAdd.Reserve(ModifiersToApply.Num());

			for (const FTcsAttributeModifierInstance& Incoming : ModifiersToApply)
			{
				bool bUpdated = false;

				if (Incoming.SourceHandle.IsValid())
				{
					const int32 SourceId = Incoming.SourceHandle.Id;

					// 使用稳定 ID 缓存查找现有修改器
					if (const TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(SourceId))
					{
						for (int32 ModifierInstId : *InstIdsPtr)
						{
							// 通过 ModifierInstId 查找当前数组下标
							const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(ModifierInstId);
							if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
							{
								// 索引失效，跳过（稍后自愈）
								continue;
							}

							int32 Index = *IndexPtr;
							FTcsAttributeModifierInstance& Stored = AttributeComponent->AttributeModifiers[Index];

							// 验证 ModifierInstId 匹配（防御性检查）
							if (Stored.ModifierInstId != ModifierInstId)
							{
								continue;
							}

							if (Stored.ModifierDef.ModifierName != Incoming.ModifierDef.ModifierName)
							{
								continue;
							}

							// Keep ModifierInstId and ApplyTimestamp stable; treat this as a refresh/update.
							Stored.Operands = Incoming.Operands;
							Stored.Instigator = Incoming.Instigator;
							Stored.Target = Incoming.Target;
							Stored.SourceHandle = Incoming.SourceHandle;
							Stored.SourceName = Incoming.SourceName;
							Stored.UpdateTimestamp = UtcNowTicks;
							Stored.LastTouchedBatchId = BatchId;

							UpdatedExistingModifiers.Add(Stored);
							bUpdated = true;
							break;
						}
					}
				}

				if (!bUpdated)
				{
					IncomingToAdd.Add(Incoming);
				}
			}

			ModifiersToApply = IncomingToAdd;
		}

		// 剩余的 ModifiersToApply 即为新增的修改器
		NewlyAddedModifiers = ModifiersToApply;

		// 添加新修改器并更新索引
		for (const FTcsAttributeModifierInstance& Modifier : ModifiersToApply)
		{
			FTcsAttributeModifierInstance ModifierToStore = Modifier;
			ModifierToStore.LastTouchedBatchId = BatchId;
			int32 NewIndex = AttributeComponent->AttributeModifiers.Add(ModifierToStore);

			// 更新两个缓存: ModifierInstId -> Index 和 SourceId -> ModifierInstIds
			AttributeComponent->ModifierInstIdToIndex.Add(ModifierToStore.ModifierInstId, NewIndex);

			if (ModifierToStore.SourceHandle.IsValid())
			{
				TArray<int32>& InstIds = AttributeComponent->SourceHandleIdToModifierInstIds.FindOrAdd(ModifierToStore.SourceHandle.Id);
				InstIds.AddUnique(ModifierToStore.ModifierInstId);
			}
		}

		// 广播新增事件
		for (const FTcsAttributeModifierInstance& Updated : UpdatedExistingModifiers)
		{
			AttributeComponent->BroadcastAttributeModifierUpdatedEvent(Updated);
		}

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
		// 使用 ModifierInstId 定位元素
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 修改器不存在或索引失效
			continue;
		}

		int32 RemovedIndex = *IndexPtr;
		const FTcsAttributeModifierInstance& RemovedModifierRef = AttributeComponent->AttributeModifiers[RemovedIndex];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (RemovedModifierRef.ModifierInstId != Modifier.ModifierInstId)
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] ModifierInstId mismatch at index %d: expected %d, found %d"),
				*FString(__FUNCTION__),
				RemovedIndex,
				Modifier.ModifierInstId,
				RemovedModifierRef.ModifierInstId);
			continue;
		}

		// 在 RemoveAtSwap 之前拷贝数据，避免引用失效
		const FTcsAttributeModifierInstance RemovedModifier = RemovedModifierRef;

		// 从两个缓存中移除
		AttributeComponent->ModifierInstIdToIndex.Remove(RemovedModifier.ModifierInstId);

		if (RemovedModifier.SourceHandle.IsValid())
		{
			TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(RemovedModifier.SourceHandle.Id);
			if (InstIdsPtr)
			{
				InstIdsPtr->Remove(RemovedModifier.ModifierInstId);
				if (InstIdsPtr->Num() == 0)
				{
					AttributeComponent->SourceHandleIdToModifierInstIds.Remove(RemovedModifier.SourceHandle.Id);
				}
			}
		}

		// 使用 RemoveAtSwap 删除元素（O(1) 操作）
		const int32 LastIndex = AttributeComponent->AttributeModifiers.Num() - 1;
		if (RemovedIndex != LastIndex)
		{
			// 有元素被 swap 过来，更新其索引
			const FTcsAttributeModifierInstance& SwappedModifier = AttributeComponent->AttributeModifiers[LastIndex];
			AttributeComponent->ModifierInstIdToIndex[SwappedModifier.ModifierInstId] = RemovedIndex;
		}

		AttributeComponent->AttributeModifiers.RemoveAtSwap(RemovedIndex);

		AttributeComponent->BroadcastAttributeModifierRemovedEvent(RemovedModifier);
		bModified = true;
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
		// 使用 ModifierInstId 定位元素
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 修改器不存在或索引失效
			continue;
		}

		const int32 ModifierIndex = *IndexPtr;
		const FTcsAttributeModifierInstance OldStored = AttributeComponent->AttributeModifiers[ModifierIndex];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (OldStored.ModifierInstId != Modifier.ModifierInstId)
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] ModifierInstId mismatch at index %d: expected %d, found %d"),
				*FString(__FUNCTION__),
				ModifierIndex,
				Modifier.ModifierInstId,
				OldStored.ModifierInstId);
			continue;
		}

		Modifier.UpdateTimestamp = UtcNowTicks;
		Modifier.LastTouchedBatchId = BatchId;

		AttributeComponent->AttributeModifiers[ModifierIndex] = Modifier;

		// 更新 SourceHandle 缓存（如果 SourceHandle 发生变化）
		const int32 OldSourceId = OldStored.SourceHandle.IsValid() ? OldStored.SourceHandle.Id : -1;
		const int32 NewSourceId = Modifier.SourceHandle.IsValid() ? Modifier.SourceHandle.Id : -1;
		if (OldSourceId != NewSourceId)
		{
			// 从旧桶中移除
			if (OldSourceId >= 0)
			{
				if (TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(OldSourceId))
				{
					InstIdsPtr->Remove(Modifier.ModifierInstId);
					if (InstIdsPtr->IsEmpty())
					{
						AttributeComponent->SourceHandleIdToModifierInstIds.Remove(OldSourceId);
					}
				}
			}

			// 添加到新桶
			if (NewSourceId >= 0)
			{
				TArray<int32>& InstIds = AttributeComponent->SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
				InstIds.AddUnique(Modifier.ModifierInstId);
			}
		}
		else if (NewSourceId >= 0)
		{
			// SourceHandle 未变化：确保 InstId 存在（防御性）
			TArray<int32>& InstIds = AttributeComponent->SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
			InstIds.AddUnique(Modifier.ModifierInstId);
		}

		AttributeComponent->BroadcastAttributeModifierUpdatedEvent(Modifier);

		bModified = true;
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

	// 执行范围约束传播
	EnforceAttributeRangeConstraints(AttributeComponent);
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

	// 执行范围约束传播
	EnforceAttributeRangeConstraints(AttributeComponent);
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
	float* OutMaxValue,
	const FAttributeValueResolver* Resolver)
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
			// 优先从 Resolver 读取（工作集），否则从已提交值读取
			bool bResolved = false;
			if (Resolver && (*Resolver)(Range.MinValueAttribute, MinValue))
			{
				bResolved = true;
			}
			else if (!AttributeComponent->GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
				// Keep default "no constraint" min value and continue clamping.
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
			// 优先从 Resolver 读取（工作集），否则从已提交值读取
			bool bResolved = false;
			if (Resolver && (*Resolver)(Range.MaxValueAttribute, MaxValue))
			{
				bResolved = true;
			}
			else if (!AttributeComponent->GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
				// Keep default "no constraint" max value and continue clamping.
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

void UTcsAttributeManagerSubsystem::EnforceAttributeRangeConstraints(UTcsAttributeComponent* AttributeComponent)
{
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const int32 MaxIterations = 8;  // 防止无限循环
	int32 Iteration = 0;
	bool bAnyChanged = true;

	// 工作集: 当前正在处理的值
	TMap<FName, float> WorkingBaseValues;
	TMap<FName, float> WorkingCurrentValues;

	// 初始化工作集
	for (auto& Pair : AttributeComponent->Attributes)
	{
		WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
		WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
	}

	// 值解析器: 优先从工作集读取
	FAttributeValueResolver Resolver = [&](FName AttributeName, float& OutValue) -> bool
	{
		if (float* Value = WorkingCurrentValues.Find(AttributeName))
		{
			OutValue = *Value;
			return true;
		}
		return false;
	};

	// 迭代直到稳定
	while (bAnyChanged && Iteration < MaxIterations)
	{
		bAnyChanged = false;
		Iteration++;

		for (auto& Pair : AttributeComponent->Attributes)
		{
			FName AttributeName = Pair.Key;

			// Clamp BaseValue
			float OldBase = WorkingBaseValues[AttributeName];
			float NewBase = OldBase;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, NewBase, nullptr, nullptr, &Resolver);
			if (!FMath::IsNearlyEqual(OldBase, NewBase))
			{
				WorkingBaseValues[AttributeName] = NewBase;
				bAnyChanged = true;
			}

			// Clamp CurrentValue
			float OldCurrent = WorkingCurrentValues[AttributeName];
			float NewCurrent = OldCurrent;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, NewCurrent, nullptr, nullptr, &Resolver);
			if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
			{
				WorkingCurrentValues[AttributeName] = NewCurrent;
				bAnyChanged = true;
			}
		}
	}

	// 检查是否收敛
	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Max iterations reached for entity '%s', possible circular dependency"),
			*FString(__FUNCTION__),
			*AttributeComponent->GetOwner()->GetName());
	}

	// 提交工作集到组件
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;

	for (auto& Pair : AttributeComponent->Attributes)
	{
		FName AttributeName = Pair.Key;
		FTcsAttributeInstance& Attribute = Pair.Value;

		// 提交 BaseValue
		float NewBase = WorkingBaseValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.BaseValue, NewBase))
		{
			float OldBase = Attribute.BaseValue;
			Attribute.BaseValue = NewBase;

			// 构造事件 payload
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldBase;
			Payload.NewValue = NewBase;
			// SourceHandle: 空(因为是 enforcement 导致的变化,不是 modifier)
			BaseChangePayloads.Add(Payload);
		}

		// 提交 CurrentValue
		float NewCurrent = WorkingCurrentValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.CurrentValue, NewCurrent))
		{
			float OldCurrent = Attribute.CurrentValue;
			Attribute.CurrentValue = NewCurrent;

			// 构造事件 payload
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldCurrent;
			Payload.NewValue = NewCurrent;
			CurrentChangePayloads.Add(Payload);
		}
	}

	// 广播事件
	if (BaseChangePayloads.Num() > 0)
	{
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}
	if (CurrentChangePayloads.Num() > 0)
	{
		AttributeComponent->BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
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

	// 使用稳定 ID 缓存查找匹配的修改器
	const TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	// 先拷贝 ID 列表（避免在迭代中修改）
	TArray<int32> InstIdsCopy = *InstIdsPtr;

	// 收集要移除的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (int32 ModifierInstId : InstIdsCopy)
	{
		// 通过 ModifierInstId 查找当前数组下标
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 索引失效，跳过
			continue;
		}

		int32 Index = *IndexPtr;
		const FTcsAttributeModifierInstance& Modifier = AttributeComponent->AttributeModifiers[Index];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (Modifier.ModifierInstId == ModifierInstId)
		{
			ModifiersToRemove.Add(Modifier);
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

	// 使用稳定 ID 缓存查找匹配的修改器
	TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	// 收集匹配的修改器，同时自愈陈旧的 ID
	TArray<int32> StaleIds;
	for (int32 ModifierInstId : *InstIdsPtr)
	{
		// 通过 ModifierInstId 查找当前数组下标
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 索引失效，标记为陈旧
			StaleIds.Add(ModifierInstId);
			continue;
		}

		int32 Index = *IndexPtr;
		const FTcsAttributeModifierInstance& Modifier = AttributeComponent->AttributeModifiers[Index];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (Modifier.ModifierInstId != ModifierInstId)
		{
			// 不匹配，标记为陈旧
			StaleIds.Add(ModifierInstId);
			continue;
		}

		OutModifiers.Add(Modifier);
	}

	// 自愈：从桶中移除陈旧的 ID
	if (StaleIds.Num() > 0)
	{
		for (int32 StaleId : StaleIds)
		{
			InstIdsPtr->Remove(StaleId);
		}

		// 如果桶为空，移除整个桶
		if (InstIdsPtr->Num() == 0)
		{
			AttributeComponent->SourceHandleIdToModifierInstIds.Remove(SourceHandle.Id);
		}

		UE_LOG(LogTcsAttribute, Verbose,
			TEXT("[%s] Self-healed %d stale ModifierInstIds for SourceHandle.Id=%d"),
			*FString(__FUNCTION__),
			StaleIds.Num(),
			SourceHandle.Id);
	}

	return OutModifiers.Num() > 0;
}
