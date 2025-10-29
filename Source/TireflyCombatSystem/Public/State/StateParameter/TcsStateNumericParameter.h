// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsStateNumericParameter.generated.h"



class UTcsStateInstance;



// 状态参数值结构体：常量数值
USTRUCT(BlueprintType, DisplayName = "Numeric：Constant")
struct TIREFLYCOMBATSYSTEM_API FTcsStateNumericParam_Constant
{
	GENERATED_BODY()

public:
	// 常量数值
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float NumericValue = 0.f;
};



// 状态数值类参数值解析器
UCLASS(BlueprintType, Blueprintable,
	ClassGroup = (TireflyCombatSystem),
	DisplayName="Numeric Evaluator: Constant")
class TIREFLYCOMBATSYSTEM_API UTcsStateNumericParamEvaluator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 解析状态参数值
	 * 
	 * @param Instigator 发起者
	 * @param Target 目标
	 * @param StateInstance 状态实例
	 * @param InstancedStruct 参数值结构体
	 * @param OutValue 解析的参数值结果
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|State")
	bool Evaluate(
		AActor* Instigator,
		AActor* Target,
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const;
	virtual bool Evaluate_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const;
};
