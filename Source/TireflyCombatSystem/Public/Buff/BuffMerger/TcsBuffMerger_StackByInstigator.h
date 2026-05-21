// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "TcsBuffMerger_StackByInstigator.generated.h"



// Buff 合并器：按发起者叠层
UCLASS(Meta = (DisplayName = "Buff 合并器：按发起者叠层"))
class TIREFLYCOMBATSYSTEM_API UTcsBuffMerger_StackByInstigator : public UTcsBuffMerger
{
	GENERATED_BODY()

public:
	virtual ETcsBuffMergeDependencyFlags GetDependencyFlags_Implementation() const override;

	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& MergedBuffs,
		TArray<UTcsBuffInstance*>& MergedOutBuffs) override;
};