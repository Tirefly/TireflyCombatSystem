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
	// 预加载所有 State 定义（启动时全部加载）
	PreloadAll		UMETA(DisplayName = "Preload All"),

	// 按需加载 State 定义（首次使用时加载）
	OnDemand		UMETA(DisplayName = "On Demand"),

	// 混合策略：预加载常用 State，其他按需加载
	Hybrid			UMETA(DisplayName = "Hybrid (Preload Common)"),
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

	/** 覆写 PostInitProperties 以在编辑器启动时触发扫描 */
	virtual void PostInitProperties() override;

	/**
	 * 扫描并缓存所有定义资产
	 * 从 AssetManager 配置中读取路径，使用 Asset Registry 扫描
	 */
	void ScanAndCacheDefinitions();

protected:
	/**
	 * 扫描属性定义资产
	 */
	void ScanAttributeDefinitions(const TArray<struct FAssetData>& AssetDataList);

	/**
	 * 扫描状态定义资产
	 */
	void ScanStateDefinitions(const TArray<struct FAssetData>& AssetDataList);

	/**
	 * 扫描状态槽定义资产
	 */
	void ScanStateSlotDefinitions(const TArray<struct FAssetData>& AssetDataList);

	/**
	 * 扫描属性修改器定义资产
	 */
	void ScanAttributeModifierDefinitions(const TArray<struct FAssetData>& AssetDataList);

	/**
	 * 注册 Asset Registry 回调
	 */
	void RegisterAssetRegistryCallbacks();

	/**
	 * 资产添加回调
	 */
	void OnAssetAdded(const struct FAssetData& AssetData);

	/**
	 * 资产删除回调
	 */
	void OnAssetRemoved(const struct FAssetData& AssetData);

	/**
	 * 资产重命名回调
	 */
	void OnAssetRenamed(const struct FAssetData& AssetData, const FString& OldObjectPath);

#endif

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
			AllowedClasses = "/Script/TireflyCombatSystem.TcsStateDefinitionAsset"))
	TArray<TSoftObjectPtr<UTcsStateDefinitionAsset>> CommonStateDefinitions;

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

	// 验证 AssetManager 配置
	// 注意：路径配置已移至 DefaultGame.ini 的 AssetManager 设置中

	return Result;
}
#endif
