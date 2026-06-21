// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "Engine/Engine.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "Skill/TcsSkillInstance.h"
#include "Skill/SkillEntrySelector/TcsSkillEntrySelector.h"
#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "State/TcsStateInstance.h"
#include "TcsDefinitionRegistrySubsystem.h"
#include "TcsDeveloperSettings.h"
#include "TcsLogChannels.h"



namespace
{
	template <typename ModifierInstanceType>
	void SyncRuntimeEntriesActiveStatesFromModifierInstances(
		FTcsSkillModifierRuntimeIndex& RuntimeIndex,
		const FTcsSkillModifierRuntimeEntry& RuntimeEntry,
		const TArray<ModifierInstanceType>& ModifierInstances)
	{
		TArray<const FTcsSkillModifierRuntimeEntry*> ConflictEntries;
		if (!RuntimeIndex.FindConflictSet(RuntimeEntry.MakeConflictKey(), ConflictEntries))
		{
			return;
		}

		for (const FTcsSkillModifierRuntimeEntry* ConflictEntry : ConflictEntries)
		{
			if (!ConflictEntry)
			{
				continue;
			}

			FTcsSkillModifierRuntimeEntry* MutableEntry = RuntimeIndex.FindRuntimeEntryMutable(ConflictEntry->RuntimeModifierId);
			if (!MutableEntry)
			{
				continue;
			}

			const ModifierInstanceType* MatchingInstance = ModifierInstances.FindByPredicate(
				[&](const ModifierInstanceType& Instance)
				{
					return Instance.RuntimeModifierId == ConflictEntry->RuntimeModifierId;
				});

			MutableEntry->bActive = MatchingInstance ? MatchingInstance->bActive : false;
		}
	}


	bool CopyRuntimeEntries(
		const TArray<const FTcsSkillModifierRuntimeEntry*>& InEntries,
		TArray<FTcsSkillModifierRuntimeEntry>& OutEntries)
	{
		OutEntries.Reset();
		OutEntries.Reserve(InEntries.Num());

		for (const FTcsSkillModifierRuntimeEntry* Entry : InEntries)
		{
			if (!Entry)
			{
				continue;
			}

			OutEntries.Add(*Entry);
		}

		return !OutEntries.IsEmpty();
	}
}


void UTcsSkillComponent::BindOwnerStateLifecycleEvents(UTcsStateComponent* StateComponent)
{
	if (!IsValid(StateComponent))
	{
		return;
	}

	StateComponent->OnInternalStateFinalizeRemovalStarted().AddUObject(this, &UTcsSkillComponent::HandleOwnerStateFinalizeRemovalStarted);
	StateComponent->OnInternalStateFinalizeRemovalSourceCleanup().AddUObject(this, &UTcsSkillComponent::HandleOwnerStateFinalizeRemovalSourceCleanup);
}


void UTcsSkillComponent::UnbindOwnerStateLifecycleEvents(UTcsStateComponent* StateComponent)
{
	if (!IsValid(StateComponent))
	{
		return;
	}

	StateComponent->OnInternalStateFinalizeRemovalStarted().RemoveAll(this);
	StateComponent->OnInternalStateFinalizeRemovalSourceCleanup().RemoveAll(this);
}


void UTcsSkillComponent::HandleOwnerStateFinalizeRemovalStarted(UTcsStateComponent* StateComponent, UTcsStateInstance* StateInstance, FName /*RemovalReason*/)
{
	if (!IsValid(StateComponent) || StateComponent != GetOwnerStateComponent() || !IsValid(StateInstance))
	{
		return;
	}

	UTcsSkillInstance* SkillInstance = Cast<UTcsSkillInstance>(StateInstance);
	if (!SkillInstance)
	{
		return;
	}

	HandleSkillModifierSkillInstanceEnded(SkillInstance);
}


void UTcsSkillComponent::HandleOwnerStateFinalizeRemovalSourceCleanup(UTcsStateComponent* StateComponent, UTcsStateInstance* StateInstance, FName /*RemovalReason*/)
{
	if (!IsValid(StateComponent) || StateComponent != GetOwnerStateComponent() || !IsValid(StateInstance))
	{
		return;
	}

	if (Cast<UTcsSkillInstance>(StateInstance) || !StateInstance->GetSourceHandle().IsValid())
	{
		return;
	}

	HandleSkillModifierSourceEnded(StateInstance->GetSourceHandle());
}


