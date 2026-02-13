// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "TcsStateNumericParameter_InstigatorLevelMap.generated.h"



// 状态参数值结构体：基于施法者等级的映射表
USTRUCT(BlueprintType, DisplayName = "Numeric: Instigator Level Map")
struct TIREFLYCOMBATSYSTEM_API FTcsStateNumericParam_InstigatorLevelMap
{
	GENERATED_BODY()

public:
	// 参数值映射表，键为施法者等级，值为对应的参数值
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<int32, float> LevelValues;

	// 默认值（当等级不在映射表中时使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DefaultValue = 0.f;
};



// 状态参数值解析器 - 基于施法者等级的映射表
UCLASS(DisplayName = "Numeric Evaluator: Instigator Level Map")
class TIREFLYCOMBATSYSTEM_API UTcsStateNumericParamEvaluator_InstigatorLevelMap : public UTcsStateNumericParamEvaluator
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
