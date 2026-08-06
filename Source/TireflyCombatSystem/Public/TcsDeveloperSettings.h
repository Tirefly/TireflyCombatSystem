// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "State/TcsStateInstance.h"
#include "Attribute/TcsAttributeModifierCompatibility.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsDeveloperSettings.generated.h"



// 前向声明
class UTcsAttributeDefinition;
class UTcsStateDefinition;
class UTcsStateSlotDefinition;
class UTcsAttributeModifierDefinition;
class UTcsSkillModifierDefinition;
class UDataTable;
class UPrimaryDataAsset;



/**
 * DefinitionAsset 运行时加载策略。
 */
UENUM(BlueprintType)
enum class ETcsDefinitionLoadingStrategy : uint8
{
	// 启动时预加载该类型下的全部 DefinitionAsset
	PreloadAll = 0		UMETA(DisplayName = "全部预加载", ToolTip = "启动时预加载该类型下的全部 DefinitionAsset"),

	// 启动时只预加载显式指定的 DefinitionAsset，其余运行时优先按需异步解析
	PreloadSelected = 1	UMETA(DisplayName = "只预加载特定资产", ToolTip = "启动时只预加载显式指定的 DefinitionAsset，其余运行时优先按需异步解析"),

	// 启动时不预加载，全部在运行时首次访问时优先按需异步解析
	OnDemand = 2		UMETA(DisplayName = "完全不预加载", ToolTip = "启动时不预加载，全部在运行时首次访问时优先按需异步解析"),
};



/**
 * 单类 DefinitionAsset 的运行时加载配置。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsDefinitionLoadingConfig
{
	GENERATED_BODY()

public:
	/** 当前 DefinitionAsset 类型采用的运行时加载策略。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading")
	ETcsDefinitionLoadingStrategy LoadingStrategy = ETcsDefinitionLoadingStrategy::PreloadAll;

	/** 仅在 PreloadSelected 策略下生效的预加载白名单。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading",
		Meta = (EditCondition = "LoadingStrategy == ETcsDefinitionLoadingStrategy::PreloadSelected", EditConditionHides))
	TArray<TSoftObjectPtr<UPrimaryDataAsset>> SpecificAssets;
};



/**
 * DataTable ↔ DefAsset 单条同步配置。
 *
 * 约束：一个受管目录严格对应一张显式绑定的 DataTable。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsDataTableSyncConfig
{
	GENERATED_BODY()

public:
	/** 该配置受管的 DefAsset 类型。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync")
	TSubclassOf<UPrimaryDataAsset> DefAssetClass;

	/** 该配置受管的 DefAsset 目录。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync", Meta = (ContentDir))
	FDirectoryPath ManagedDefAssetDirectory;

	/** 与该目录显式绑定的目标 DataTable。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync")
	TSoftObjectPtr<UDataTable> TargetDataTable;

	/** 保存 DefAsset 时是否回写 DataTable。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync")
	bool bSyncDefAssetToDataTable = true;

	/** 保存 DataTable 时是否删除孤立 DefAsset。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync")
	bool bAllowDeleteOrphanDefAssets = true;
};



/**
 * 单条同步配置的校验结果。
 */
struct TIREFLYCOMBATSYSTEM_API FTcsDataTableSyncConfigValidationResult
{
	/** 当前配置是否通过校验。 */
	bool bValid = false;

	/** 阻止同步执行的错误列表。 */
	TArray<FText> Errors;

	/** 不阻止同步执行的警告列表。 */
	TArray<FText> Warnings;
};



UCLASS(Config = TireflyCombatSystemSettings, DefaultConfig)
class TIREFLYCOMBATSYSTEM_API UTcsDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

#pragma region DeveloperSettings

public:
	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const override { return FName("Project"); }

	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return FName("Game"); }

	/** The unique name for your section of settings, uses the class's FName. */
	virtual FName GetSectionName() const override { return FName("Tirefly Combat System"); }

#if WITH_EDITOR

protected:
	/** Gets the section text, uses the classes DisplayName by default. */
	virtual FText GetSectionText() const override { return FText::FromString("Tirefly Combat System"); }

	/** Gets the description for the section, uses the classes ToolTip by default. */
	virtual FText GetSectionDescription() const override { return FText::FromString("Developer settings of gameplay ability system"); };