bool UTcsSkillComponent::ApplySkillModifiersWithSourceHandle(
	const FTcsSourceHandle& SourceHandle,
	const TArray<FName>& ModifierIds,
	TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries)
{
	OutRuntimeEntries.Reset();

	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	TArray<FTcsSkillModifierRuntimeEntry> PendingRuntimeEntries;

	for (const FName& ModifierId : ModifierIds)
	{
		if (!CreateSkillModifierRuntimeEntries(ModifierId, SourceHandle, PendingRuntimeEntries))
		{
			return false;
		}
	}

	if (PendingRuntimeEntries.IsEmpty())
	{
		return false;
	}

	OutRuntimeEntries = MoveTemp(PendingRuntimeEntries);
	if (!ApplySkillModifierRuntimeEntries(OutRuntimeEntries))
	{
		OutRuntimeEntries.Reset();
		return false;
	}

	for (FTcsSkillModifierRuntimeEntry& RuntimeEntry : OutRuntimeEntries)
	{
		if (const FTcsSkillModifierRuntimeEntry* LedgerEntry = SkillModifierRuntimeIndex.FindRuntimeEntry(RuntimeEntry.RuntimeModifierId))
		{
			RuntimeEntry = *LedgerEntry;
		}
	}

	return true;
}


bool UTcsSkillComponent::RemoveSkillModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)
{
	TArray<const FTcsSkillModifierRuntimeEntry*> FoundEntries;
	if (!SkillModifierRuntimeIndex.FindBySourceHandle(SourceHandle, FoundEntries))
	{
		return false;
	}

	TArray<int32> RuntimeModifierIds;
	RuntimeModifierIds.Reserve(FoundEntries.Num());

	for (const FTcsSkillModifierRuntimeEntry* Entry : FoundEntries)
	{
		if (!Entry)
		{
			continue;
		}

		RuntimeModifierIds.Add(Entry->RuntimeModifierId);
	}

	if (RuntimeModifierIds.IsEmpty())
	{
		return false;
	}

	return RemoveSkillModifierRuntimeEntriesByIds(RuntimeModifierIds);
}


bool UTcsSkillComponent::GetSkillModifiersBySourceHandle(
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries) const
{
	TArray<const FTcsSkillModifierRuntimeEntry*> FoundEntries;
	if (!SkillModifierRuntimeIndex.FindBySourceHandle(SourceHandle, FoundEntries))
	{
		OutRuntimeEntries.Reset();
		return false;
	}

	return CopyRuntimeEntries(FoundEntries, OutRuntimeEntries);
}


bool UTcsSkillComponent::GetSkillModifiersBySkillEntry(
	UTcsSkillEntry* SkillEntry,
	TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries) const
{
	TArray<const FTcsSkillModifierRuntimeEntry*> FoundEntries;
	if (!SkillModifierRuntimeIndex.FindBySkillEntry(SkillEntry, FoundEntries))
	{
		OutRuntimeEntries.Reset();
		return false;
	}

	return CopyRuntimeEntries(FoundEntries, OutRuntimeEntries);
}


const UTcsSkillModifierDefinition* UTcsSkillComponent::ResolveSkillModifierDefinition(FName ModifierId) const
{
#if WITH_EDITOR
	if (UTcsDefinitionRegistrySubsystem* Registry = GEngine ? GEngine->GetEngineSubsystem<UTcsDefinitionRegistrySubsystem>() : nullptr)
	{
		if (const TSoftObjectPtr<UTcsSkillModifierDefinition>* Found = Registry->GetSkillModifierDefinitions().Find(ModifierId))
		{
			return Found->LoadSynchronous();
		}
	}
#endif

	if (const TSoftObjectPtr<UTcsSkillModifierDefinition>* Found = GetDefault<UTcsDeveloperSettings>()->GetCachedSkillModifierDefinitions().Find(ModifierId))
	{
		return Found->LoadSynchronous();
	}

	return nullptr;
}


