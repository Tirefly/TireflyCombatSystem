// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsEntityInterface.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"



namespace
{
	bool LogAttributeRuntimeNotReady_Modifiers(const UTcsAttributeComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return false;
	}

	void LogAttributeRuntimeNotReadyVoid_Modifiers(const UTcsAttributeComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
	}

	void BuildModifierEventPayloads(
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierEventPayload>& OutPayloads)
	{
		OutPayloads.Reset();
		OutPayloads.Reserve(Modifiers.Num());

		for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
		{
			OutPayloads.Emplace(Modifier);
		}
	}
}

bool UTcsAttributeComponent::CreateAttributeModifier(
	FName ModifierId,
	AActor* Instigator,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	if (!IsRuntimePrepared())
	{
		OutModifierInst = FTcsAttributeModifierInstance();
		return LogAttributeRuntimeNotReady_Modifiers(this, TEXT(__FUNCTION__));
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"), *FString(__FUNCTION__));
		return false;
	}

	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager();
	if (!DefinitionManager)
	{
		return false;
	}

	const UTcsAttributeModifierDefinition* ModifierDef = DefinitionManager->GetAttributeModifierDefinition(ModifierId);
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	OutModifierInst = FTcsAttributeModifierInstance();

	// 设置 DataAsset 引用和 ModifierId
	OutModifierInst.ModifierDef = ModifierDef;
	OutModifierInst.ModifierId = ModifierId;

	// 验证优先级
	if (ModifierDef->Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, will use raw priority 0."),
			*FString(__FUNCTION__),
			*ModifierDef->ModifierName.ToString(),
			ModifierDef->Priority);
		return false;
	}

	OutModifierInst.ModifierInstId = Mgr->AllocateModifierInstanceId();
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = GetOwner();
	OutModifierInst.Operands = ModifierDef->Operands;

	return true;
}

bool UTcsAttributeComponent::CreateAttributeModifierWithBindings(
	FName ModifierId,
	AActor* Instigator,
	const TArray<FTcsStateParamBinding>& Bindings,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	if (!IsRuntimePrepared())
	{
		OutModifierInst = FTcsAttributeModifierInstance();
		return LogAttributeRuntimeNotReady_Modifiers(this, TEXT(__FUNCTION__));
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"), *FString(__FUNCTION__));
		return false;
	}

	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager();
	if (!DefinitionManager)
	{
		return false;
	}

	const UTcsAttributeModifierDefinition* ModifierDef = DefinitionManager->GetAttributeModifierDefinition(ModifierId);
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	OutModifierInst = FTcsAttributeModifierInstance();
	OutModifierInst.ModifierDef = ModifierDef;
	OutModifierInst.ModifierId = ModifierId;
	OutModifierInst.ModifierInstId = Mgr->AllocateModifierInstanceId();
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = GetOwner();

	// 从 DefAsset 复制默认 Operands
	OutModifierInst.Operands = ModifierDef->Operands;

	// 设置 StateParam 绑定（首值在首次 RecalculateAttributeCurrentValues 时拉取）
	OutModifierInst.OperandBindings = Bindings;

	return true;
}

