// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/StateParameter/TcsStateParameter.h"
#include "TcsStateParameter_InstigatorLevelArray.generated.h"



// 状态参数值结构体：基于施法者等级的数组
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateParam_InstigatorLevelArray
{
	GENERATED_BODY()

public:
	// 参数值数组，索引对应施法者等级（从0开始）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<float> LevelValues;

	// 默认值（当等级超出数组范围时使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DefaultValue = 0.f;
};



// 状态参数值解析器 - 基于施法者等级的数组
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateParamParser_InstigatorLevelArray : public UTcsStateParamEvaluator
{
	GENERATED_BODY()

public:
	virtual void Evaluate_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const override;
}; 