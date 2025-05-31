// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TireflyAttributeModifierMerger.h"
#include "TireflyAttrModMerger_NoMerge.generated.h"



// 属性修改器合并器：不合并
UCLASS(Meta = (DisplayName = "属性修改器合并器：不合并"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_NoMerge : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
