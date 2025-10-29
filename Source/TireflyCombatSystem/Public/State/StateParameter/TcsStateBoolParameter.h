// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsStateBoolParameter.generated.h"



class UTcsStateInstance;



// 状态参数值结构体：常量布尔
USTRUCT(BlueprintType, DisplayName = "Bool：Constant")
struct TIREFLYCOMBATSYSTEM_API FTcsStateBoolParam_Constant
{
	GENERATED_BODY()

public:
	// 布尔值
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bBoolValue = false;
	
};




// 状态布尔类参数值解析器
UCLASS(DisplayName = "Bool Evaluator：Constant")
class TIREFLYCOMBATSYSTEM_API UTcsStateBoolParamEvaluator : public UObject
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
		bool& OutValue) const;
	virtual bool Evaluate_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		bool& OutValue) const;
};
