// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "TcsBuffMerger_NoMerge.generated.h"



// Buff 合并器：不合并
UCLASS(Meta = (DisplayName = "Buff 合并器：不合并"))
class TIREFLYCOMBATSYSTEM_API UTcsBuffMerger_NoMerge : public UTcsBuffMerger
{
	GENERATED_BODY()

public:
	virtual ETcsBuffMergeDependencyFlags GetDependencyFlags_Implementation() const override;

	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& MergedBuffs,
		TArray<UTcsBuffInstance*>& MergedOutBuffs) override;
};