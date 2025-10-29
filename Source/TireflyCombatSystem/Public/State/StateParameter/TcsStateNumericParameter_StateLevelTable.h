// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "Engine/CurveTable.h"
#include "TcsStateNumericParameter_StateLevelTable.generated.h"



// 状态参数值结构体：基于状态等级的数值表
USTRUCT(BlueprintType, DisplayName = "Numeric: State Level Table")
struct TIREFLYCOMBATSYSTEM_API FTcsStateNumericParam_StateLevelTable
{
	GENERATED_BODY()

public:
	// 曲线数值表资产，包含等级到数值的映射
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCurveTableRowHandle CurveTableRowHandle;

	// 默认值（当无法获取曲线数据时使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DefaultValue = 0.f;
};



// 状态参数值解析器 - 基于状态等级的数值表
UCLASS(DisplayName = "Numeric Evaluator: State Level Table")
class TIREFLYCOMBATSYSTEM_API UTcsStateNumericParamEvaluator_StateLevelTable : public UTcsStateNumericParamEvaluator
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