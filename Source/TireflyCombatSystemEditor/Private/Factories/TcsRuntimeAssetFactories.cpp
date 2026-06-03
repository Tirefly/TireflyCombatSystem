// Copyright Tirefly. All Rights Reserved.

#include "Factories/TcsRuntimeAssetFactories.h"

#include "AssetTypeCategories.h"
#include "Skill/TcsSkillEntry.h"
#include "StateTree/Schema/TcsSTSchema_Buff.h"
#include "StateTree/Schema/TcsSTSchema_StateComponent.h"
#include "TireflyCombatSystemEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TcsRuntimeAssetFactories)

#define LOCTEXT_NAMESPACE "TcsRuntimeAssetFactories"

namespace TcsRuntimeAssetFactories
{
	/**
	 * 获取 TCS 资产创建菜单使用的分类位。
	 *
	 * @return TCS 分类位；若模块尚未注册则回退到 Gameplay 分类。
	 */
	uint32 GetTcsMenuCategory()
	{
		const uint32 RegisteredCategory = FTireflyCombatSystemEditorModule::Get().GetRegisteredAssetCategory();
		return RegisteredCategory != 0 ? RegisteredCategory : EAssetTypeCategories::Gameplay;
	}
}

UTcsStateComponentStateTreeFactory::UTcsStateComponentStateTreeFactory()
{
	StateTreeSchemaClass = UTcsSTSchema_StateComponent::StaticClass();
}

bool UTcsStateComponentStateTreeFactory::ConfigureProperties()
{
	StateTreeSchemaClass = UTcsSTSchema_StateComponent::StaticClass();
	return true;
}

bool UTcsStateComponentStateTreeFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsStateComponentStateTreeFactory::GetMenuCategories() const
{
	return TcsRuntimeAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsStateComponentStateTreeFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategorySubMenus();
}

FText UTcsStateComponentStateTreeFactory::GetDisplayName() const
{
	return LOCTEXT("StateComponentStateTreeDisplayName", "State Component StateTree");
}

FText UTcsStateComponentStateTreeFactory::GetToolTip() const
{
	return LOCTEXT("StateComponentStateTreeToolTip", "Create a StateTree preset for UTcsStateComponent using UTcsSTSchema_StateComponent.");
}

FString UTcsStateComponentStateTreeFactory::GetDefaultNewAssetName() const
{
	return TEXT("ST_StateComponent_New");
}

UTcsBuffStateTreeFactory::UTcsBuffStateTreeFactory()
{
	StateTreeSchemaClass = UTcsSTSchema_Buff::StaticClass();
}

bool UTcsBuffStateTreeFactory::ConfigureProperties()
{
	StateTreeSchemaClass = UTcsSTSchema_Buff::StaticClass();
	return true;
}

bool UTcsBuffStateTreeFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsBuffStateTreeFactory::GetMenuCategories() const
{
	return TcsRuntimeAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsBuffStateTreeFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategorySubMenus();
}

FText UTcsBuffStateTreeFactory::GetDisplayName() const
{
	return LOCTEXT("BuffStateTreeDisplayName", "Buff StateTree");
}

FText UTcsBuffStateTreeFactory::GetToolTip() const
{
	return LOCTEXT("BuffStateTreeToolTip", "Create a StateTree preset for UTcsBuffInstance using UTcsSTSchema_Buff.");
}

FString UTcsBuffStateTreeFactory::GetDefaultNewAssetName() const
{
	return TEXT("ST_Buff_New");
}

UTcsSkillEntryBlueprintFactory::UTcsSkillEntryBlueprintFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bSkipClassPicker = true;
	BlueprintType = BPTYPE_Normal;
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UTcsSkillEntry::StaticClass();
}

bool UTcsSkillEntryBlueprintFactory::ConfigureProperties()
{
	bSkipClassPicker = true;
	BlueprintType = BPTYPE_Normal;
	ParentClass = UTcsSkillEntry::StaticClass();
	return ParentClass != nullptr;
}

bool UTcsSkillEntryBlueprintFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsSkillEntryBlueprintFactory::GetMenuCategories() const
{
	return TcsRuntimeAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsSkillEntryBlueprintFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategorySubMenus();
}

FText UTcsSkillEntryBlueprintFactory::GetDisplayName() const
{
	return LOCTEXT("SkillEntryBlueprintDisplayName", "Skill Entry Blueprint");
}

FText UTcsSkillEntryBlueprintFactory::GetToolTip() const
{
	return LOCTEXT("SkillEntryBlueprintToolTip", "Create a Blueprint subclass of UTcsSkillEntry.");
}

FString UTcsSkillEntryBlueprintFactory::GetDefaultNewAssetName() const
{
	return TEXT("BP_SkillEntry_New");
}

#undef LOCTEXT_NAMESPACE