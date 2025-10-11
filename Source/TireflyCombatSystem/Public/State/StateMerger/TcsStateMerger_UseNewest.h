// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "TcsStateMerger_UseNewest.generated.h"



// 状态合并器：保留最新
UCLASS(Meta = (DisplayName = "状态合并器：保留最新"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_UseNewest : public UTcsStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) override;
}; 