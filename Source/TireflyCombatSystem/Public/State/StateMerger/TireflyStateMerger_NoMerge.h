// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateMerger.h"
#include "TireflyStateMerger_NoMerge.generated.h"

// 状态合并器：不合并
UCLASS(Meta = (DisplayName = "状态合并器：不合并"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateMerger_NoMerge : public UTireflyStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyStateInstance>& StatesToMerge,
		TArray<FTireflyStateInstance>& MergedStates,
		bool bSameInstigator) override;
}; 