void UTcsAttributeComponent::ApplyModifier(TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_ApplyModifier);
	if (!IsRuntimePrepared())
	{
		LogAttributeRuntimeNotReadyVoid_Modifiers(this, TEXT(__FUNCTION__));
		return;
	}

	if (Modifiers.IsEmpty())
	{
		return;
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

	TArray<FTcsAttributeModifierInstance> ModifiersToExecute;
	TArray<FTcsAttributeModifierInstance> ModifiersToApply;
	ModifiersToExecute.Reserve(Modifiers.Num());
	ModifiersToApply.Reserve(Modifiers.Num());
	const int64 BatchId = Mgr->AllocateModifierChangeBatchId();
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();

	// 区分修改属性 Base 值和 Current 值的两种修改器
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		if (!Modifier.ModifierDef)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] ModifierDef is null for ModifierId: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString());
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;

		switch (ModDef->ModifierMode)
		{
		case ETcsAttributeModifierMode::AMM_BaseValue:
			{
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToExecute.Add(Modifier);
				break;
			}
		case ETcsAttributeModifierMode::AMM_CurrentValue:
			{
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToApply.Add(Modifier);
				break;
			}
		}
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Input=%d BaseOps=%d CurrentOps=%d StoredCurrentModifiers=%d"),
		*FString(__FUNCTION__),
		Modifiers.Num(),
		ModifiersToExecute.Num(),
		ModifiersToApply.Num(),
		AttributeModifiers.Num());

	if (!ModifiersToExecute.IsEmpty())
	{
		RecalculateAttributeBaseValues(ModifiersToExecute);
	}

	if (!ModifiersToApply.IsEmpty())
	{
		TArray<FTcsAttributeModifierInstance> NewlyAddedModifiers;
		TArray<FTcsAttributeModifierInstance> UpdatedExistingModifiers;
		TArray<FTcsAttributeModifierEventPayload> ModifierEventPayloads;
		NewlyAddedModifiers.Reserve(ModifiersToApply.Num());
		UpdatedExistingModifiers.Reserve(ModifiersToApply.Num());

		{
			TArray<FTcsAttributeModifierInstance> IncomingToAdd;
			IncomingToAdd.Reserve(ModifiersToApply.Num());

			for (const FTcsAttributeModifierInstance& Incoming : ModifiersToApply)
			{
				bool bUpdated = false;

				if (Incoming.SourceHandle.IsValid())
				{
					const int32 SourceId = Incoming.SourceHandle.Id;

					if (const TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceId))
					{
						for (int32 ModifierInstId : *InstIdsPtr)
						{
							const int32* IndexPtr = ModifierInstIdToIndex.Find(ModifierInstId);
							if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
							{
								continue;
							}

							int32 Index = *IndexPtr;
							FTcsAttributeModifierInstance& Stored = AttributeModifiers[Index];

							if (Stored.ModifierInstId != ModifierInstId)
							{
								continue;
							}

							if (Stored.ModifierId != Incoming.ModifierId)
							{
								continue;
							}

							Stored.Operands = Incoming.Operands;
							Stored.Instigator = Incoming.Instigator;
							Stored.Target = Incoming.Target;
							Stored.SourceHandle = Incoming.SourceHandle;
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

		NewlyAddedModifiers = ModifiersToApply;

		for (const FTcsAttributeModifierInstance& Modifier : ModifiersToApply)
		{
			FTcsAttributeModifierInstance ModifierToStore = Modifier;
			ModifierToStore.LastTouchedBatchId = BatchId;
			int32 NewIndex = AttributeModifiers.Add(ModifierToStore);

			ModifierInstIdToIndex.Add(ModifierToStore.ModifierInstId, NewIndex);

			if (ModifierToStore.SourceHandle.IsValid())
			{
				TSet<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(ModifierToStore.SourceHandle.Id);
				InstIds.Add(ModifierToStore.ModifierInstId);
			}
		}

		BuildModifierEventPayloads(UpdatedExistingModifiers, ModifierEventPayloads);
		BroadcastAttributeModifierUpdatedBatchEvent(ModifierEventPayloads);

		BuildModifierEventPayloads(NewlyAddedModifiers, ModifierEventPayloads);
		BroadcastAttributeModifierAddedBatchEvent(ModifierEventPayloads);
	}

	bMergedModifiersDirty = true;
	RecalculateAttributeCurrentValues(BatchId);
}

bool UTcsAttributeComponent::ApplyModifierWithSourceHandle(
	const FTcsSourceHandle& SourceHandle,
	const TArray<FName>& ModifierIds,
	TArray<FTcsAttributeModifierInstance>& OutModifiers)
{
	OutModifiers.Empty();
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Modifiers(this, TEXT(__FUNCTION__));
	}

	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	for (const FName& ModifierId : ModifierIds)
	{
		FTcsAttributeModifierInstance ModifierInst;
		if (CreateAttributeModifier(ModifierId, SourceHandle.Instigator.Get(), ModifierInst))
		{
			ModifierInst.SourceHandle = SourceHandle;
			OutModifiers.Add(ModifierInst);
		}
	}

	if (OutModifiers.Num() > 0)
	{
		ApplyModifier(OutModifiers);
		return true;
	}

	return false;
}

void UTcsAttributeComponent::RemoveModifier(TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	if (!IsRuntimePrepared())
	{
		LogAttributeRuntimeNotReadyVoid_Modifiers(this, TEXT(__FUNCTION__));
		return;
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

	const int64 BatchId = Mgr->AllocateModifierChangeBatchId();
	bool bModified = false;
	TArray<FTcsAttributeModifierInstance> RemovedModifiers;
	RemovedModifiers.Reserve(Modifiers.Num());
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		const int32* IndexPtr = ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			continue;
		}

		int32 RemovedIndex = *IndexPtr;
		const FTcsAttributeModifierInstance& RemovedModifierRef = AttributeModifiers[RemovedIndex];

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

		const FTcsAttributeModifierInstance RemovedModifier = RemovedModifierRef;

		ModifierInstIdToIndex.Remove(RemovedModifier.ModifierInstId);

		if (RemovedModifier.SourceHandle.IsValid())
		{
			TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(RemovedModifier.SourceHandle.Id);
			if (InstIdsPtr)
			{
				InstIdsPtr->Remove(RemovedModifier.ModifierInstId);
				if (InstIdsPtr->Num() == 0)
				{
					SourceHandleIdToModifierInstIds.Remove(RemovedModifier.SourceHandle.Id);
				}
			}
		}

		const int32 LastIndex = AttributeModifiers.Num() - 1;
		if (RemovedIndex != LastIndex)
		{
			const FTcsAttributeModifierInstance& SwappedModifier = AttributeModifiers[LastIndex];
			ModifierInstIdToIndex[SwappedModifier.ModifierInstId] = RemovedIndex;
		}

		AttributeModifiers.RemoveAtSwap(RemovedIndex);

		RemovedModifiers.Add(RemovedModifier);
		bModified = true;
	}

	if (!RemovedModifiers.IsEmpty())
	{
		TArray<FTcsAttributeModifierEventPayload> RemovedPayloads;
		BuildModifierEventPayloads(RemovedModifiers, RemovedPayloads);
		BroadcastAttributeModifierRemovedBatchEvent(RemovedPayloads);
	}

	if (bModified)
	{
		bMergedModifiersDirty = true;
		RecalculateAttributeCurrentValues(BatchId);
	}
}

bool UTcsAttributeComponent::RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RemoveModifiersBySourceHandle);
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Modifiers(this, TEXT(__FUNCTION__));
	}

	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	const TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] SourceId=%d BucketSize=%d StoredCurrentModifiers=%d"),
		*FString(__FUNCTION__),
		SourceHandle.Id,
		InstIdsPtr->Num(),
		AttributeModifiers.Num());

	return RemoveStoredModifiersByInstIds(*InstIdsPtr, Mgr->AllocateModifierChangeBatchId());
}

