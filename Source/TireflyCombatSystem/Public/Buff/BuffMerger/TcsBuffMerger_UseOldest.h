// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "TcsBuffMerger_UseOldest.generated.h"



/**
 * Buff 合并器：使用最旧
 *
 * 保留最早施加的状态实例，拒绝所有新的状态实例。
 * 典型场景：互斥 Buff（如已有霸体时，拒绝新的同类效果）。
 */
UCLASS(Meta = (DisplayName = "Buff 合并器：使用最旧"))
class TIREFLYCOMBATSYSTEM_API UTcsBuffMerger_UseOldest : public UTcsBuffMerger
{
	GENERATED_BODY()

public:
	virtual ETcsBuffMergeDependencyFlags GetDependencyFlags_Implementation() const override;

	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& MergedBuffs,
		TArray<UTcsBuffInstance*>& MergedOutBuffs) override;
};