// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "TcsAttrClampStrategy_Linear.generated.h"



/**
 * 属性 Clamp 策略：线性约束
 *
 * 这是插件提供的默认 Clamp 策略实现，使用标准的线性约束逻辑。
 *
 * 行为：
 * - 使用 FMath::Clamp(Value, MinValue, MaxValue) 进行约束
 * - 如果 Value < MinValue，返回 MinValue
 * - 如果 Value > MaxValue，返回 MaxValue
 * - 否则返回 Value
 *
 * 所有属性定义默认使用此策略（通过构造函数设置）。
 */
UCLASS(Meta = (DisplayName = "属性 Clamp 策略：线性约束"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrClampStrategy_Linear : public UTcsAttributeClampStrategy
{
	GENERATED_BODY()

public:
	virtual float Clamp_Implementation(
		float Value,
		float MinValue,
		float MaxValue,
		const FTcsAttributeClampContextBase& Context,
		const FInstancedStruct& Config) override;
};
