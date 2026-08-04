// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateSlotDefinition.h"

#include "TcsDefDataTableRows.generated.h"

class UPrimaryDataAsset;



/**
 * Attribute Definition 对应的 DataTable 行结构。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeDefRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	FTcsAttributeDefRow();

	/** 属性类别。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FString AttributeCategory;

	/** 属性语义标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FGameplayTag AttributeTag;

	/** 属性数值范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FTcsAttributeRange AttributeRange;

	/** 属性 Clamp 策略类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	TSubclassOf<class UTcsAttributeClampStrategy> ClampStrategyClass;

	/** 属性 Clamp 策略配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FInstancedStruct ClampStrategyConfig;

	/** 是否在 UI 中显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	bool bShowInUI = true;

	/** 属性显示名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute",
		Meta = (EditCondition = "bShowInUI", EditConditionHides))
	FText AttributeName;

	/** 属性显示描述。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute",
		Meta = (EditCondition = "bShowInUI", EditConditionHides))
	FText AttributeDescription;

	/** 属性图标资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute",
		Meta = (EditCondition = "bShowInUI", EditConditionHides))
	TSoftObjectPtr<UTexture2D> Icon;

	/** 是否按小数显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute",
		Meta = (EditCondition = "bShowInUI", EditConditionHides))
	bool bAsDecimal = false;

	/** 是否按百分比显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute",
		Meta = (EditCondition = "bShowInUI", EditConditionHides))
	bool bAsPercentage = false;
};



/**
 * Attribute Modifier Definition 对应的 DataTable 行结构。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierDefRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	FTcsAttributeModifierDefRow();

	/** 修改器名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Modifier")
	FName ModifierName = NAME_None;

	/** 修改器标签集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Modifier")
	FGameplayTagContainer Tags;

	/** 执行优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Modifier")
	int32 Priority = 0;

	/** 修改器合并器类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Modifier")
	TSubclassOf<class UTcsAttributeModifierMerger> MergerType;

	/** 以稳定 OperationId 为 Key 的属性修改操作配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Modifier")
	TMap<FName, FTcsAttributeOperationSpec> Operations;
};



/**
 * Buff Definition 对应的 DataTable 行结构。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsBuffDefRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	FTcsBuffDefRow();

	/** 状态语义标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTag StateTag;

	/** 状态槽类型标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTag StateSlotType;

	/** 状态优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	int32 Priority = 0;

	/** 状态类别标签集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTagContainer CategoryTags;

	/** 状态功能标签集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTagContainer FunctionTags;

	/** 状态树引用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FStateTreeReference StateTreeRef;

	/** StateTree Tick 策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	ETcsStateTreeTickPolicy TickPolicy = ETcsStateTreeTickPolicy::ManualOnly;

	/** 激活条件配置列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	TArray<FTcsStateConditionConfig> ActiveConditions;

	/** 状态参数表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	TMap<FGameplayTag, FTcsStateParameter> Parameters;

	/** Level 参数标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTag LevelParamTag;

	/** 持续时间类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	TEnumAsByte<ETcsBuffDurationType> DurationType = SDT_None;

	/** 持续时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff",
		Meta = (EditConditionHides, EditCondition = "DurationType == ETcsBuffDurationType::SDT_Duration"))
	float Duration = 0.f;

	/** 周期触发间隔。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff",
		Meta = (EditConditionHides, EditCondition = "DurationType != ETcsBuffDurationType::SDT_None"))
	float Period = 0.f;

	/** 最大叠层数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	int32 MaxStackCount = 1;

	/** Buff 合并器类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	TSubclassOf<class UTcsBuffMerger> MergerType;

	/** 叠层上涨后的增量反应配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff",
		Meta = (EditConditionHides, EditCondition = "MaxStackCount > 1"))
	FTcsBuffOnStackIncreasePolicy OnStackIncrease;

	/** 持续时间耗尽后的增量反应配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff",
		Meta = (EditConditionHides, EditCondition = "MaxStackCount > 1 && DurationType == ETcsBuffDurationType::SDT_Duration"))
	FTcsBuffOnDurationExpiredPolicy OnDurationExpired;

	/** Buff 运行时实例类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	TSubclassOf<class UTcsBuffInstance> BuffInstanceClass;
};



/**
 * Skill Definition 对应的 DataTable 行结构。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillDefRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	/** 状态语义标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTag StateTag;

	/** 状态槽类型标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTag StateSlotType;

	/** 状态优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	int32 Priority = 0;

	/** 状态类别标签集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTagContainer CategoryTags;

	/** 状态功能标签集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTagContainer FunctionTags;

	/** 状态树引用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FStateTreeReference StateTreeRef;

	/** StateTree Tick 策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	ETcsStateTreeTickPolicy TickPolicy = ETcsStateTreeTickPolicy::ManualOnly;

	/** 激活条件配置列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	TArray<FTcsStateConditionConfig> ActiveConditions;

	/** 状态参数表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	TMap<FGameplayTag, FTcsStateParameter> Parameters;

	/** Level 参数标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGameplayTag LevelParamTag;

	/** Skill 运行时实例类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<class UTcsSkillInstance> SkillInstanceClass;

	/** Skill learned-entry 类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<class UTcsSkillEntry> SkillEntryClass;

	/** 冷却参数标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FGameplayTag CooldownParamTag;

	/** 冷却参数配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill",
		Meta = (EditCondition = "CooldownParamTag.IsValid()", EditConditionHides))
	FTcsStateParameter CooldownParam;
};



/**
 * Skill Modifier Definition 对应的 DataTable 行结构。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierDefRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	FTcsSkillModifierDefRow();

	/** 目标技能选取策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier")
	TSubclassOf<UTcsSkillEntrySelector> EntrySelectorClass;

	/** 目标技能选取策略配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier",
		Meta = (EditCondition = "EntrySelectorClass != nullptr", EditConditionHides))
	FInstancedStruct EntrySelectorConfig;

	/** 目标参数标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier")
	FGameplayTag TargetParamTag;

	/** 目标参数类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier")
	ETcsStateParameterType TargetParamType = ETcsStateParameterType::SPT_Numeric;

	/** Numeric 参数求值策略类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier",
		Meta = (EditCondition = "TargetParamTag.IsValid() && TargetParamType == ETcsStateParameterType::SPT_Numeric", EditConditionHides))
	TSubclassOf<UTcsStateParamNumericModifierExecution> NumericEvaluatorClass;

	/** Bool 参数求值策略类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier",
		Meta = (EditCondition = "TargetParamTag.IsValid() && TargetParamType == ETcsStateParameterType::SPT_Bool", EditConditionHides))
	TSubclassOf<UTcsStateParamBoolModifierExecution> BoolEvaluatorClass;

	/** Vector 参数求值策略类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier",
		Meta = (EditCondition = "TargetParamTag.IsValid() && TargetParamType == ETcsStateParameterType::SPT_Vector", EditConditionHides))
	TSubclassOf<UTcsStateParamVectorModifierExecution> VectorEvaluatorClass;

	/** 参数求值策略配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier",
		Meta = (EditCondition = "(TargetParamType == ETcsStateParameterType::SPT_Numeric && NumericEvaluatorClass != nullptr) || (TargetParamType == ETcsStateParameterType::SPT_Bool && BoolEvaluatorClass != nullptr) || (TargetParamType == ETcsStateParameterType::SPT_Vector && VectorEvaluatorClass != nullptr)", EditConditionHides))
	FInstancedStruct EvaluatorConfig;

	/** 执行优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier")
	int32 Priority = 0;

	/** 同 SkillModifierDefId 的合并策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Modifier")
	ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;
};



/**
 * State Slot Definition 对应的 DataTable 行结构。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateSlotDefRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	FTcsStateSlotDefRow();

	/** 槽位标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	FGameplayTag SlotTag;

	/** 对应的 StateTree 状态名；运行时 StateSlotMapping 要求该值非空。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	FName StateTreeStateName = NAME_None;

	/** 状态激活模式。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	ETcsStateSlotActivationMode ActivationMode = ETcsStateSlotActivationMode::SSAM_PriorityOnly;

	/** Gate 关闭时的处理策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	ETcsStateSlotGateClosePolicy GateCloseBehavior = ETcsStateSlotGateClosePolicy::SSGCP_Pause;

	/** 高优先级状态进入时的抢占策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	ETcsStatePreemptionPolicy PreemptionPolicy = ETcsStatePreemptionPolicy::SPP_PauseLowerPriority;

	/** 同优先级状态排序策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot",
		Meta = (EditCondition = "ActivationMode == ETcsStateSlotActivationMode::SSAM_PriorityOnly", EditConditionHides = true))
	TSubclassOf<class UTcsStateSamePriorityPolicy> SamePriorityPolicy;
};



/**
 * 解析指定 DefAsset 类型期望使用的 RowStruct。
 *
	 * @param DefAssetClass 待解析的 DefAsset 类型。
	 * @return 成功时返回期望 RowStruct；否则返回 nullptr。
 */
