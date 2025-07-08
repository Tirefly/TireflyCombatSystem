// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateParameter.h"
#include "Engine/CurveTable.h"
#include "TireflyStateParameter_InstigatorLevelTable.generated.h"



// 状态参数值结构体：基于施法者等级的数值表
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyStateParam_InstigatorLevelTable
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



// 状态参数值解析器 - 基于施法者等级的数值表
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateParamParser_InstigatorLevelTable : public UTireflyStateParamParser
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