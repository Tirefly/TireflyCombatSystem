// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsStateMerger.h"
#include "TcsStateMerger_StackDirectly.generated.h"



// 状态合并器：直接叠层（无视来源）
UCLASS(Meta = (DisplayName = "状态合并器：直接叠层"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_StackDirectly : public UTcsStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) override;
};
