// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "State/StateCondition/TcsStateCondition.h"
#include "TcsGenericEnum.h"
#include "TcsStateCondition_ParameterBased.generated.h"



// 参数基础条件Payload
USTRUCT(BlueprintType)
struct FTcsStateConditionPayload_ParameterBased
{
	GENERATED_BODY()

public:
	// 要比较的参数标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter Comparison")
	FGameplayTag ParameterTag;

	// 比较操作
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter Comparison")
	ETcsNumericComparison ComparisonType = ETcsNumericComparison::Equal;

	// 比较值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter Comparison")
	float CompareValue = 0.0f;
};



// 基于参数比较的状态条件
UCLASS(BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateCondition_ParameterBased : public UTcsStateCondition
{
	GENERATED_BODY()

public:
	// 检查条件实现
	virtual bool CheckCondition_Implementation(
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& Payload,
		float CurrentGameTime) override;
}; 