// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Attribute/TcsAttributeModifier.h"
#include "TcsAttributeModifierExecution.generated.h"



// 属性修改器执行器
UCLASS(Abstract, BlueprintType, Abstract, ClassGroup = (TireflyCombatSystem))
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
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Execute(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues);
	virtual void Execute_Implementation(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) {}

	/**
	 * 收集本次执行可能直接写入的属性名集合。
	 *
	 * 该声明只用于帮助 AttributeComponent 缩小差异检测范围，
	 * 不参与真正的执行逻辑，也不会改变 Execute 的写入结果。
	 *
	 * @param ModInst 当前执行的修改器实例
	 * @param OutAttributeNames 输出的 touched 属性名集合
	 * 返回 false 表示当前执行器未声明 touched 集，调用方应回退到更保守的全表差异检测路径。
	 * @return 是否提供了可用于局部差异检测的 touched 集声明
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	bool CollectTouchedAttributes(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TArray<FName>& OutAttributeNames) const;
	virtual bool CollectTouchedAttributes_Implementation(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TArray<FName>& OutAttributeNames) const;
};
