// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsSourceHandle.h"
#include "TcsSkillModifierInstance.generated.h"



class UTcsStateParamNumericModifierExecution;
class UTcsStateParamBoolModifierExecution;
class UTcsStateParamVectorModifierExecution;


// 合并策略枚举
UENUM(BlueprintType)
enum class ETcsSkillModifierMergePolicy : uint8
{
	Stack		UMETA(DisplayName = "Stack", ToolTip = "同 ModifierId 可叠加"),
	Exclusive	UMETA(DisplayName = "Exclusive", ToolTip = "同 ModifierId 只保留最高 Priority"),
};



// Config 结构体已迁移至 Skill/SkillModExecution/TcsSkillModifierExecution.h



/**
 * Numeric 类型 SkillModifier 运行时实例。
 *
 * 挂在 FTcsNumericStateParamInstance.ModifierInstances 上。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FStateParamNumericModifierInstance
{
	GENERATED_BODY()

#pragma region Identity

public:
	UPROPERTY()
	FName ModifierId;

#pragma endregion


#pragma region Evaluator

public:
	UPROPERTY()
	TObjectPtr<UTcsStateParamNumericModifierExecution> Evaluator;

	UPROPERTY()
	FInstancedStruct Config;

#pragma endregion


#pragma region Merge

public:
	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;

#pragma endregion


#pragma region Lifecycle

public:
	UPROPERTY()
	FTcsSourceHandle SourceHandle;

	UPROPERTY()
	bool bActive = true;

#pragma endregion
};



/**
 * Bool 类型 SkillModifier 运行时实例。
 *
 * 挂在 FTcsBoolStateParamInstance.ModifierInstances 上。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FStateParamBoolModifierInstance
{
	GENERATED_BODY()

#pragma region Identity

public:
	UPROPERTY()
	FName ModifierId;

#pragma endregion


#pragma region Evaluator

public:
	UPROPERTY()
	TObjectPtr<UTcsStateParamBoolModifierExecution> Evaluator;

	UPROPERTY()
	FInstancedStruct Config;

#pragma endregion


#pragma region Merge

public:
	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;

#pragma endregion


#pragma region Lifecycle

public:
	UPROPERTY()
	FTcsSourceHandle SourceHandle;

	UPROPERTY()
	bool bActive = true;

#pragma endregion
};



/**
 * Vector 类型 SkillModifier 运行时实例。
 *
 * 挂在 FTcsVectorStateParamInstance.ModifierInstances 上。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FStateParamVectorModifierInstance
{
	GENERATED_BODY()

#pragma region Identity

public:
	UPROPERTY()
	FName ModifierId;

#pragma endregion


#pragma region Evaluator

public:
	UPROPERTY()
	TObjectPtr<UTcsStateParamVectorModifierExecution> Evaluator;

	UPROPERTY()
	FInstancedStruct Config;

#pragma endregion


#pragma region Merge

public:
	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;

#pragma endregion


#pragma region Lifecycle

public:
	UPROPERTY()
	FTcsSourceHandle SourceHandle;

	UPROPERTY()
	bool bActive = true;

#pragma endregion
};