bool UTcsAttributeComponent::GetModifiersBySourceHandle(
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsAttributeModifierInstance>& OutModifiers) const
{
	OutModifiers.Empty();
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Modifiers(this, TEXT(__FUNCTION__));
	}

	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	const TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	OutModifiers.Reserve(InstIdsPtr->Num());
	for (int32 ModifierInstId : *InstIdsPtr)
	{
		const int32* IndexPtr = ModifierInstIdToIndex.Find(ModifierInstId);
		if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			continue;
		}

		int32 Index = *IndexPtr;
		const FTcsAttributeModifierInstance& Modifier = AttributeModifiers[Index];

		if (Modifier.ModifierInstId == ModifierInstId)
		{
			OutModifiers.Add(Modifier);
		}
	}

	return OutModifiers.Num() > 0;
}

void UTcsAttributeComponent::HandleModifierUpdated(TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	if (!IsRuntimePrepared())
	{
		LogAttributeRuntimeNotReadyVoid_Modifiers(this, TEXT(__FUNCTION__));
		return;
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

	bool bModified = false;
	const int64 BatchId = Mgr->AllocateModifierChangeBatchId();
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();
	TArray<FTcsAttributeModifierInstance> UpdatedModifiers;
	UpdatedModifiers.Reserve(Modifiers.Num());
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		const int32* IndexPtr = ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			continue;
		}

		const int32 ModifierIndex = *IndexPtr;
		const FTcsAttributeModifierInstance OldStored = AttributeModifiers[ModifierIndex];

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

		AttributeModifiers[ModifierIndex] = Modifier;

		const int32 OldSourceId = OldStored.SourceHandle.IsValid() ? OldStored.SourceHandle.Id : -1;
		const int32 NewSourceId = Modifier.SourceHandle.IsValid() ? Modifier.SourceHandle.Id : -1;
		if (OldSourceId != NewSourceId)
		{
			if (OldSourceId >= 0)
			{
				if (TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(OldSourceId))
				{
					InstIdsPtr->Remove(Modifier.ModifierInstId);
					if (InstIdsPtr->IsEmpty())
					{
						SourceHandleIdToModifierInstIds.Remove(OldSourceId);
					}
				}
			}

			if (NewSourceId >= 0)
			{
				TSet<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
				InstIds.Add(Modifier.ModifierInstId);
			}
		}
		else if (NewSourceId >= 0)
		{
			TSet<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
			InstIds.Add(Modifier.ModifierInstId);
		}

		UpdatedModifiers.Add(Modifier);

		bModified = true;
	}

	if (!UpdatedModifiers.IsEmpty())
	{
		TArray<FTcsAttributeModifierEventPayload> UpdatedPayloads;
		BuildModifierEventPayloads(UpdatedModifiers, UpdatedPayloads);
		BroadcastAttributeModifierUpdatedBatchEvent(UpdatedPayloads);
	}

	if (bModified)
	{
		bMergedModifiersDirty = true;
		RecalculateAttributeCurrentValues(BatchId);
	}
}

