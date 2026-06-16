// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffDurationTracker.h"

#include "Buff/TcsBuffInstance.h"



void FTcsBuffDurationTracker::Add(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	TrackedInstances.Add(BuffInstance);
}

void FTcsBuffDurationTracker::Remove(UTcsBuffInstance* BuffInstance)
{
	if (!BuffInstance)
	{
		return;
	}

	TrackedInstances.Remove(BuffInstance);
}

void FTcsBuffDurationTracker::RefreshInstances()
{
	TArray<TObjectPtr<UTcsBuffInstance>> InvalidBuffs;
	for (const TObjectPtr<UTcsBuffInstance>& BuffInstance : TrackedInstances)
	{
		if (!IsValid(BuffInstance) || BuffInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			InvalidBuffs.Add(BuffInstance);
		}
	}

	for (const TObjectPtr<UTcsBuffInstance>& Buff : InvalidBuffs)
	{
		TrackedInstances.Remove(Buff);
	}
}