bool UTcsSkillComponent::CreateSkillModifierRuntimeEntries(
	FName ModifierId,
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries)
{
	const UTcsSkillModifierDefinition* ModifierDef = ResolveSkillModifierDefinition(ModifierId);
	if (!ModifierDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] SkillModifierDefinition '%s' not found"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	if (!ModifierDef->EntrySelectorClass)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] SkillModifierDefinition '%s' has no EntrySelectorClass"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	UClass* EvaluatorClass = ModifierDef->ResolveActiveEvaluatorClass();
	if (!EvaluatorClass)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] SkillModifierDefinition '%s' failed to resolve active evaluator class"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	UTcsSkillEntrySelector* Selector = ModifierDef->EntrySelectorClass->GetDefaultObject<UTcsSkillEntrySelector>();
	if (!Selector)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] SkillModifierDefinition '%s' failed to resolve EntrySelector CDO"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	const TArray<UTcsSkillEntry*> TargetEntries = Selector->ResolveTargets(ModifierDef->EntrySelectorConfig, this);
	if (TargetEntries.IsEmpty())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] SkillModifierDefinition '%s' resolved no target SkillEntry"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	const int32 InitialCount = OutRuntimeEntries.Num();
	OutRuntimeEntries.Reserve(InitialCount + TargetEntries.Num());

	for (UTcsSkillEntry* TargetEntry : TargetEntries)
	{
		if (!IsValid(TargetEntry))
		{
			continue;
		}

		FTcsSkillModifierRuntimeEntry RuntimeEntry;
		RuntimeEntry.RuntimeModifierId = AllocateSkillModifierRuntimeId();
		RuntimeEntry.ModifierId = ModifierId;
		RuntimeEntry.Definition = const_cast<UTcsSkillModifierDefinition*>(ModifierDef);
		RuntimeEntry.TargetSkillEntry = TargetEntry;
		RuntimeEntry.TargetParamTag = ModifierDef->TargetParamTag;
		RuntimeEntry.TargetParamType = ModifierDef->TargetParamType;
		RuntimeEntry.SourceHandle = SourceHandle;
		RuntimeEntry.Priority = ModifierDef->Priority;
		RuntimeEntry.MergePolicy = ModifierDef->MergePolicy;
		RuntimeEntry.bActive = true;
		RuntimeEntry.ResolvedEvaluator = EvaluatorClass->GetDefaultObject<UObject>();
		RuntimeEntry.ResolvedConfig = ModifierDef->EvaluatorConfig;

		OutRuntimeEntries.Add(MoveTemp(RuntimeEntry));
	}

	return OutRuntimeEntries.Num() > InitialCount;
}


bool UTcsSkillComponent::ApplySkillModifierRuntimeEntries(TArray<FTcsSkillModifierRuntimeEntry>& RuntimeEntries)
{
	TArray<int32> AppliedRuntimeIds;
	AppliedRuntimeIds.Reserve(RuntimeEntries.Num());

	for (FTcsSkillModifierRuntimeEntry& Entry : RuntimeEntries)
	{
		if (!SkillModifierRuntimeIndex.AddRuntimeEntry(Entry))
		{
			if (!RemoveSkillModifierRuntimeEntriesByIds(AppliedRuntimeIds))
			{
				UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to rollback previously applied runtime entries after AddRuntimeEntry failure"), *FString(__FUNCTION__));
			}
			return false;
		}

		if (!WriteRuntimeEntryToSkillEntry(Entry))
		{
			FTcsSkillModifierRuntimeEntry IgnoredRemovedEntry;
			SkillModifierRuntimeIndex.RemoveRuntimeEntry(Entry.RuntimeModifierId, &IgnoredRemovedEntry);
			if (!RemoveSkillModifierRuntimeEntriesByIds(AppliedRuntimeIds))
			{
				UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to rollback previously applied runtime entries after SkillEntry write failure"), *FString(__FUNCTION__));
			}
			return false;
		}

		AppliedRuntimeIds.Add(Entry.RuntimeModifierId);
	}

	return true;
}


