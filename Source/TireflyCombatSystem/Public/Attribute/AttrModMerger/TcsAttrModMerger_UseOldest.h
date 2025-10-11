// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "TcsAttrModMerger_UseOldest.generated.h"



// 属性修改器合并器：取最旧
UCLASS(Meta = (DisplayName = "属性修改器合并器：取最旧"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModMerger_UseOldest : public UTcsAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers) override;
};
