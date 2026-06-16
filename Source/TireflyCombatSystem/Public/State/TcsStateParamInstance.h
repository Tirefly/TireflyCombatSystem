// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsSourceHandle.h"
#include "Skill/TcsSkillModifierInstance.h"
#include "TcsStateParamInstance.generated.h"


class UTcsStateInstance;
class UTcsStateNumericParamEvaluator;
class UTcsStateBoolParamEvaluator;
class UTcsStateVectorParamEvaluator;



// 状态参数类型枚举
UENUM(BlueprintType)
enum class ETcsStateParameterType : uint8
{
	SPT_Numeric = 0		UMETA(DisplayName = "Numeric", ToolTip = "数值类型参数(Float)，需要使用参数解析器计算"),
	SPT_Bool			UMETA(DisplayName = "Bool", ToolTip = "布尔类型参数，直接存储使用"),
	SPT_Vector			UMETA(DisplayName = "Vector", ToolTip = "向量类型参数，直接存储使用"),
};



// 状态参数数据（定义层，编辑器配置）
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateParameter
{
	GENERATED_BODY()

public:
	// 参数类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Type")
	ETcsStateParameterType ParameterType = ETcsStateParameterType::SPT_Numeric;

	// 快照配置（快照参数在激活时计算一次；非快照参数会实时同步变化）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Policy",
		meta = (ToolTip = "是否为快照参数：快照参数在技能激活时计算一次；非快照参数会实时同步变化"))
	bool bIsSnapshot = true;

	// 参数值提取类（仅 Numeric 类型使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Parameter",
		meta = (EditCondition = "ParameterType == ETcsStateParameterType::SPT_Numeric", EditConditionHides))
	TSubclassOf<UTcsStateNumericParamEvaluator> NumericParamEvaluator;

	// 参数值提取类（仅 Bool 类型使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Parameter",
		meta = (EditCondition = "ParameterType == ETcsStateParameterType::SPT_Bool", EditConditionHides))
	TSubclassOf<UTcsStateBoolParamEvaluator> BoolParamEvaluator;

	// 参数值提取类（仅 Vector 类型使用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector Parameter",
		meta = (EditCondition = "ParameterType == ETcsStateParameterType::SPT_Vector", EditConditionHides))
	TSubclassOf<UTcsStateVectorParamEvaluator> VectorParamEvaluator;

	// 参数值容器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Value")
	FInstancedStruct ParamValueContainer;
};


/**
 * Numeric 类型运行时 StateParam 实例。
 *
 * 持有 Numeric 专用 Evaluator CDO 和 Snapshot 策略，通过 GetValue() 返回当前数值。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsNumericStateParamInstance
{
	GENERATED_BODY()

#pragma region Identity

public:
	// 参数标识（GameplayTag）
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayTag ParamTag;

	// 是否为快照参数（true=首次求值后缓存，false=每次都重新求值）
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	bool bIsSnapshot = true;

#pragma endregion


#pragma region Evaluator

public:
	// 参数数据（FInstancedStruct，从 Definition 复制，供 Evaluator 使用）
	UPROPERTY()
	FInstancedStruct ParamData;

	// 求值器类型
	UPROPERTY()
	TSubclassOf<UTcsStateNumericParamEvaluator> NumericEvaluatorClass;

	// 缓存求值器 CDO（初始化时获取并校验）
	UPROPERTY()
	TObjectPtr<UTcsStateNumericParamEvaluator> CachedEvaluator;

#pragma endregion


#pragma region Value

public:
	// 数值类型值（求值器产出基础值，永不改写）
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	float NumericValue = 0.0f;

	// Snapshot 已求值守卫（仅在 bIsSnapshot == true 时使用）
	bool bHasEvaluated = false;

#pragma endregion


#pragma region StateModifier

public:
	// StateModifier 实例列表
	UPROPERTY()
	TArray<struct FStateParamNumericModifierInstance> ModifierInstances;

	void AssignModifier(const struct FStateParamNumericModifierInstance& Instance);

	void RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle);

	float GetModifiedValue(class UTcsSkillEntry* SkillEntry, AActor* Instigator) const;

#pragma endregion


#pragma region Functions

public:
	bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
	float GetValue() const { return NumericValue; }

#pragma endregion
};


/**
 * Bool 类型运行时 StateParam 实例。
 *
 * 持有 Bool 专用 Evaluator CDO 和 Snapshot 策略，通过 GetValue() 返回当前布尔值。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsBoolStateParamInstance
{
	GENERATED_BODY()

#pragma region Identity

public:
	// 参数标识（GameplayTag）
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayTag ParamTag;

	// 是否为快照参数
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	bool bIsSnapshot = true;

#pragma endregion


#pragma region Evaluator

public:
	// 参数数据（FInstancedStruct，从 Definition 复制）
	UPROPERTY()
	FInstancedStruct ParamData;

	// 求值器类型
	UPROPERTY()
	TSubclassOf<UTcsStateBoolParamEvaluator> BoolEvaluatorClass;

	// 缓存求值器 CDO
	UPROPERTY()
	TObjectPtr<UTcsStateBoolParamEvaluator> CachedEvaluator;

#pragma endregion


#pragma region Value

public:
	// 布尔类型值
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	bool BoolValue = false;

	// Snapshot 已求值守卫
	bool bHasEvaluated = false;

#pragma endregion


#pragma region SkillModifier

public:
	UPROPERTY()
	TArray<struct FStateParamBoolModifierInstance> ModifierInstances;

	void AssignModifier(const struct FStateParamBoolModifierInstance& Instance);

	void RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle);

	bool GetModifiedValue(class UTcsSkillEntry* SkillEntry, AActor* Instigator) const;

#pragma endregion


#pragma region Functions

public:
	bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
	bool GetValue() const { return BoolValue; }

#pragma endregion
};


/**
 * Vector 类型运行时 StateParam 实例。
 *
 * 持有 Vector 专用 Evaluator CDO 和 Snapshot 策略，通过 GetValue() 返回当前向量值。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsVectorStateParamInstance
{
	GENERATED_BODY()

#pragma region Identity

public:
	// 参数标识（GameplayTag）
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayTag ParamTag;

	// 是否为快照参数
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	bool bIsSnapshot = true;

#pragma endregion


#pragma region Evaluator

public:
	// 参数数据（FInstancedStruct，从 Definition 复制）
	UPROPERTY()
	FInstancedStruct ParamData;

	// 求值器类型
	UPROPERTY()
	TSubclassOf<UTcsStateVectorParamEvaluator> VectorEvaluatorClass;

	// 缓存求值器 CDO
	UPROPERTY()
	TObjectPtr<UTcsStateVectorParamEvaluator> CachedEvaluator;

#pragma endregion


#pragma region Value

public:
	// 向量类型值
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	FVector VectorValue = FVector::ZeroVector;

	// Snapshot 已求值守卫
	bool bHasEvaluated = false;

#pragma endregion


#pragma region SkillModifier

public:
	UPROPERTY()
	TArray<struct FStateParamVectorModifierInstance> ModifierInstances;

	void AssignModifier(const struct FStateParamVectorModifierInstance& Instance);

	void RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle);

	FVector GetModifiedValue(class UTcsSkillEntry* SkillEntry, AActor* Instigator) const;

#pragma endregion


#pragma region Functions

public:
	bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
	FVector GetValue() const { return VectorValue; }

#pragma endregion
};
