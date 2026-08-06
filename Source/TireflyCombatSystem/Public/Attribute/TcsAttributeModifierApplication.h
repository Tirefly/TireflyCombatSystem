// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Attribute/AttrModOperation/TcsAttributeModifierOperation.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeModifierApplication.generated.h"



class UTcsStateInstance;
class UTcsSkillEntry;
class UTcsAttributeModifierNumericEvaluator;



/** AttributeModifier 的结算模式。 */
UENUM(BlueprintType)
enum class ETcsAttributeModifierApplicationMode : uint8
{
	AMAM_None = 0		UMETA(DisplayName = "None", ToolTip = "No AttributeModifier application mode is selected."),
	AMAM_Instant = 1	UMETA(DisplayName = "Instant", ToolTip = "Atomically apply operations to Attribute BaseValues."),
	AMAM_Ongoing = 2	UMETA(DisplayName = "Ongoing", ToolTip = "Store operations as revocable CurrentValue contributions."),
};



/** AttributeModifier Application 的结构化失败原因。 */
UENUM(BlueprintType)
enum class ETcsAttributeModifierApplicationFailure : uint8
{
	AMAF_None = 0					UMETA(DisplayName = "None", ToolTip = "The application has not failed."),
	AMAF_RuntimeNotPrepared = 1			UMETA(DisplayName = "Runtime Not Prepared", ToolTip = "The target AttributeComponent runtime is not prepared."),
	AMAF_InvalidRequest = 2				UMETA(DisplayName = "Invalid Request", ToolTip = "The application request is invalid."),
	AMAF_InvalidSourceHandle = 3			UMETA(DisplayName = "Invalid Source Handle", ToolTip = "The application request has an invalid SourceHandle."),
	AMAF_DefinitionNotFound = 4			UMETA(DisplayName = "Definition Not Found", ToolTip = "The requested AttributeModifier Definition was not found."),
	AMAF_NoOperations = 5				UMETA(DisplayName = "No Operations", ToolTip = "The AttributeModifier Definition contains no operations."),
	AMAF_InvalidOperationOverride = 6		UMETA(DisplayName = "Invalid Operation Override", ToolTip = "An operation override does not target a Definition operation."),
	AMAF_InvalidOperationSpec = 7			UMETA(DisplayName = "Invalid Operation Spec", ToolTip = "An AttributeModifier operation specification is invalid."),
	AMAF_TargetAttributeMissing = 8		UMETA(DisplayName = "Target Attribute Missing", ToolTip = "An operation target Attribute is missing from the target component."),
	AMAF_EvaluatorFailed = 9				UMETA(DisplayName = "Evaluator Failed", ToolTip = "An OperandEvaluator failed to produce an operand."),
	AMAF_InvalidOperand = 10				UMETA(DisplayName = "Invalid Operand", ToolTip = "An evaluated operand is not finite."),
	AMAF_OperatorFailed = 11				UMETA(DisplayName = "Operator Failed", ToolTip = "An AttributeModifier operator failed to produce a result."),
	AMAF_InvalidOperatorResult = 12		UMETA(DisplayName = "Invalid Operator Result", ToolTip = "An AttributeModifier operator produced a non-finite result."),
	AMAF_InvalidOngoingOwner = 13			UMETA(DisplayName = "Invalid Ongoing Owner", ToolTip = "The Ongoing application has no valid local StateInstance owner."),
	AMAF_DuplicateOngoingDefinition = 14	UMETA(DisplayName = "Duplicate Ongoing Definition", ToolTip = "The StateInstance already owns this ongoing AttributeModifier Definition."),
	AMAF_UnsupportedMerger = 15			UMETA(DisplayName = "Unsupported Merger", ToolTip = "The selected Ongoing merger is not supported by the current runtime stage."),
	AMAF_IncompatibleOperatorMerger = 16	UMETA(DisplayName = "Incompatible Operator Merger", ToolTip = "An Operator and Merger combination is forbidden."),
};



