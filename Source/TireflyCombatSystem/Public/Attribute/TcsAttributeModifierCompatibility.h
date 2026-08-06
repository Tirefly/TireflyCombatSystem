// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModOperation/TcsAttributeModifierOperation.h"
#include "TcsAttributeModifierCompatibility.generated.h"



class UTcsAttributeModifierMerger;
class UTcsAttributeModifierCustomOperator;
class UTcsAttributeModifierDefinition;
class UTcsDeveloperSettings;



/** Operator 与 Merger 的二元兼容判定。 */
UENUM(BlueprintType)
enum class ETcsAttributeOperatorMergerCompatibility : uint8
{
	AOMC_Allowed = 0	UMETA(DisplayName = "Allowed", ToolTip = "The Operator and Merger combination is allowed."),
	AOMC_Forbidden = 1	UMETA(DisplayName = "Forbidden", ToolTip = "The Operator and Merger combination is forbidden."),
};



/** 一条可配置的 Operator / Merger 兼容规则。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeOperatorMergerRule
{
	GENERATED_BODY()

public:
	// 规则作用的 Merger 类；空表示匹配任意 Merger。
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility")
	TSubclassOf<UTcsAttributeModifierMerger> MergerType;

	// 规则作用的内建 Operator。
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility")
	ETcsAttributeModifierOperator Operator = ETcsAttributeModifierOperator::AMO_None;

	// Operator 为 AMO_Custom 时可选匹配的 Custom Operator 类；空表示匹配任意 Custom。
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility",
		Meta = (EditCondition = "Operator == ETcsAttributeModifierOperator::AMO_Custom", EditConditionHides))
	TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass;

	// 该组合的兼容判定。
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility")
	ETcsAttributeOperatorMergerCompatibility Compatibility = ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed;
};



/** AttributeModifier Operator / Merger 兼容查询与默认矩阵。 */
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierCompatibility
{
public:
	/**
	 * 查询 Operator 与 Merger 是否兼容。
	 *
	 * @param MergerType Ongoing Merger 类。
	 * @param Operator 内建 Operator。
	 * @param CustomOperatorClass Custom Operator 类；非 Custom 时可为空。
	 * @param Settings 可选项目设置；为空时只使用内建默认矩阵。
	 * @return Allowed 或 Forbidden。
	 */
	static ETcsAttributeOperatorMergerCompatibility EvaluateOperatorMergerCompatibility(
		TSubclassOf<UTcsAttributeModifierMerger> MergerType,
		ETcsAttributeModifierOperator Operator,
		TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass,
		const UTcsDeveloperSettings* Settings = nullptr);

	/**
	 * 判断 Merger 是否属于内建选择 / 聚合类（非 NoMerge）。
	 *
	 * @param MergerType 待检查的 Merger 类。
	 * @return 选择或聚合 Merger 返回 true。
	 */
	static bool IsSelectionOrAggregationMerger(TSubclassOf<UTcsAttributeModifierMerger> MergerType);

	/**
	 * 校验 Definition 的 Operator / Merger / 多 Operation 兼容性。
	 *
	 * @param ModifierDefinition 待校验 Definition。
	 * @param OutErrors 输出 Error 级问题。
	 * @param OutWarnings 输出 Warning 级问题。
	 * @param Settings 可选项目设置。
	 * @return 无 Error 时返回 true。
	 */
	static bool ValidateModifierDefinitionCompatibility(
		const UTcsAttributeModifierDefinition& ModifierDefinition,
		TArray<FText>& OutErrors,
		TArray<FText>& OutWarnings,
		const UTcsDeveloperSettings* Settings = nullptr);
};
