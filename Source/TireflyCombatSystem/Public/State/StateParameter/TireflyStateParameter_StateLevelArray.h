// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateParameter.h"
#include "TireflyStateParameter_StateLevelArray.generated.h"



// 状态参数值结构体：基于状态等级的数组
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyStateParam_StateLevelArray
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
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateParamParser_StateLevelArray : public UTireflyStateParamParser
{
	GENERATED_BODY()

public:
	virtual void ParseStateParameter_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTireflyStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const override;
}; 