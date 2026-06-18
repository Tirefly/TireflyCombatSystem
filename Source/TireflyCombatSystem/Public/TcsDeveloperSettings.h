// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "State/TcsStateInstance.h"

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



// State 加载策略
UENUM(BlueprintType)
enum class ETcsStateLoadingStrategy : uint8
{
	// 预加载所有 State 定义（启动时全部加载）
	PreloadAll		UMETA(DisplayName = "Preload All"),

	// 按需加载 State 定义（首次使用时加载）
	OnDemand		UMETA(DisplayName = "On Demand"),

	// 混合策略：预加载常用 State，其他按需加载
	Hybrid			UMETA(DisplayName = "Hybrid (Preload Common)"),
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


#pragma region DataAssetPaths

public:
	/**
	 * State 加载策略
	 * - PreloadAll: 启动时加载所有 State 定义（适合小型项目）
	 * - OnDemand: 完全按需加载，启动时不加载任何 State（适合大型项目）
	 * - Hybrid: 启动时只加载常用 State，其他按需加载（平衡性能和内存）
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "State 定义的加载策略"))
	ETcsStateLoadingStrategy StateLoadingStrategy = ETcsStateLoadingStrategy::PreloadAll;

	/**
	 * 常用 State 定义路径列表（仅在 Hybrid 策略下使用）
	 * 这些路径下的 State 会在启动时预加载，其他 State 按需加载
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "常用 State 定义路径列表，仅在 Hybrid 策略下使用",
			EditCondition = "StateLoadingStrategy == ETcsStateLoadingStrategy::Hybrid",
			EditConditionHides))
	TArray<FDirectoryPath> CommonStateDefinitionPaths;

	/**
	 * 常用 State 定义资产列表（仅在 Hybrid 策略下使用）
	 * 这些 State 会在启动时预加载，优先级高于路径配置
	 * 使用软引用避免编辑器启动时加载所有资产
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "常用 State 定义资产列表，仅在 Hybrid 策略下使用，优先级高于路径配置",
			EditCondition = "StateLoadingStrategy == ETcsStateLoadingStrategy::Hybrid",
			EditConditionHides,
			AllowedClasses = "/Script/TireflyCombatSystem.TcsStateDefinition"))
	TArray<TSoftObjectPtr<UTcsStateDefinition>> CommonStateDefinitions;

#pragma endregion



#pragma region SkillConfig

public:
	/** Level 参数的默认 GameplayTag（StateDefinition 构造函数中读取）。 */
	UPROPERTY(EditAnywhere, Config, Category = "State Param Tag")
	FGameplayTag DefaultLevelParamTag;

	/** 冷却参数的默认 GameplayTag（SkillDef 构造函数中读取）。 */
	UPROPERTY(EditAnywhere, Config, Category = "State Param Tag")
	FGameplayTag DefaultSkillCooldownParamTag;

#pragma endregion



#pragma region InternalCache

protected:
	/**
	 * 内部缓存：属性定义资产映射（Transient，运行时自动填充）
	 * Key: AttributeDefId (FName)
	 * Value: TSoftObjectPtr<UTcsAttributeDefinition>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>> CachedAttributeDefinitions;

	/**
	 * 内部缓存：状态定义资产映射（Transient，运行时自动填充）
	 * Key: StateDefId (FName)
	 * Value: TSoftObjectPtr<UTcsStateDefinition>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsStateDefinition>> CachedStateDefinitions;

	/**
	 * 内部缓存：状态槽定义资产映射（Transient，运行时自动填充）
	 * Key: StateSlotDefId (FName)
	 * Value: TSoftObjectPtr<UTcsStateSlotDefinition>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>> CachedStateSlotDefinitions;

	/**
	 * 内部缓存：属性修改器定义资产映射（Transient，运行时自动填充）
	 * Key: AttributeModifierDefId (FName)
	 * Value: TSoftObjectPtr<UTcsAttributeModifierDefinition>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>> CachedAttributeModifierDefinitions;

	/**
	 * 内部缓存：技能修改器定义资产映射（Transient，运行时自动填充）
	 * Key: ModifierId (FName)
	 * Value: TSoftObjectPtr<UTcsSkillModifierDefinition>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsSkillModifierDefinition>> CachedSkillModifierDefinitions;

public:
	/**
	 * 获取缓存的属性定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>>& GetCachedAttributeDefinitions() const
	{
		return CachedAttributeDefinitions;
	}

	/**
	 * 获取缓存的状态定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsStateDefinition>>& GetCachedStateDefinitions() const
	{
		return CachedStateDefinitions;
	}

	/**
	 * 获取缓存的状态槽定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>>& GetCachedStateSlotDefinitions() const
	{
		return CachedStateSlotDefinitions;
	}

	/**
	 * 获取缓存的属性修改器定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>>& GetCachedAttributeModifierDefinitions() const
	{
		return CachedAttributeModifierDefinitions;
	}

	/**
	 * 获取缓存的技能修改器定义资产映射。
	 */
	const TMap<FName, TSoftObjectPtr<UTcsSkillModifierDefinition>>& GetCachedSkillModifierDefinitions() const
	{
		return CachedSkillModifierDefinitions;
	}

	/**
	 * 设置缓存的属性定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedAttributeDefinitions(const TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>>& InCache)
	{
		CachedAttributeDefinitions = InCache;
	}

	/**
	 * 设置缓存的状态定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedStateDefinitions(const TMap<FName, TSoftObjectPtr<UTcsStateDefinition>>& InCache)
	{
		CachedStateDefinitions = InCache;
	}

	/**
	 * 设置缓存的状态槽定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedStateSlotDefinitions(const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>>& InCache)
	{
		CachedStateSlotDefinitions = InCache;
	}

	/**
	 * 设置缓存的属性修改器定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedAttributeModifierDefinitions(const TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>>& InCache)
	{
		CachedAttributeModifierDefinitions = InCache;
	}

	/**
	 * 设置缓存的技能修改器定义资产映射（由 Subsystem 调用）。
	 */
	void SetCachedSkillModifierDefinitions(const TMap<FName, TSoftObjectPtr<UTcsSkillModifierDefinition>>& InCache)
	{
		CachedSkillModifierDefinitions = InCache;
	}

#pragma endregion
};
