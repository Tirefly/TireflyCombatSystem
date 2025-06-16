// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateMerger.h"
#include "TireflyStateMerger_UseOldest.generated.h"



// 状态合并器：保留最旧
UCLASS(Meta = (DisplayName = "状态合并器：保留最旧"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateMerger_UseOldest : public UTireflyStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTireflyStateInstance*>& StatesToMerge,
		TArray<UTireflyStateInstance*>& MergedStates) override;
}; 