// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "TcsAttrModMerger_NoMerge.generated.h"



// 属性修改器合并器：不合并
UCLASS(Meta = (DisplayName = "属性修改器合并器：不合并"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModMerger_NoMerge : public UTcsAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers) override;
};
