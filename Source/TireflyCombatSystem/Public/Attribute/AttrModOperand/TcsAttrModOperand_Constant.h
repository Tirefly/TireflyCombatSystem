// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluator.h"
#include "TcsAttrModOperand_Constant.generated.h"



/** 静态数值操作数 Payload。 */
USTRUCT(BlueprintType, DisplayName = "Numeric Operand: Constant")
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeConstantOperandPayload : public FTcsAttributeOperandPayload
{
	GENERATED_BODY()

// 常量操作数创作数据
#pragma region Constant

public:
	// 常量 Evaluator 返回的数值操作数。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand")
	float Value = 0.f;

#pragma endregion
};



/** 读取 FTcsAttributeConstantOperandPayload 的内建 Evaluator。 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierConstantOperandEvaluator : public UTcsAttributeModifierNumericEvaluator
{
	GENERATED_BODY()

public:
	// 返回 FTcsAttributeConstantOperandPayload 中配置的数值。
	virtual bool Evaluate(
		const FTcsAttributeOperandEvaluatorContext& Context,
		const FInstancedStruct& Payload,
		float& OutOperand) const override;
};
