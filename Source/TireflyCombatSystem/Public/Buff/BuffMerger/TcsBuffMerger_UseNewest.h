// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "TcsBuffMerger_UseNewest.generated.h"



/**
 * Buff 合并器：使用最新
 *
 * 保留最新施加的状态实例，移除所有旧的状态实例。
 * 典型场景：单实例 Buff（如护盾），只保留最新施加的效果。
 */
UCLASS(Meta = (DisplayName = "Buff 合并器：使用最新"))
class TIREFLYCOMBATSYSTEM_API UTcsBuffMerger_UseNewest : public UTcsBuffMerger
{
	GENERATED_BODY()

public:
	virtual ETcsBuffMergeDependencyFlags GetDependencyFlags_Implementation() const override;

	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& MergedBuffs,
		TArray<UTcsBuffInstance*>& MergedOutBuffs) override;
};