public:
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

#endif

#pragma endregion


#pragma region EditorValidation

public:
	/**
	 * 编辑器勘误忽略列表
	 * 列表中的 DefAsset 类型不会参与 AssetManagerSettings 覆盖检查
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor Validation",
		meta = (ToolTip = "DefAsset 类型勘误忽略列表"))
	TArray<TSubclassOf<UPrimaryDataAsset>> IgnoredDefinitionAssetTypes;

	/** DataTable ↔ DefAsset 自动同步总开关。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync")
	bool bEnableDataTableAutoSync = false;

	/** 每种 DefAsset 类型的同步配置列表。 */
	UPROPERTY(Config, EditAnywhere, Category = "DataTable Sync",
		meta = (EditCondition = "bEnableDataTableAutoSync", EditConditionHides))
	TArray<FTcsDataTableSyncConfig> DataTableSyncConfigs;

	/**
	 * 校验单条 DataTable 同步配置是否合法。
	 *
	 * @param Config 待校验的同步配置。
	 * @return 单条配置的校验结果。
	 */
	FTcsDataTableSyncConfigValidationResult ValidateConfig(const FTcsDataTableSyncConfig& Config) const;

	/**
	 * 校验全部 DataTable 同步配置是否合法。
	 *
	 * @param OutErrors 输出的错误列表。
	 * @param OutWarnings 输出的警告列表。
	 * @return 全部配置都合法时返回 true。
	 */
	bool ValidateAllConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#pragma endregion


#pragma region DefinitionLoading

public:
	/** AttributeDefinition 的运行时加载配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading|Attribute")
	FTcsDefinitionLoadingConfig AttributeDefinitionLoading;

	/** AttributeModifierDefinition 的运行时加载配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading|Attribute")
	FTcsDefinitionLoadingConfig AttributeModifierDefinitionLoading;

	/** BuffDefinition 的运行时加载配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading|StateLike")
	FTcsDefinitionLoadingConfig BuffDefinitionLoading;

	/** SkillDefinition 的运行时加载配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading|StateLike")
	FTcsDefinitionLoadingConfig SkillDefinitionLoading;

	/** SkillModifierDefinition 的运行时加载配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading|Skill")
	FTcsDefinitionLoadingConfig SkillModifierDefinitionLoading;

	/** StateSlotDefinition 的运行时加载配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Definition Loading|State")
	FTcsDefinitionLoadingConfig StateSlotDefinitionLoading;

#pragma endregion


#pragma region SkillConfig

public:
	/** StateInstance 等级（Level）参数的默认 GameplayTag（StateDefinition 构造函数中读取）。 */
	UPROPERTY(EditAnywhere, Config, Category = "State Param Tag")
	FGameplayTag DefaultStateInstanceLevelParamTag;

	/** 冷却参数的默认 GameplayTag（SkillDef 构造函数中读取）。 */
	UPROPERTY(EditAnywhere, Config, Category = "State Param Tag")
	FGameplayTag DefaultSkillCooldownParamTag;

#pragma endregion


// AttributeModifier Operator / Merger 兼容规则
#pragma region AttributeModifierCompatibility

public:
	/**
	 * 显式 Operator / Merger 兼容规则列表。
	 * Forbidden 优先于 Allowed；未命中规则时回退到内建默认矩阵。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Attribute Modifier|Compatibility")
	TArray<FTcsAttributeOperatorMergerRule> AttributeOperatorMergerRules;

	/**
	 * 是否允许 MultiplyAdditive 与 UseAdditiveSum 组合。
	 * 允许时按 delta 语义聚合 Operand。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Attribute Modifier|Compatibility")
	bool bAllowMultiplyAdditiveWithUseAdditiveSum = false;

	/**
	 * 多 Operation + 内建选择/聚合 Merger 的 Data Validation 是否报 Error。
	 * false 时降级为 Warning。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Attribute Modifier|Compatibility")
	bool bMultiOperationSelectionMergerIsError = true;

#pragma endregion
};
