// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluator.h"
#include "TcsAttrModOperand_CurrentValue.generated.h"



/** 指定目标组件内一个 Attribute CurrentValue 的 Operand Payload。 */
USTRUCT(BlueprintType, DisplayName = "Numeric Operand: Attribute Current Value")
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeCurrentValueOperandPayload : public FTcsAttributeOperandPayload
{
	GENERATED_BODY()

// Attribute 引用配置
#pragma region Attribute

public:
	// 要从当前求值 Snapshot 读取的 Attribute Definition Id。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand",
		Meta = (GetOptions = "TcsGenericLibrary.GetAttributeNames"))
	FName AttributeId = NAME_None;

#pragma endregion
};



/** 从目标组件求值 Snapshot 读取 Attribute CurrentValue 的 Evaluator。 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierCurrentValueOperandEvaluator : public UTcsAttributeModifierNumericEvaluator
{
	GENERATED_BODY()

public:
	/**
	 * 读取 Payload 指定的目标本地 Attribute CurrentValue。
	 *
	 * @param Context 当前只读 OperandEvaluator Context。
	 * @param Payload Attribute Id 配置。
	 * @param OutOperand 成功时输出 CurrentValue。
	 * @return Snapshot 中存在对应 Attribute 且结果有限时返回 true。
	 */
	virtual bool Evaluate(
		const FTcsAttributeOperandEvaluatorContext& Context,
		const FInstancedStruct& Payload,
		float& OutOperand) const override;
};
