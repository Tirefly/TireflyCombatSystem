// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluator.h"
#include "TcsAttrModOperand_StateParam.generated.h"



/** StateParam Operand 读取的显式来源类型。 */
UENUM(BlueprintType)
enum class ETcsAttributeStateParamOperandSource : uint8
{
	ASPOS_SourceStateInstance = 0	UMETA(DisplayName = "Source State Instance", ToolTip = "Read the numeric StateParam from SourceStateInstance."),
	ASPOS_SourceSkillEntry = 1		UMETA(DisplayName = "Source Skill Entry", ToolTip = "Read the numeric StateParam from SourceSkillEntry."),
};



/** 指定 Numeric StateParam 及其读取来源的 Operand Payload。 */
USTRUCT(BlueprintType, DisplayName = "Numeric Operand: State Parameter")
struct TIREFLYCOMBATSYSTEM_API FTcsStateParamOperandPayload : public FTcsAttributeOperandPayload
{
	GENERATED_BODY()

// StateParam 引用配置
#pragma region StateParameter

public:
	// 要读取 effective 值的 Numeric StateParam Tag。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand")
	FGameplayTag StateParamTag;

	// 读取 StateParam 的显式来源。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand")
	ETcsAttributeStateParamOperandSource Source = ETcsAttributeStateParamOperandSource::ASPOS_SourceStateInstance;

#pragma endregion
};



/** 从 EvaluatorContext 的 State 或 Skill 参数读取 effective 数值的 Evaluator。 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierStateParamOperandEvaluator : public UTcsAttributeModifierNumericEvaluator
{
	GENERATED_BODY()

public:
	/**
	 * 读取指定 Numeric StateParam 的无参 GetModifiedValue()。
	 *
	 * @param Context 当前只读 OperandEvaluator Context。
	 * @param Payload StateParam Tag 与来源配置。
	 * @param OutOperand 成功时输出 effective 数值。
	 * @return 来源、参数和类型均有效时返回 true。
	 */
	virtual bool Evaluate(
		const FTcsAttributeOperandEvaluatorContext& Context,
		const FInstancedStruct& Payload,
		float& OutOperand) const override;
};
