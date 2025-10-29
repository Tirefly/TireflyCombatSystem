// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsStateVectorParameter.generated.h"



class UTcsStateInstance;



// 状态参数值结构体：常量向量
USTRUCT(BlueprintType, DisplayName = "Vector：Constant")
struct TIREFLYCOMBATSYSTEM_API FTcsStateVectorParam_Constant
{
	GENERATED_BODY()

public:
	// 向量值
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector VectorValue;
	
};




// 状态向量类参数值解析器
UCLASS(DisplayName = "Vector Evaluator：Constant")
class TIREFLYCOMBATSYSTEM_API UTcsStateVectorParamEvaluator : public UObject
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
		FVector& OutValue) const;
	virtual bool Evaluate_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		FVector& OutValue) const;
};
