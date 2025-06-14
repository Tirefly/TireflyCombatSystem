// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateMerger.h"
#include "TireflyStateMerger_UseNewest.generated.h"

// 状态合并器：使用最新状态
UCLASS(Meta = (DisplayName = "状态合并器：使用最新状态"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateMerger_UseNewest : public UTireflyStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyStateInstance>& StatesToMerge,
		TArray<FTireflyStateInstance>& MergedStates,
		bool bSameInstigator) override;
}; 