bool UTcsSkillComponent::WriteRuntimeEntryToSkillEntry(FTcsSkillModifierRuntimeEntry& RuntimeEntry)
{
	UTcsSkillEntry* TargetEntry = RuntimeEntry.TargetSkillEntry.Get();
	if (!IsValid(TargetEntry) || !RuntimeEntry.TargetParamTag.IsValid() || !RuntimeEntry.ResolvedEvaluator)
	{
		return false;
	}

	switch (RuntimeEntry.TargetParamType)
	{
	case ETcsStateParameterType::SPT_Numeric:
		{
			FTcsNumericStateParamInstance* ParamInstance = TargetEntry->FindNumericParamInstance(RuntimeEntry.TargetParamTag);
			UTcsStateParamNumericModifierExecution* Evaluator = Cast<UTcsStateParamNumericModifierExecution>(RuntimeEntry.ResolvedEvaluator);

			if (!ParamInstance || !Evaluator)
			{
				return false;
			}

			FStateParamNumericModifierInstance ModifierInstance;
			ModifierInstance.ModifierId = RuntimeEntry.ModifierId;
			ModifierInstance.RuntimeModifierId = RuntimeEntry.RuntimeModifierId;
			ModifierInstance.Evaluator = Evaluator;
			ModifierInstance.Config = RuntimeEntry.ResolvedConfig;
			ModifierInstance.Priority = RuntimeEntry.Priority;
			ModifierInstance.MergePolicy = RuntimeEntry.MergePolicy;
			ModifierInstance.SourceHandle = RuntimeEntry.SourceHandle;
			ModifierInstance.bActive = RuntimeEntry.bActive;

			ParamInstance->AssignModifier(ModifierInstance);
			SyncSkillModifierConflictSetActiveStates(RuntimeEntry);
			return true;
		}

	case ETcsStateParameterType::SPT_Bool:
		{
			FTcsBoolStateParamInstance* ParamInstance = TargetEntry->FindBoolParamInstance(RuntimeEntry.TargetParamTag);
			UTcsStateParamBoolModifierExecution* Evaluator = Cast<UTcsStateParamBoolModifierExecution>(RuntimeEntry.ResolvedEvaluator);

			if (!ParamInstance || !Evaluator)
			{
				return false;
			}

			FStateParamBoolModifierInstance ModifierInstance;
			ModifierInstance.ModifierId = RuntimeEntry.ModifierId;
			ModifierInstance.RuntimeModifierId = RuntimeEntry.RuntimeModifierId;
			ModifierInstance.Evaluator = Evaluator;
			ModifierInstance.Config = RuntimeEntry.ResolvedConfig;
			ModifierInstance.Priority = RuntimeEntry.Priority;
			ModifierInstance.MergePolicy = RuntimeEntry.MergePolicy;
			ModifierInstance.SourceHandle = RuntimeEntry.SourceHandle;
			ModifierInstance.bActive = RuntimeEntry.bActive;

			ParamInstance->AssignModifier(ModifierInstance);
			SyncSkillModifierConflictSetActiveStates(RuntimeEntry);
			return true;
		}

	case ETcsStateParameterType::SPT_Vector:
		{
			FTcsVectorStateParamInstance* ParamInstance = TargetEntry->FindVectorParamInstance(RuntimeEntry.TargetParamTag);
			UTcsStateParamVectorModifierExecution* Evaluator = Cast<UTcsStateParamVectorModifierExecution>(RuntimeEntry.ResolvedEvaluator);

			if (!ParamInstance || !Evaluator)
			{
				return false;
			}

			FStateParamVectorModifierInstance ModifierInstance;
			ModifierInstance.ModifierId = RuntimeEntry.ModifierId;
			ModifierInstance.RuntimeModifierId = RuntimeEntry.RuntimeModifierId;
			ModifierInstance.Evaluator = Evaluator;
			ModifierInstance.Config = RuntimeEntry.ResolvedConfig;
			ModifierInstance.Priority = RuntimeEntry.Priority;
			ModifierInstance.MergePolicy = RuntimeEntry.MergePolicy;
			ModifierInstance.SourceHandle = RuntimeEntry.SourceHandle;
			ModifierInstance.bActive = RuntimeEntry.bActive;

			ParamInstance->AssignModifier(ModifierInstance);
			SyncSkillModifierConflictSetActiveStates(RuntimeEntry);
			return true;
		}

	default:
		return false;
	}
}


