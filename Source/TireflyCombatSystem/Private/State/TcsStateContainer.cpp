// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateContainer.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"


static void RemoveInvalidAndExpired(TArray<TObjectPtr<UTcsStateInstance>>& Instances)
{
	Instances.RemoveAll([](const UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}
		return State->GetCurrentStage() == ETcsStateStage::SS_Expired;
	});
}


void FTcsStateInstanceIndex::AddInstance(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance) || StateInstance->GetStateDefId().IsNone())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateInstance or StateDefId."),
			*FString(__FUNCTION__));
		return;
	}

	if (Instances.Contains(StateInstance))
	{
		return;
	}

	Instances.Add(StateInstance);
	InstancesById.FindOrAdd(StateInstance->GetInstanceId()) = StateInstance;

	FTcsStateInstanceArray& ByName = InstancesByName.FindOrAdd(StateInstance->GetStateDefId());
	ByName.StateInstances.Add(StateInstance);

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (StateDef)
	{
		const FGameplayTag SlotTag = StateDef->StateSlotType;
		if (SlotTag.IsValid())
		{
			FTcsStateInstanceArray& BySlot = InstancesBySlot.FindOrAdd(SlotTag);
			BySlot.StateInstances.Add(StateInstance);
		}
	}
}

void FTcsStateInstanceIndex::RemoveInstance(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance) || StateInstance->GetStateDefId().IsNone())
	{
		return;
	}

	Instances.Remove(StateInstance);
	InstancesById.Remove(StateInstance->GetInstanceId());

	if (FTcsStateInstanceArray* InstanceArray = InstancesByName.Find(StateInstance->GetStateDefId()))
	{
		InstanceArray->StateInstances.Remove(StateInstance);
		if (InstanceArray->StateInstances.IsEmpty())
		{
			InstancesByName.Remove(StateInstance->GetStateDefId());
		}
	}

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (StateDef)
	{
		const FGameplayTag SlotTag = StateDef->StateSlotType;
		if (SlotTag.IsValid())
		{
			if (FTcsStateInstanceArray* SlotArray = InstancesBySlot.Find(SlotTag))
			{
				SlotArray->StateInstances.Remove(StateInstance);
				if (SlotArray->StateInstances.IsEmpty())
				{
					InstancesBySlot.Remove(SlotTag);
				}
			}
		}
	}
}

void FTcsStateInstanceIndex::RefreshInstances()
{
	RemoveInvalidAndExpired(Instances);

	TArray<int32> InvalidIds;
	for (const TPair<int32, TObjectPtr<UTcsStateInstance>>& Pair : InstancesById)
	{
		if (!IsValid(Pair.Value) || Pair.Value->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			InvalidIds.Add(Pair.Key);
		}
	}
	for (int32 Id : InvalidIds)
	{
		InstancesById.Remove(Id);
	}

	TArray<FName> InvalidNames;
	for (const TPair<FName, FTcsStateInstanceArray>& Pair : InstancesByName)
	{
		FTcsStateInstanceArray& InstanceArray = InstancesByName[Pair.Key];
		RemoveInvalidAndExpired(InstanceArray.StateInstances);
		if (InstanceArray.StateInstances.IsEmpty())
		{
			InvalidNames.Add(Pair.Key);
		}
	}
	for (const FName& Name : InvalidNames)
	{
		InstancesByName.Remove(Name);
	}

	TArray<FGameplayTag> InvalidSlots;
	for (const TPair<FGameplayTag, FTcsStateInstanceArray>& Pair : InstancesBySlot)
	{
		FTcsStateInstanceArray& SlotArray = InstancesBySlot[Pair.Key];
		RemoveInvalidAndExpired(SlotArray.StateInstances);
		if (SlotArray.StateInstances.IsEmpty())
		{
			InvalidSlots.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Slot : InvalidSlots)
	{
		InstancesBySlot.Remove(Slot);
	}
}

UTcsStateInstance* FTcsStateInstanceIndex::GetInstanceById(int32 InstanceId) const
{
	return InstancesById.Contains(InstanceId) ? InstancesById[InstanceId] : nullptr;
}

bool FTcsStateInstanceIndex::GetInstancesByName(FName StateDefId, TArray<UTcsStateInstance*>& OutInstances) const
{
	if (const FTcsStateInstanceArray* Found = InstancesByName.Find(StateDefId))
	{
		OutInstances.Empty(Found->StateInstances.Num());
		for (UTcsStateInstance* State : Found->StateInstances)
		{
			if (IsValid(State) && State->GetCurrentStage() != ETcsStateStage::SS_Expired)
			{
				OutInstances.Add(State);
			}
		}
		return OutInstances.Num() > 0;
	}

	OutInstances.Empty();
	return false;
}

bool FTcsStateInstanceIndex::GetInstancesBySlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutInstances) const
{
	if (const FTcsStateInstanceArray* Found = InstancesBySlot.Find(SlotTag))
	{
		OutInstances.Empty(Found->StateInstances.Num());
		for (UTcsStateInstance* State : Found->StateInstances)
		{
			if (IsValid(State) && State->GetCurrentStage() != ETcsStateStage::SS_Expired)
			{
				OutInstances.Add(State);
			}
		}
		return OutInstances.Num() > 0;
	}

	OutInstances.Empty();
	return false;
}


void FTcsStateDurationTracker::Add(UTcsStateInstance* StateInstance, float InitialRemaining)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	RemainingByInstance.FindOrAdd(StateInstance) = InitialRemaining;
}

void FTcsStateDurationTracker::Remove(UTcsStateInstance* StateInstance)
{
	if (!StateInstance)
	{
		return;
	}

	RemainingByInstance.Remove(StateInstance);
}

bool FTcsStateDurationTracker::GetRemaining(const UTcsStateInstance* StateInstance, float& OutRemaining) const
{
	if (!IsValid(StateInstance))
	{
		return false;
	}

	if (const float* Remaining = RemainingByInstance.Find(const_cast<UTcsStateInstance*>(StateInstance)))
	{
		OutRemaining = *Remaining;
		return true;
	}

	return false;
}

bool FTcsStateDurationTracker::SetRemaining(UTcsStateInstance* StateInstance, float NewRemaining)
{
	if (!IsValid(StateInstance))
	{
		return false;
	}

	if (float* Remaining = RemainingByInstance.Find(StateInstance))
	{
		*Remaining = NewRemaining;
		return true;
	}

	return false;
}

void FTcsStateDurationTracker::RefreshInstances()
{
	TArray<TObjectPtr<UTcsStateInstance>> InvalidStates;
	for (const TPair<TObjectPtr<UTcsStateInstance>, float>& Pair : RemainingByInstance)
	{
		if (!IsValid(Pair.Key) || Pair.Key->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			InvalidStates.Add(Pair.Key);
		}
	}
	for (const TObjectPtr<UTcsStateInstance>& State : InvalidStates)
	{
		RemainingByInstance.Remove(State);
	}
}


void FTcsStateTreeTickScheduler::Add(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (!RunningInstances.Contains(StateInstance))
	{
		RunningInstances.Add(StateInstance);
	}
}

void FTcsStateTreeTickScheduler::Remove(UTcsStateInstance* StateInstance)
{
	if (!StateInstance)
	{
		return;
	}

	RunningInstances.Remove(StateInstance);
}

void FTcsStateTreeTickScheduler::RefreshInstances()
{
	RunningInstances.RemoveAll([](const UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}
		if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			return true;
		}
		return false;
	});
}
