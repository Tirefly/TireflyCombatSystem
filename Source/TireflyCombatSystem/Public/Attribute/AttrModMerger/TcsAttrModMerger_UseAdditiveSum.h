// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "TcsAttrModMerger_UseAdditiveSum.generated.h"



// 属性修改器合并器：取加法和值
UCLASS(Meta = (DisplayName = "属性修改器合并器：取加法和值"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModMerger_UseAdditiveSum : public UTcsAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers) override;
};
