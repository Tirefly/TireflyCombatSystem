// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "TcsAttributeModifierOperandEvaluator.generated.h"



struct FTcsAttributeOperandEvaluatorContext;



/** 属性修改器数值 Evaluator 消费的创作 Payload 基类。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeOperandPayload
{
	GENERATED_BODY()
};



/** 将 AttributeModifier Payload 求值为 float Operand 的 CDO 策略。 */
UCLASS(Abstract)
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierNumericEvaluator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 为一条 AttributeModifier Operation 求值数值 Operand。
	 *
	 * @param Context 只读 Application Context 与 Evaluation Snapshot。
	 * @param Payload Operation 选择的创作 Payload。
	 * @param OutOperand 成功时输出已求值的数值 Operand。
	 * @return 成功生成有效 Operand 时返回 true。
	 */
	virtual bool Evaluate(
		const FTcsAttributeOperandEvaluatorContext& Context,
		const FInstancedStruct& Payload,
		float& OutOperand) const PURE_VIRTUAL(, return false;);
};
