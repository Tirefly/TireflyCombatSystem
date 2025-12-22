// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateContainer.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"


void FTcsPersistentStateInstanceContainer::AddInstance(UTcsStateInstance* StateInstance)
{
	if (!StateInstance || StateInstance->GetStateDefId().IsNone())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateInstance or StateDefId."),
			*FString(__FUNCTION__));
		return;
	}

	if (Instances.Contains(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is already in Instances."),
			*FString(__FUNCTION__));
		return;
	}

	Instances.Add(StateInstance);
	InstancesById.FindOrAdd(StateInstance->GetInstanceId()) = StateInstance;
	
	FTcsStateInstanceArray& InstanceArray = InstancesByName.FindOrAdd(StateInstance->GetStateDefId());
	InstanceArray.StateInstances.Add(StateInstance);
}

void FTcsPersistentStateInstanceContainer::RemoveInstance(UTcsStateInstance* StateInstance)
{
	if (!StateInstance || StateInstance->GetStateDefId().IsNone())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateInstance or StateDefId."),
			*FString(__FUNCTION__));
		return;
	}

	if (Instances.Remove(StateInstance) == 0)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance not found in Instances."),
			*FString(__FUNCTION__));
		return;
	}

	InstancesById.Remove(StateInstance->GetInstanceId());
	if (FTcsStateInstanceArray* InstanceArray = InstancesByName.Find(StateInstance->GetStateDefId()))
	{
		InstanceArray->StateInstances.Remove(StateInstance);
		if (InstanceArray->StateInstances.Num() == 0)
		{
			InstancesByName.Remove(StateInstance->GetStateDefId());
		}
	}
}

void FTcsPersistentStateInstanceContainer::RefreshInstances()
{
	Instances.RemoveAll([](const UTcsStateInstance* State)
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

	TArray<int32> InvalidIds;
	for (const TPair<int32, UTcsStateInstance*>& Pair: InstancesById)
	{
		if (!IsValid(Pair.Value))
		{
			InvalidIds.Add(Pair.Key);
			continue;
		}
		if (Pair.Value->GetCurrentStage() == ETcsStateStage::SS_Expired)
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
		InstanceArray.StateInstances.RemoveAll([](const UTcsStateInstance* State)
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
		if (InstanceArray.StateInstances.Num() == 0)
		{
			InvalidNames.Add(Pair.Key);
		}
	}
	for (FName Name : InvalidNames)
	{
		InstancesByName.Remove(Name);
	}
}

UTcsStateInstance* FTcsPersistentStateInstanceContainer::GetInstanceById(int32 InstanceId) const
{
	return InstancesById.Contains(InstanceId) ? InstancesById[InstanceId] : nullptr;
}

bool FTcsPersistentStateInstanceContainer::GetInstancesByName(
	FName StateDefId,
	TArray<UTcsStateInstance*>& OutInstances) const
{
	if (InstancesByName.Contains(StateDefId))
	{
		OutInstances = InstancesByName[StateDefId].StateInstances;
		return true;
	}

	OutInstances.Empty();
	return false;
}