bool UTcsSkillComponent::RemoveRuntimeEntryFromSkillEntry(const FTcsSkillModifierRuntimeEntry& RuntimeEntry)
{
	UTcsSkillEntry* TargetEntry = RuntimeEntry.TargetSkillEntry.Get();
	if (!IsValid(TargetEntry) || !RuntimeEntry.TargetParamTag.IsValid())
	{
		return false;
	}

	FName RemovedModifierId;
	bool bRemovedActiveInstance = false;

	switch (RuntimeEntry.TargetParamType)
	{
	case ETcsStateParameterType::SPT_Numeric:
		{
			FTcsNumericStateParamInstance* ParamInstance = TargetEntry->FindNumericParamInstance(RuntimeEntry.TargetParamTag);
			if (!ParamInstance || !ParamInstance->RemoveModifierByRuntimeId(RuntimeEntry.RuntimeModifierId, RemovedModifierId, bRemovedActiveInstance))
			{
				return false;
			}

			if (bRemovedActiveInstance)
			{
				ParamInstance->ReactivateHighestInactiveExclusive(RemovedModifierId);
			}

			return true;
		}

	case ETcsStateParameterType::SPT_Bool:
		{
			FTcsBoolStateParamInstance* ParamInstance = TargetEntry->FindBoolParamInstance(RuntimeEntry.TargetParamTag);
			if (!ParamInstance || !ParamInstance->RemoveModifierByRuntimeId(RuntimeEntry.RuntimeModifierId, RemovedModifierId, bRemovedActiveInstance))
			{
				return false;
			}

			if (bRemovedActiveInstance)
			{
				ParamInstance->ReactivateHighestInactiveExclusive(RemovedModifierId);
			}

			return true;
		}

	case ETcsStateParameterType::SPT_Vector:
		{
			FTcsVectorStateParamInstance* ParamInstance = TargetEntry->FindVectorParamInstance(RuntimeEntry.TargetParamTag);
			if (!ParamInstance || !ParamInstance->RemoveModifierByRuntimeId(RuntimeEntry.RuntimeModifierId, RemovedModifierId, bRemovedActiveInstance))
			{
				return false;
			}

			if (bRemovedActiveInstance)
			{
				ParamInstance->ReactivateHighestInactiveExclusive(RemovedModifierId);
			}

			return true;
		}

	default:
		return false;
	}
}


void UTcsSkillComponent::SyncSkillModifierConflictSetActiveStates(const FTcsSkillModifierRuntimeEntry& RuntimeEntry)
{
	UTcsSkillEntry* TargetEntry = RuntimeEntry.TargetSkillEntry.Get();
	if (!IsValid(TargetEntry) || !RuntimeEntry.TargetParamTag.IsValid())
	{
		return;
	}

	switch (RuntimeEntry.TargetParamType)
	{
	case ETcsStateParameterType::SPT_Numeric:
		if (FTcsNumericStateParamInstance* ParamInstance = TargetEntry->FindNumericParamInstance(RuntimeEntry.TargetParamTag))
		{
			SyncRuntimeEntriesActiveStatesFromModifierInstances(SkillModifierRuntimeIndex, RuntimeEntry, ParamInstance->ModifierInstances);
		}
		break;

	case ETcsStateParameterType::SPT_Bool:
		if (FTcsBoolStateParamInstance* ParamInstance = TargetEntry->FindBoolParamInstance(RuntimeEntry.TargetParamTag))
		{
			SyncRuntimeEntriesActiveStatesFromModifierInstances(SkillModifierRuntimeIndex, RuntimeEntry, ParamInstance->ModifierInstances);
		}
		break;

	case ETcsStateParameterType::SPT_Vector:
		if (FTcsVectorStateParamInstance* ParamInstance = TargetEntry->FindVectorParamInstance(RuntimeEntry.TargetParamTag))
		{
			SyncRuntimeEntriesActiveStatesFromModifierInstances(SkillModifierRuntimeIndex, RuntimeEntry, ParamInstance->ModifierInstances);
		}
		break;

	default:
		break;
	}
}


