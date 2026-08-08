// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/TcsAttributeComponent_AttrModHelpers.h"
#include "TcsLogChannels.h"



bool UTcsAttributeComponent::RecalculateDirtyOngoingModifierInstances(
	const TSet<int32>& DirtyModifierInstIds,
	TArray<FTcsAttributeModifierInstance>& OutModifierInstances,
	TMap<FName, float>& OutCurrentValues,
	TSet<int32>& OutRecalculatedModifierInstIds,
	FString& OutFailureDiagnostic)
{
	OutModifierInstances.Reset();
	OutCurrentValues.Reset();
	OutRecalculatedModifierInstIds.Reset();
	OutFailureDiagnostic.Reset();

	// Dirty flush is an Attribute transaction: re-evaluate all registered parents,
	// including empty-contribution ones, with temporary-skip semantics.
	TSet<int32> SeedIds = DirtyModifierInstIds;
	for (const FTcsAttributeModifierInstance& ModifierInstance : AttributeModifiers)
	{
		if (ModifierInstance.AppliedOperations.IsEmpty())
		{
			SeedIds.Add(ModifierInstance.ModifierInstId);
		}
	}

	// Expand the dirty seed through the last committed dependency graph first so
	// BuildOngoingAttributeValues can rebuild the whole affected set.
	TArray<int32> SeedOrder;
	FString SeedDiagnostic;
	if (!SeedIds.IsEmpty())
	{
		BuildOngoingDependencyEvaluationOrder(
			AttributeModifiers,
			SeedIds,
			SeedOrder,
			SeedDiagnostic);
		// Cycle among committed parents is handled inside skip-mode rebuild.
	}

	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	if (!BuildOngoingAttributeValues(
		BaseValues,
		AttributeModifiers,
		OutModifierInstances,
		OutCurrentValues,
		INDEX_NONE,
		nullptr,
		true))
	{
		OutFailureDiagnostic = SeedDiagnostic.IsEmpty()
			? TEXT("Failed to rebuild Dirty Ongoing contributions with temporary skip.")
			: MoveTemp(SeedDiagnostic);
		return false;
	}

	for (const FTcsAttributeModifierInstance& ModifierInstance : OutModifierInstances)
	{
		OutRecalculatedModifierInstIds.Add(ModifierInstance.ModifierInstId);
	}
	return true;
}

void UTcsAttributeComponent::FlushDirtyOngoingModifiers(
	const TMap<FName, float>* PreviousBaseValues,
	const TMap<FName, float>* PreviousCurrentValues,
	bool bBroadcastEvents)
{
	if (bIsFlushingDirtyOngoingModifiers)
	{
		return;
	}
	if (bIsBroadcastingAttributeStateDiffs)
	{
		if (!bHasDeferredAttributeEventBaseline)
		{
			DeferredEventBaseValues = ActiveBroadcastBaseValues;
			DeferredEventCurrentValues = ActiveBroadcastCurrentValues;
			bHasDeferredAttributeEventBaseline = true;
		}
		bDeferredBroadcastEvents = true;
		SetComponentTickEnabled(true);
		return;
	}

	const TMap<FName, float> EventBaseValues = bHasDeferredAttributeEventBaseline
		? DeferredEventBaseValues
		: (PreviousBaseValues ? *PreviousBaseValues : GetAttributeBaseValues());
	const TMap<FName, float> EventCurrentValues = bHasDeferredAttributeEventBaseline
		? DeferredEventCurrentValues
		: (PreviousCurrentValues ? *PreviousCurrentValues : GetAttributeValues());
	const bool bShouldBroadcastEvents = bBroadcastEvents || bDeferredBroadcastEvents;
	DeferredEventBaseValues.Reset();
	DeferredEventCurrentValues.Reset();
	bHasDeferredAttributeEventBaseline = false;
	bDeferredBroadcastEvents = false;

	if (!DirtyOngoingModifierInstIds.IsEmpty())
	{
		TSet<int32> DirtyBatch = MoveTemp(DirtyOngoingModifierInstIds);
		DirtyOngoingModifierInstIds.Reset();
		TGuardValue<bool> FlushGuard(bIsFlushingDirtyOngoingModifiers, true);

		// Any Attribute transaction re-tries empty-contribution parents.
		for (const FTcsAttributeModifierInstance& ModifierInstance : AttributeModifiers)
		{
			if (ModifierInstance.AppliedOperations.IsEmpty())
			{
				DirtyBatch.Add(ModifierInstance.ModifierInstId);
			}
		}

		TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
		TMap<FName, float> CurrentValues;
		TSet<int32> RecalculatedModifierInstIds;
		FString FailureDiagnostic;
		if (RecalculateDirtyOngoingModifierInstances(
			DirtyBatch,
			UpdatedModifierInstances,
			CurrentValues,
			RecalculatedModifierInstIds,
			FailureDiagnostic))
		{
			CommitAttributeModifierTransaction(
				GetAttributeBaseValues(),
				CurrentValues,
				UpdatedModifierInstances,
				true,
				false,
				&RecalculatedModifierInstIds);
		}
		else
		{
			// Keep the batch retryable; do not drop dependency invalidations on flush failure.
			DirtyOngoingModifierInstIds.Append(DirtyBatch);
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Rejected Dirty Ongoing Flush for %s: %s"),
				*FString(__FUNCTION__),
				*GetPathNameSafe(this),
				*FailureDiagnostic);
		}
	}

	if (bShouldBroadcastEvents)
	{
		BroadcastAttributeStateDiffs(EventBaseValues, EventCurrentValues);
	}
	SetComponentTickEnabled(
		!DirtyOngoingModifierInstIds.IsEmpty() ||
		bHasDeferredAttributeEventBaseline);
}
