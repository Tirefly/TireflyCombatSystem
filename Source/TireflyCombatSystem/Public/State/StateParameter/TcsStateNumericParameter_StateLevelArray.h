// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "TcsStateNumericParameter_StateLevelArray.generated.h"



// 状态参数值结构体：基于状态等级的数组
USTRUCT(BlueprintType, DisplayName = "Numeric: State Level Array")
struct TIREFLYCOMBATSYSTEM_API FTcsStateNumericParam_StateLevelArray
{
	GENERATED_BODY()

public:
	// 参数值数组，索引对应状态等级（从0开始）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<float> LevelValues;

	// 默认值（当等级超出数组范围时使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DefaultValue = 0.f;
};



// 状态参数值解析器 - 基于状态等级的数组
UCLASS(DisplayName = "Numeric Evaluator: State Level Array")
class TIREFLYCOMBATSYSTEM_API UTcsStateNumericParamEvaluator_StateLevelArray : public UTcsStateNumericParamEvaluator
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const override;
}; 