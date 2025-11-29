// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "TcsStateMerger_UseNewest.generated.h"



/**
 * 状态合并器：使用最新
 *
 * 保留最新施加的状态实例，移除所有旧的状态实例。
 * 典型场景：单实例 Buff（如护盾），只保留最新施加的效果。
 */
UCLASS(Meta = (DisplayName = "状态合并器：使用最新"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_UseNewest : public UTcsStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) override;
};
