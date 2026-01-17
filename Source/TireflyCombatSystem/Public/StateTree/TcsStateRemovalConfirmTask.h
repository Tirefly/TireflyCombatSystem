// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TcsStateRemovalConfirmTask.generated.h"

/**
 * StateTree task: explicitly confirm a pending removal request by stopping the StateTree execution.
 * Intended usage: place this node at the end of your "Cancelled/Expired/Removed" handling branch.
 */
USTRUCT(meta = (DisplayName = "Tcs State Removal Confirm Task"))
struct TIREFLYCOMBATSYSTEM_API FTcsStateRemovalConfirmTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FTcsStateRemovalConfirmTask();

	/** Optional: for debugging only. If true, warns when pending removal reason is not Cancelled. */
	UPROPERTY(EditAnywhere, Category = "Removal")
	bool bCancelled = false;

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

