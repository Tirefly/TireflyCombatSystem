// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateMerger.h"
#include "TireflyStateMerger_Stack.generated.h"



// 状态合并器：叠层合并
UCLASS(Meta = (DisplayName = "状态合并器：叠层合并"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateMerger_Stack : public UTireflyStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTireflyStateInstance*>& StatesToMerge,
		TArray<UTireflyStateInstance*>& MergedStates) override;
}; 