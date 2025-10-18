// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "State/TcsState.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsDeveloperSettings.generated.h"



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


#pragma region DataTable

public:
	// 属性定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable",
		meta = (ToolTip = "属性定义数据表：行结构应为 FTcsAttributeDefinition",
			RequiredAssetDataTags = "RowStructure=/Script/TireflyCombatSystem.TcsAttributeDefinition"))
	TSoftObjectPtr<UDataTable> AttributeDefTable;

	// 属性修改器定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable",
		meta = (ToolTip = "属性修改器定义数据表：行结构应为 FTcsAttributeModifierDefinition",
			RequiredAssetDataTags = "RowStructure=/Script/TireflyCombatSystem.TcsAttributeModifierDefinition"))
	TSoftObjectPtr<UDataTable> AttributeModifierDefTable;

	// 技能修改器定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable",
		meta = (ToolTip = "技能修改器定义数据表：行结构应为 FTcsSkillModifierDefinition",
			RequiredAssetDataTags = "RowStructure=/Script/TireflyCombatSystem.TcsSkillModifierDefinition"))
	TSoftObjectPtr<UDataTable> SkillModifierDefTable;

	// 状态定义数据表（用于从数据表读取 FTcsStateDefinition）
	UPROPERTY(Config, EditAnywhere, Category = "DataTable",
		meta = (ToolTip = "状态定义数据表：行结构应为 FTcsStateDefinition"))
	TSoftObjectPtr<UDataTable> StateDefTable;

	// 状态槽定义数据表
	UPROPERTY(Config, EditAnywhere, Category = "DataTable", 
		meta = (ToolTip = "状态槽定义数据表，定义各个槽位的激活模式与映射", 
			RequiredAssetDataTags = "RowStructure=/Script/TireflyCombatSystem.TcsStateSlotDefinition"))
	TSoftObjectPtr<UDataTable> StateSlotDefTable;	

#pragma endregion
};

#if WITH_EDITOR
inline EDataValidationResult UTcsDeveloperSettings::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	EDataValidationResult Result = SuperResult;
	bool bHasError = false;

	if (StateDefTable.IsValid())
	{
		const UDataTable* StateTable = StateDefTable.LoadSynchronous();
		if (!StateTable)
		{
			Context.AddError(NSLOCTEXT("TcsCombatSystemSettings", "StateDefTableLoadFailed", "Failed to load StateDefTable during validation."));
			return EDataValidationResult::Invalid;
		}

		const FString ContextString(TEXT("UTcsDeveloperSettings::IsDataValid"));
		for (const FName& RowName : StateTable->GetRowNames())
		{
			const FTcsStateDefinition* Definition = StateTable->FindRow<FTcsStateDefinition>(RowName, ContextString);
			if (!Definition)
			{
				continue;
			}

			if (Definition->StateType == ST_Skill && !Definition->StateSlotType.IsValid())
			{
				Context.AddError(FText::Format(
					NSLOCTEXT("TcsCombatSystemSettings", "MissingSkillSlotTag", "Skill state '{0}' must define StateSlotType for the Stage3 skill-slot pipeline."),
					FText::FromName(RowName)));
				bHasError = true;
			}
		}
	}

	if (bHasError)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
