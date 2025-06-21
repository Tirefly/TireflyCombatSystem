// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateCondition.h"
#include "TireflyCombatSystemEnum.h"
#include "TireflyStateCondition_ParameterBased.generated.h"



// 参数基础条件Payload
USTRUCT(BlueprintType)
struct FTireflyStateConditionPayload_ParameterBased
{
	GENERATED_BODY()

public:
	// 要比较的参数名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter Comparison")
	FName ParameterName = NAME_None;

	// 比较操作
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter Comparison")
	ETireflyNumericComparison ComparisonType = ETireflyNumericComparison::Equal;

	// 比较值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter Comparison")
	float CompareValue = 0.0f;
};



// 基于参数比较的状态条件
UCLASS(BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyStateCondition_ParameterBased : public UTireflyStateCondition
{
	GENERATED_BODY()

public:
	// 检查条件实现
	virtual bool CheckCondition_Implementation(
		UTireflyStateInstance* StateInstance,
		const FInstancedStruct& Payload,
		float CurrentGameTime) override;
}; 