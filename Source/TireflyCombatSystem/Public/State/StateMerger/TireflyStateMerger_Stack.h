// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateMerger.h"
#include "TireflyStateMerger_Stack.generated.h"

// 状态合并器：状态叠加
UCLASS(Meta = (DisplayName = "状态合并器：状态叠加"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateMerger_Stack : public UTireflyStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyStateInstance>& StatesToMerge,
		TArray<FTireflyStateInstance>& MergedStates,
		bool bSameInstigator) override;
}; 