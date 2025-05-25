// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TireflyAttributeModifierMerger.h"
#include "TireflyAttrModMerger_UseMinimum.generated.h"



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseMinimum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
