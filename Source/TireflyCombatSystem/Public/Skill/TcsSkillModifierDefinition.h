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
class UTcsStateParamBoolModifierExecution;
class UTcsStateParamVectorModifierExecution;


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

	/** Numeric 参数求值策略 CDO。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction",
		Meta = (EditCondition = "TargetParamTag.IsValid() && TargetParamType == ETcsStateParameterType::SPT_Numeric", EditConditionHides))
	TSubclassOf<UTcsStateParamNumericModifierExecution> NumericEvaluatorClass;

	/** Bool 参数求值策略 CDO。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction",
		Meta = (EditCondition = "TargetParamTag.IsValid() && TargetParamType == ETcsStateParameterType::SPT_Bool", EditConditionHides))
	TSubclassOf<UTcsStateParamBoolModifierExecution> BoolEvaluatorClass;

	/** Vector 参数求值策略 CDO。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction",
		Meta = (EditCondition = "TargetParamTag.IsValid() && TargetParamType == ETcsStateParameterType::SPT_Vector", EditConditionHides))
	TSubclassOf<UTcsStateParamVectorModifierExecution> VectorEvaluatorClass;

	/** 求值策略入参。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value Correction",
		Meta = (EditCondition = "(TargetParamType == ETcsStateParameterType::SPT_Numeric && NumericEvaluatorClass != nullptr) || (TargetParamType == ETcsStateParameterType::SPT_Bool && BoolEvaluatorClass != nullptr) || (TargetParamType == ETcsStateParameterType::SPT_Vector && VectorEvaluatorClass != nullptr)", EditConditionHides))
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


public:
	/** 构造默认 SkillModifier evaluator。 */
	UTcsSkillModifierDefinition();

	/**
	 * 获取当前目标参数类型实际生效的 evaluator 类。
	 *
	 * @return 与当前 `TargetParamType` 匹配的 evaluator 类；若未配置则返回 nullptr。
	 */
	UClass* ResolveActiveEvaluatorClass() const;


#if WITH_EDITOR
	/**
	 * 编辑器属性变更后的默认值归一化。
	 *
	 * @param PropertyChangedEvent 变更事件。
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * 编辑器数据有效性检查。
	 *
	 * @param Context 验证上下文。
	 * @return 数据验证结果。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * 为 SkillModifierDefinition 的 typed evaluator 字段补齐默认 concrete 类。
 *
 * @param SkillModifierDefinition 待归一化的 SkillModifier 定义资产。
 */
TIREFLYCOMBATSYSTEM_API void NormalizeSkillModifierStrategyDefaults(UTcsSkillModifierDefinition& SkillModifierDefinition);

/**
 * 校验 SkillModifierDefinition 当前目标参数类型对应的 evaluator 选择是否有效。
 *
 * @param SkillModifierDefinition 待校验的 SkillModifier 定义资产。
 * @param OutError 输出的错误描述。
 * @return 当前 evaluator 选择有效时返回 true。
 */
TIREFLYCOMBATSYSTEM_API bool ValidateSkillModifierStrategySelection(
	const UTcsSkillModifierDefinition& SkillModifierDefinition,
	FString& OutError);
