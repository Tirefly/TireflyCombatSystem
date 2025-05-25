// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TireflyAttributeModifier.h"
#include "TireflyAttributeModifierExecution.generated.h"



// 属性修改器执行算法
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeModifierExecution : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器执行算法
	 * 
	 * @param Instigator 属性修改流程的发起者
	 * @param Target 属性修改流程的目标
	 * @param SourceObject 属性修改流程的源对象（技能、Buff）
	 * @param ModInst 属性修改器实例
	 * @param BaseValues 要修改的所有属性的基础值
	 * @param CurrentValues 要修改的所有属性的当前值
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Execute(
		AActor* Instigator,
		AActor* Target,
		UObject* SourceObject,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues);
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		UObject* SourceObject,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) {}
};