bool UTcsSkillComponent::RemoveSkillModifierRuntimeEntriesByIds(const TArray<int32>& RuntimeModifierIds)
{
	if (RuntimeModifierIds.IsEmpty())
	{
		return true;
	}

	bool bAllSucceeded = true;
	bool bRemovedAny = false;

	for (const int32 RuntimeModifierId : RuntimeModifierIds)
	{
		const FTcsSkillModifierRuntimeEntry* FoundEntry = SkillModifierRuntimeIndex.FindRuntimeEntry(RuntimeModifierId);
		if (!FoundEntry)
		{
			bAllSucceeded = false;
			continue;
		}

		const FTcsSkillModifierRuntimeEntry RuntimeEntryToRemove = *FoundEntry;
		if (!RemoveRuntimeEntryFromSkillEntry(RuntimeEntryToRemove))
		{
			UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to remove runtime entry %d from SkillEntry param chain"), *FString(__FUNCTION__), RuntimeModifierId);
			bAllSucceeded = false;
			continue;
		}

		FTcsSkillModifierRuntimeEntry RemovedEntry;
		if (!SkillModifierRuntimeIndex.RemoveRuntimeEntry(RuntimeModifierId, &RemovedEntry))
		{
			UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to remove runtime entry %d from runtime ledger, attempting to restore param chain"), *FString(__FUNCTION__), RuntimeModifierId);
			FTcsSkillModifierRuntimeEntry RuntimeEntryToRestore = RuntimeEntryToRemove;
			if (!WriteRuntimeEntryToSkillEntry(RuntimeEntryToRestore))
			{
				UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to restore runtime entry %d back to SkillEntry param chain after runtime ledger removal failure"), *FString(__FUNCTION__), RuntimeModifierId);
			}
			bAllSucceeded = false;
			continue;
		}

		SyncSkillModifierConflictSetActiveStates(RemovedEntry);
		bRemovedAny = true;
	}

	return bRemovedAny && bAllSucceeded;
}


void UTcsSkillComponent::HandleSkillModifierSourceEnded(const FTcsSourceHandle& SourceHandle)
{
	if (!SourceHandle.IsValid())
	{
		return;
	}

	TArray<FTcsSkillModifierRuntimeEntry> RuntimeEntries;
	if (!GetSkillModifiersBySourceHandle(SourceHandle, RuntimeEntries))
	{
		return;
	}

	if (!RemoveSkillModifiersBySourceHandle(SourceHandle))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to remove SkillModifiers for SourceHandle %d during lifecycle cleanup"), *FString(__FUNCTION__), SourceHandle.Id);
	}
}


void UTcsSkillComponent::HandleSkillModifierSkillInstanceEnded(UTcsSkillInstance* SkillInstance)
{
	if (!IsValid(SkillInstance))
	{
		return;
	}

	if (UTcsSkillEntry* SkillEntry = SkillInstance->GetSkillEntry())
	{
		if (SkillEntry->ActiveInstance.Get() == SkillInstance)
		{
			SkillEntry->ActiveInstance.Reset();
		}
	}

	HandleSkillModifierSourceEnded(SkillInstance->GetSourceHandle());
}


void UTcsSkillComponent::RemoveSkillModifiersForSkillEntry(UTcsSkillEntry* SkillEntry)
{
	TArray<const FTcsSkillModifierRuntimeEntry*> FoundEntries;
	if (!SkillModifierRuntimeIndex.FindBySkillEntry(SkillEntry, FoundEntries))
	{
		return;
	}

	TArray<int32> RuntimeModifierIds;
	RuntimeModifierIds.Reserve(FoundEntries.Num());

	for (const FTcsSkillModifierRuntimeEntry* Entry : FoundEntries)
	{
		if (!Entry)
		{
			continue;
		}

		RuntimeModifierIds.Add(Entry->RuntimeModifierId);
	}

	if (!RemoveSkillModifierRuntimeEntriesByIds(RuntimeModifierIds))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to remove one or more SkillModifiers for SkillEntry '%s' during lifecycle cleanup"), *FString(__FUNCTION__), *GetNameSafe(SkillEntry));
	}
}
