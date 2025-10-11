// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TcsCombatSystemSettings.generated.h"


UCLASS(Config = TcsCombatSystemSettings, DefaultConfig)
class TIREFLYCOMBATSYSTEM_API UTcsCombatSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

#pragma region DeveloperSettings

public:
	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const override { return FName("Project"); }

	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return FName("Game"); }

	/** The unique name for your section of settings, uses the class's FName. */
	virtual FName GetSectionName() const override { return FName("Tcs Combat System"); }

#if WITH_EDITOR

protected:
	/** Gets the section text, uses the classes DisplayName by default. */
	virtual FText GetSectionText() const override { return FText::FromString("Tcs Combat System"); }

	/** Gets the description for the section, uses the classes ToolTip by default. */
	virtual FText GetSectionDescription() const override { return FText::FromString("Developer settings of gameplay ability system"); };
#endif

#pragma endregion


#pragma region DataTable

public:
	// 属性定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable")
	TSoftObjectPtr<UDataTable> AttributeDefTable;

	// 属性修改器定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable")
	TSoftObjectPtr<UDataTable> AttributeModifierDefTable;

	// 状态槽配置数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable", 
		meta = (ToolTip = "状态槽配置数据表，定义各个槽位的激活模式与映射", 
			// 使用当前模块中定义的行结构，而非历史名（已由 CoreRedirect 兼容）
			RequiredAssetDataTags = "RowStructure=/Script/TireflyCombatSystem.TcsStateSlotDefinition"))
	TSoftObjectPtr<UDataTable> SlotConfigurationTable;

	// 状态定义数据表（用于从数据表读取 FTcsStateDefinition）
	UPROPERTY(Config, EditAnywhere, Category = "DataTable",
		meta = (ToolTip = "状态定义数据表：行结构应为 FTcsStateDefinition"))
	TSoftObjectPtr<UDataTable> StateDefTable;

#pragma endregion
};
