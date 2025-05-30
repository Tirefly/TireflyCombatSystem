// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TireflyAttributeModifierMerger.h"
#include "TireflyAttrModMerger_UseOldest.generated.h"



// 属性修改器合并器：取最旧
UCLASS(Meta = (DisplayName = "属性修改器合并器：取最旧"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseOldest : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
