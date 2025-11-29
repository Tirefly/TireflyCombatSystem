// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "TcsStateMerger_UseOldest.generated.h"



/**
 * 状态合并器：使用最旧
 *
 * 保留最早施加的状态实例，拒绝所有新的状态实例。
 * 典型场景：互斥 Buff（如已有霸体时，拒绝新的同类效果）。
 */
UCLASS(Meta = (DisplayName = "状态合并器：使用最旧"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_UseOldest : public UTcsStateMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) override;
};
