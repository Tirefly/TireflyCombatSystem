// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsAttributeModifierCustomOperator.generated.h"



/** 为使用 Custom 数值 Operator 的 AttributeModifier Operation 提供的 CDO 策略。 */
UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierCustomOperator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 将 Custom Operator 施加到当前值与已求值 Operand。
	 *
	 * @param CurrentValue 当前 Operation 执行前的 Attribute 值。
	 * @param Operand Operation Evaluator 生成的数值 Operand。
	 * @param OutValue 成功时输出结果 Attribute 值。
	 * @return 成功生成有效结果值时返回 true。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|Attribute")
	bool Apply(float CurrentValue, float Operand, float& OutValue) const;
	virtual bool Apply_Implementation(float CurrentValue, float Operand, float& OutValue) const;
};