TIREFLYCOMBATSYSTEM_API UScriptStruct* ResolveExpectedDefDataTableRowStruct(const UClass* DefAssetClass);

/**
 * 从受支持的 DefAsset 中提取权威 DefId。
 *
	 * @param DefAsset 待读取的 DefAsset。
	 * @param OutDefId 输出的 DefId。
	 * @return 成功提取时返回 true。
 */
TIREFLYCOMBATSYSTEM_API bool TryGetDefAssetSyncId(const UPrimaryDataAsset* DefAsset, FName& OutDefId);

/**
 * 向受支持的 DefAsset 写入权威 DefId。
 *
	 * @param DefAsset 待写入的 DefAsset。
	 * @param DefId 要设置的 DefId。
	 * @return 成功写入时返回 true。
 */
TIREFLYCOMBATSYSTEM_API bool TrySetDefAssetSyncId(UPrimaryDataAsset* DefAsset, FName DefId);

/**
 * 将 DefAsset 显式搬运为对应的 DataTable 行数据。
 *
	 * @param DefAsset 待导出的 DefAsset。
	 * @param OutRowName 输出的 DataTable RowName。
	 * @param OutRowData 输出的行结构数据。
	 * @return 成功导出时返回 true。
 */
TIREFLYCOMBATSYSTEM_API bool TryBuildDefAssetDataTableRow(const UPrimaryDataAsset* DefAsset, FName& OutRowName, FInstancedStruct& OutRowData);

/**
 * 将 DataTable 行数据显式回写到 DefAsset。
 *
	 * @param RowName 权威 RowName，同时也是 DefId。
	 * @param RowData 待回写的行结构数据。
	 * @param DefAsset 目标 DefAsset。
	 * @return 成功回写时返回 true。
 */
TIREFLYCOMBATSYSTEM_API bool TryApplyDefAssetDataTableRow(FName RowName, const FInstancedStruct& RowData, UPrimaryDataAsset* DefAsset);
