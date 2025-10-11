// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Attribute/TcsAttributeModifier.h"
#include "TcsAttributeModifierExecution.generated.h"



// 属性修改器执行器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierExecution : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器执行器
	 * 
	 * @param ModInst 属性修改器实例
	 * @param BaseValues 要修改的所有属性的基础值
	 * @param CurrentValues 要修改的所有属性的当前值
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TcsCombatSystem)
	void Execute(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues);
	virtual void Execute_Implementation(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) {}
};
