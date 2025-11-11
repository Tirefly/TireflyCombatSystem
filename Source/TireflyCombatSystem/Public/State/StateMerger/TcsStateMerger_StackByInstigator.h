// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "TcsStateMerger_StackByInstigator.generated.h"



// 状态合并器：按发起者叠层
UCLASS(Meta = (DisplayName = "状态合并器：按发起者叠层"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_StackByInstigator : public UTcsStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) override;
}; 