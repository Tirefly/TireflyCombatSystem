// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModOperation/TcsAttributeOperandEvaluator.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsAttributeModifierOperation.generated.h"



class UTcsAttributeModifierCustomOperator;



/** AttributeModifier 内建数值 Operator。 */
UENUM(BlueprintType)
enum class ETcsAttributeModifierOperator : uint8
{
	// 未选择 Operator。
	AMO_None = 0				UMETA(DisplayName = "None", ToolTip = "No AttributeModifier operator is selected."),
	// 使用 Custom Operator 类。
	AMO_Custom = 1			UMETA(DisplayName = "Custom", ToolTip = "Apply the selected custom AttributeModifier operator."),
	// 将求值后的 Operand 加到当前值。
	AMO_Add = 2				UMETA(DisplayName = "Add", ToolTip = "Calculate CurrentValue + Operand."),
	// 以一加上求值后的 Operand 进行乘算。
	AMO_MultiplyAdditive = 3	UMETA(DisplayName = "Multiply Additive", ToolTip = "Calculate CurrentValue * (1 + Operand)."),
	// 以求值后的 Operand 进行乘算。
	AMO_MultiplyCompound = 4	UMETA(DisplayName = "Multiply Compound", ToolTip = "Calculate CurrentValue * Operand."),
	// 以求值后的 Operand 覆盖当前值。
	AMO_Override = 5			UMETA(DisplayName = "Override", ToolTip = "Use Operand as the resulting value."),
};



/** 已完成 Operand 求值、尚未写入 Attribute 的运行时 Operation。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsEvaluatedAttributeOperation
{
	GENERATED_BODY()

// 已解析的 Operation 标识
#pragma region Identity

public:
	// 父 AttributeModifier Definition 中的稳定 OperationId。
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FName OperationId = NAME_None;

	// 已解析 Operation 选定的 Attribute Definition Id。
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FName TargetAttributeId = NAME_None;

#pragma endregion


// 已解析的 Operator 配置
#pragma region Operator

public:
	// Operation Definition 选定的内建 Operator。
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	ETcsAttributeModifierOperator Operator = ETcsAttributeModifierOperator::AMO_None;

	// 当 Operator 为 AMO_Custom 时选定的 CDO 策略。
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass;

#pragma endregion


// 已求值数值
#pragma region Operand

public:
	// Operation Evaluator 生成的数值。
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	float EvaluatedOperand = 0.f;

#pragma endregion
};



/** 以 OperationId Map Key 存储的一条可创作 AttributeModifier Operation。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeOperationSpec
{
	GENERATED_BODY()

// 目标 Attribute 配置
#pragma region Target

public:
	// 本 Operation 修改的 Attribute Definition Id。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation",
		Meta = (GetOptions = "TcsGenericLibrary.GetAttributeNames"))
	FName TargetAttributeId = NAME_None;

#pragma endregion


// Operator 配置
#pragma region Operator

public:
	// 内建 Operator 选择；AMO_Custom 要求配置 CustomOperatorClass。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	ETcsAttributeModifierOperator Operator = ETcsAttributeModifierOperator::AMO_None;

	// Operator 为 AMO_Custom 时使用的 CDO 策略。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation",
		Meta = (EditCondition = "Operator == ETcsAttributeModifierOperator::AMO_Custom", EditConditionHides))
	TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass;

#pragma endregion


// Operand 求值配置
#pragma region Operand

public:
	// 将 OperandPayload 转换为数值 Operand 的 CDO Evaluator。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<UTcsAttributeModifierNumericEvaluator> OperandEvaluatorClass;

	// 仅由 OperandEvaluatorClass 消费的创作 Payload。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation",
		Meta = (BaseStruct = "/Script/TireflyCombatSystem.TcsAttributeOperandPayload", ExcludeBaseStruct))
	FInstancedStruct OperandPayload;

#pragma endregion


// 构造函数
#pragma region Construction

public:
	// 设置默认 Constant Operand Evaluator 与 Payload。
	FTcsAttributeOperationSpec();

#pragma endregion
};



/**
 * 施加内建或 Custom AttributeModifier Operator。
 *
 * @param Operator 内建 Operator 选择。
 * @param CustomOperatorClass Operator 为 AMO_Custom 时使用的 Custom CDO 策略。
 * @param CurrentValue Operation 执行前的 Attribute 值。
 * @param Operand 已求值数值 Operand。
 * @param OutValue 成功时输出结果 Attribute 值。
 * @return Operator 成功生成结果时返回 true。
 */
TIREFLYCOMBATSYSTEM_API bool ApplyTcsAttributeModifierOperator(
	ETcsAttributeModifierOperator Operator,
	TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass,
	float CurrentValue,
	float Operand,
	float& OutValue);