bool UTcsAttributeComponent::RemoveStoredModifiersByInstIds(
	const TSet<int32>& ModifierInstIdsToRemove,
	int64 ChangeBatchId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RemoveStoredModifiersByInstIds);

	if (ModifierInstIdsToRemove.IsEmpty() || AttributeModifiers.IsEmpty())
	{
		return false;
	}

	TArray<FTcsAttributeModifierInstance> RemovedModifiers;
	RemovedModifiers.Reserve(FMath::Min(ModifierInstIdsToRemove.Num(), AttributeModifiers.Num()));

	TArray<FTcsAttributeModifierInstance> RemainingModifiers;
	RemainingModifiers.Reserve(AttributeModifiers.Num());

	for (const FTcsAttributeModifierInstance& StoredModifier : AttributeModifiers)
	{
		if (ModifierInstIdsToRemove.Contains(StoredModifier.ModifierInstId))
		{
			RemovedModifiers.Add(StoredModifier);
			continue;
		}

		RemainingModifiers.Add(StoredModifier);
	}

	if (RemovedModifiers.IsEmpty())
	{
		return false;
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] RemoveIds=%d Removed=%d Remaining=%d BatchId=%lld"),
		*FString(__FUNCTION__),
		ModifierInstIdsToRemove.Num(),
		RemovedModifiers.Num(),
		RemainingModifiers.Num(),
		ChangeBatchId);

	AttributeModifiers = MoveTemp(RemainingModifiers);
	RebuildModifierRuntimeCaches();

	TArray<FTcsAttributeModifierEventPayload> RemovedPayloads;
	BuildModifierEventPayloads(RemovedModifiers, RemovedPayloads);
	BroadcastAttributeModifierRemovedBatchEvent(RemovedPayloads);

	bMergedModifiersDirty = true;
	RecalculateAttributeCurrentValues(ChangeBatchId);
	return true;
}

void UTcsAttributeComponent::RebuildModifierRuntimeCaches()
{
	ModifierInstIdToIndex.Reset();
	SourceHandleIdToModifierInstIds.Reset();

	ModifierInstIdToIndex.Reserve(AttributeModifiers.Num());
	SourceHandleIdToModifierInstIds.Reserve(AttributeModifiers.Num());

	for (int32 Index = 0; Index < AttributeModifiers.Num(); ++Index)
	{
		const FTcsAttributeModifierInstance& Modifier = AttributeModifiers[Index];
		ModifierInstIdToIndex.Add(Modifier.ModifierInstId, Index);

		if (Modifier.SourceHandle.IsValid())
		{
			SourceHandleIdToModifierInstIds.FindOrAdd(Modifier.SourceHandle.Id).Add(Modifier.ModifierInstId);
		}
	}
}
