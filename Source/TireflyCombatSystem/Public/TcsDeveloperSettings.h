// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "State/TcsState.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsDeveloperSettings.generated.h"


// 前向声明
class UTcsAttributeDefinitionAsset;
class UTcsStateDefinitionAsset;
class UTcsStateSlotDefinitionAsset;
class UTcsAttributeModifierDefinitionAsset;


// State 加载策略
UENUM(BlueprintType)
enum class ETcsStateLoadingStrategy : uint8
{
	// 加载所有 State 定义（启动时全部加载）
	LoadAll			UMETA(DisplayName = "Load All"),

	// 按需加载 State 定义（首次使用时加载）
	LoadOnDemand	UMETA(DisplayName = "Load On Demand"),
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


#pragma region DataAssetPaths

public:
	/**
	 * 属性定义资产路径列表
	 * 系统会自动扫描这些路径下的所有 UTcsAttributeDefinitionAsset
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "属性定义资产路径列表，系统会自动扫描这些路径下的所有 UTcsAttributeDefinitionAsset"))
	TArray<FDirectoryPath> AttributeDefinitionPaths;

	/**
	 * 状态定义资产路径列表
	 * 系统会自动扫描这些路径下的所有 UTcsStateDefinitionAsset
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "状态定义资产路径列表，系统会自动扫描这些路径下的所有 UTcsStateDefinitionAsset"))
	TArray<FDirectoryPath> StateDefinitionPaths;

	/**
	 * 状态槽定义资产路径列表
	 * 系统会自动扫描这些路径下的所有 UTcsStateSlotDefinitionAsset
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "状态槽定义资产路径列表，系统会自动扫描这些路径下的所有 UTcsStateSlotDefinitionAsset"))
	TArray<FDirectoryPath> StateSlotDefinitionPaths;

	/**
	 * 属性修改器定义资产路径列表
	 * 系统会自动扫描这些路径下的所有 UTcsAttributeModifierDefinitionAsset
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "属性修改器定义资产路径列表，系统会自动扫描这些路径下的所有 UTcsAttributeModifierDefinitionAsset"))
	TArray<FDirectoryPath> AttributeModifierDefinitionPaths;

	/**
	 * State 加载策略
	 * LoadAll: 启动时加载所有 State 定义（适合小型项目）
	 * LoadOnDemand: 按需加载 State 定义（适合大型项目）
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DataAsset Paths",
		meta = (ToolTip = "State 加载策略：LoadAll（启动时加载所有）或 LoadOnDemand（按需加载）"))
	ETcsStateLoadingStrategy StateLoadingStrategy = ETcsStateLoadingStrategy::LoadAll;

#pragma endregion


#pragma region InternalCache

protected:
	/**
	 * 内部缓存：属性定义资产映射（Transient，运行时自动填充）
	 * Key: AttributeDefId (FName)
	 * Value: TSoftObjectPtr<UTcsAttributeDefinitionAsset>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsAttributeDefinitionAsset>> CachedAttributeDefinitions;

	/**
	 * 内部缓存：状态定义资产映射（Transient，运行时自动填充）
	 * Key: StateDefId (FName)
	 * Value: TSoftObjectPtr<UTcsStateDefinitionAsset>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsStateDefinitionAsset>> CachedStateDefinitions;

	/**
	 * 内部缓存：状态槽定义资产映射（Transient，运行时自动填充）
	 * Key: StateSlotDefId (FName)
	 * Value: TSoftObjectPtr<UTcsStateSlotDefinitionAsset>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinitionAsset>> CachedStateSlotDefinitions;

	/**
	 * 内部缓存：属性修改器定义资产映射（Transient，运行时自动填充）
	 * Key: AttributeModifierDefId (FName)
	 * Value: TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset>
	 */
	UPROPERTY(Transient)
	TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset>> CachedAttributeModifierDefinitions;

public:
	/**
	 * 获取缓存的属性定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsAttributeDefinitionAsset>>& GetCachedAttributeDefinitions() const
	{
		return CachedAttributeDefinitions;
	}

	/**
	 * 获取缓存的状态定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsStateDefinitionAsset>>& GetCachedStateDefinitions() const
	{
		return CachedStateDefinitions;
	}

	/**
	 * 获取缓存的状态槽定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinitionAsset>>& GetCachedStateSlotDefinitions() const
	{
		return CachedStateSlotDefinitions;
	}

	/**
	 * 获取缓存的属性修改器定义资产映射
	 */
	const TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset>>& GetCachedAttributeModifierDefinitions() const
	{
		return CachedAttributeModifierDefinitions;
	}

	/**
	 * 设置缓存的属性定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedAttributeDefinitions(const TMap<FName, TSoftObjectPtr<UTcsAttributeDefinitionAsset>>& InCache)
	{
		CachedAttributeDefinitions = InCache;
	}

	/**
	 * 设置缓存的状态定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedStateDefinitions(const TMap<FName, TSoftObjectPtr<UTcsStateDefinitionAsset>>& InCache)
	{
		CachedStateDefinitions = InCache;
	}

	/**
	 * 设置缓存的状态槽定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedStateSlotDefinitions(const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinitionAsset>>& InCache)
	{
		CachedStateSlotDefinitions = InCache;
	}

	/**
	 * 设置缓存的属性修改器定义资产映射（由 Subsystem 调用）
	 */
	void SetCachedAttributeModifierDefinitions(const TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset>>& InCache)
	{
		CachedAttributeModifierDefinitions = InCache;
	}

#pragma endregion


#pragma region SkillDataTable

	// 技能修改器定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable",
		meta = (ToolTip = "技能修改器定义数据表：行结构应为 FTcsSkillModifierDefinition",
			RequiredAssetDataTags = "RowStructure=/Script/TireflyCombatSystem.TcsSkillModifierDefinition"))
	TSoftObjectPtr<UDataTable> SkillModifierDefTable;

#pragma endregion
};

#if WITH_EDITOR
inline EDataValidationResult UTcsDeveloperSettings::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	EDataValidationResult Result = SuperResult;

	// 验证路径配置
	if (AttributeDefinitionPaths.Num() == 0)
	{
		Context.AddWarning(NSLOCTEXT("TireflyCombatSystemSettings", "NoAttributeDefPaths", "No AttributeDefinitionPaths configured. Attribute definitions will not be loaded."));
	}

	if (StateDefinitionPaths.Num() == 0)
	{
		Context.AddWarning(NSLOCTEXT("TireflyCombatSystemSettings", "NoStateDefPaths", "No StateDefinitionPaths configured. State definitions will not be loaded."));
	}

	if (StateSlotDefinitionPaths.Num() == 0)
	{
		Context.AddWarning(NSLOCTEXT("TireflyCombatSystemSettings", "NoStateSlotDefPaths", "No StateSlotDefinitionPaths configured. StateSlot definitions will not be loaded."));
	}

	return Result;
}
#endif
