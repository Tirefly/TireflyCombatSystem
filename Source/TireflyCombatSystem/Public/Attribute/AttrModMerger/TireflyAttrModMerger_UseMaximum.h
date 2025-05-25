// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TireflyAttributeModifierMerger.h"
#include "TireflyAttrModMerger_UseMaximum.generated.h"



// 属性修改器合并操作：取操作数最大
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取操作数最大"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseMaximum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
