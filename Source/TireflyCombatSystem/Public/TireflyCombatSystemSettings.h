// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TireflyCombatSystemSettings.generated.h"


UCLASS(Config = TireflyCombatSystemSettings, DefaultConfig)
class TIREFLYCOMBATSYSTEM_API UTireflyCombatSystemSettings : public UDeveloperSettings
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

#pragma endregion
};