/** 单条 Operation 允许覆写的求值配置。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierOperationOverride
{
	GENERATED_BODY()

// Evaluator 覆写
#pragma region Evaluator

public:
	// 是否以 OperandEvaluatorClass 覆写 Definition 中的 Evaluator。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Override")
	bool bOverrideOperandEvaluator = false;

	// 覆写后的 CDO Numeric Evaluator。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Override",
		Meta = (EditCondition = "bOverrideOperandEvaluator", EditConditionHides))
	TSubclassOf<UTcsAttributeModifierNumericEvaluator> OperandEvaluatorClass;

#pragma endregion


// Payload 覆写
#pragma region Payload

public:
	// 是否以 OperandPayload 覆写 Definition 中的 Payload。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Override")
	bool bOverrideOperandPayload = false;

	// 覆写后的 Evaluator Payload。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Override",
		Meta = (EditCondition = "bOverrideOperandPayload", EditConditionHides,
			BaseStruct = "/Script/TireflyCombatSystem.TcsAttributeOperandPayload", ExcludeBaseStruct))
	FInstancedStruct OperandPayload;

#pragma endregion
};



/** 单条 Operation 的 Application 审计结果。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierOperationApplicationResult
{
	GENERATED_BODY()

// Operation 标识
#pragma region Identity

public:
	// Definition Operation Map 中的稳定 OperationId。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	FName OperationId = NAME_None;

	// Operation 目标 Attribute Definition Id。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	FName TargetAttributeId = NAME_None;

	// 本次 Operation 使用的来源句柄。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	FTcsSourceHandle SourceHandle;

#pragma endregion


// 结算结果
#pragma region Value

public:
	// Operator 写入候选值前的数值。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	float OldValue = 0.f;

	// Operator 写入候选值后的数值；最终 Clamp 结果由 Attribute 变化事件表达。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	float NewValue = 0.f;

	// 当前 Operation 是否已成功参与 Application。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	bool bSucceeded = false;

	// 当前 Operation 或 Application 失败的结构化原因。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	ETcsAttributeModifierApplicationFailure Failure = ETcsAttributeModifierApplicationFailure::AMAF_None;

#pragma endregion
};



/** AttributeModifier 唯一 Application 入口的输入。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierApplicationRequest
{
	GENERATED_BODY()

// Definition 与模式
#pragma region Application

public:
	// 要施加的 AttributeModifier Definition Id。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application",
		Meta = (GetOptions = "TcsGenericLibrary.GetAttributeModifierIds"))
	FName ModifierDefId = NAME_None;

	// 本次 Application 的统一结算模式。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application")
	ETcsAttributeModifierApplicationMode ApplicationMode = ETcsAttributeModifierApplicationMode::AMAM_None;

	// 所有 Application 都必须携带的有效来源句柄。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application")
	FTcsSourceHandle SourceHandle;

	// 以 OperationId 为 Key 的 Evaluator / Payload 覆写集合。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application")
	TMap<FName, FTcsAttributeModifierOperationOverride> OperationOverrides;

#pragma endregion


// 运行时来源上下文
#pragma region SourceContext

public:
	// Native-only 来源 StateInstance；Ongoing 必须由其持有。
	TWeakObjectPtr<UTcsStateInstance> SourceStateInstance;

	// Native-only 来源 SkillEntry；仅由要求该来源的 Evaluator 使用。
	TWeakObjectPtr<UTcsSkillEntry> SourceSkillEntry;

#pragma endregion
};



/** AttributeModifier 唯一 Application 入口的输出。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierApplicationResult
{
	GENERATED_BODY()

// Application 标识
#pragma region Application

public:
	// 本次请求的 AttributeModifier Definition Id。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	FName ModifierDefId = NAME_None;

	// 本次请求的结算模式。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	ETcsAttributeModifierApplicationMode ApplicationMode = ETcsAttributeModifierApplicationMode::AMAM_None;

	// 本次请求使用的来源句柄。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	FTcsSourceHandle SourceHandle;

	// 全部 Operation 是否作为一个原子 Application 成功提交。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	bool bSucceeded = false;

	// 整个 Application 的失败原因。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	ETcsAttributeModifierApplicationFailure Failure = ETcsAttributeModifierApplicationFailure::AMAF_None;

#pragma endregion


// Operation 审计
#pragma region Operations

public:
	// 按稳定 OperationId 顺序记录的逐条 Operation 结果。
	UPROPERTY(BlueprintReadOnly, Category = "Application Result")
	TArray<FTcsAttributeModifierOperationApplicationResult> OperationResults;

#pragma endregion


// 初始化
#pragma region Reset

public:
	// 清空上一次 Application 的审计数据。
	void Reset();

#pragma endregion
};
