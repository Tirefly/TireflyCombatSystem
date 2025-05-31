// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TireflyAttributeModifier.h"
#include "TireflyAttributeModifierMerger.generated.h"



// 属性修改器合并器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeModifierMerger : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器合并器
	 * 
	 * @param ModifiersToMerge 要合并的属性修改器
	 * @param MergedModifiers 合并后的属性修改器
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Merge(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers);
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) {}
};
