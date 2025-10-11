// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "TcsStateMerger_NoMerge.generated.h"



// 状态合并器：不合并
UCLASS(Meta = (DisplayName = "状态合并器：不合并"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_NoMerge : public UTcsStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) override;
}; 