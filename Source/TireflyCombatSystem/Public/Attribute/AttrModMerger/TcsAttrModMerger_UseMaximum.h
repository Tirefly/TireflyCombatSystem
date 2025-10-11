// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "TcsAttrModMerger_UseMaximum.generated.h"



// 属性修改器合并器：取操作数最大
UCLASS(Meta = (DisplayName = "属性修改器合并器：取操作数最大"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModMerger_UseMaximum : public UTcsAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers) override;
};
