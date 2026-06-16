// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Skill/TcsSkillModifierInstance.h"
#include "State/TcsStateParamInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsSkillModifierDefinition.generated.h"



class UTcsSkillEntrySelector;
class UTcsStateParamNumericModifierExecution;


/**
 * SkillModifier 定义资产。
 *
 * 一个 Def 只修改一个 StateParam。如需修改多个参数，创建多个 DefAsset。
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

#pragma region PrimaryAsset

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


#pragma region Identity

public:
	/** 唯一标识符（用于互斥判定和跨系统引用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName ModifierId;

#pragma endregion


#pragma region Target

public:
	/** 目标技能选取策略 CDO。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target",
		Meta = (DisplayName = "Entry Selector"))
	TSubclassOf<UTcsSkillEntrySelector> EntrySelectorClass;

	/** Entry 选取策略入参。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target",
		Meta = (EditCondition = "EntrySelectorClass != nullptr", EditConditionHides))
	FInstancedStruct EntrySelectorConfig;

#pragma endregion


#pragma region ValueCorrection

public:
	/** 修改的目标 StateParam Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction")
	FGameplayTag TargetParamTag;

	/** 参数类型（用于筛选 EvaluatorClass 的可用基类）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction")
	ETcsStateParameterType TargetParamType = ETcsStateParameterType::SPT_Numeric;

	/** 求值策略 CDO。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction",
		Meta = (EditCondition = "TargetParamTag.IsValid()", EditConditionHides))
	TSubclassOf<UTcsStateParamNumericModifierExecution> EvaluatorClass;

	/** 求值策略入参。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction",
		Meta = (EditCondition = "EvaluatorClass != nullptr", EditConditionHides))
	FInstancedStruct EvaluatorConfig;

#pragma endregion


#pragma region Merge

public:
	/** 执行优先级（从高到低）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merge")
	int32 Priority = 0;

	/** 同 ModifierId 的合并策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merge")
	ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;

#pragma endregion
};
