// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateParameter.h"
#include "TireflyStateParameter_ConstNumeric.generated.h"



// 状态参数值结构体：常量数值
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyStateParam_ConstNumeric
{
	GENERATED_BODY()

public:
	// 常量数值
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float NumericValue = 0.f;
};



// 状态参数值解析器 - 常量数值
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateParamParser_ConstNumeric : public UTireflyStateParamParser
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
