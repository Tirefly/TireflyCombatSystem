// Copyright Tirefly. All Rights Reserved.

#include "Factories/TcsRuntimeAssetFactories.h"

#include "AssetTypeCategories.h"
#include "Components/StateTreeComponentSchema.h"
#include "Skill/TcsSkillInstance.h"
#include "StateTree/TcsStateTreeSchema_StateInstance.h"
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
	StateTreeSchemaClass = UStateTreeComponentSchema::StaticClass();
}

bool UTcsStateComponentStateTreeFactory::ConfigureProperties()
{
	StateTreeSchemaClass = UStateTreeComponentSchema::StaticClass();
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
	return LOCTEXT("StateComponentStateTreeToolTip", "Create a StateTree preset for UTcsStateComponent using UStateTreeComponentSchema.");
}

FString UTcsStateComponentStateTreeFactory::GetDefaultNewAssetName() const
{
	return TEXT("ST_StateComponent_New");
}

UTcsStateInstanceStateTreeFactory::UTcsStateInstanceStateTreeFactory()
{
	StateTreeSchemaClass = UTcsStateTreeSchema_StateInstance::StaticClass();
}

bool UTcsStateInstanceStateTreeFactory::ConfigureProperties()
{
	StateTreeSchemaClass = UTcsStateTreeSchema_StateInstance::StaticClass();
	return true;
}

bool UTcsStateInstanceStateTreeFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsStateInstanceStateTreeFactory::GetMenuCategories() const
{
	return TcsRuntimeAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsStateInstanceStateTreeFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategorySubMenus();
}

FText UTcsStateInstanceStateTreeFactory::GetDisplayName() const
{
	return LOCTEXT("StateInstanceStateTreeDisplayName", "State Instance StateTree");
}

FText UTcsStateInstanceStateTreeFactory::GetToolTip() const
{
	return LOCTEXT("StateInstanceStateTreeToolTip", "Create a StateTree preset for UTcsStateInstance using UTcsStateTreeSchema_StateInstance.");
}

FString UTcsStateInstanceStateTreeFactory::GetDefaultNewAssetName() const
{
	return TEXT("ST_StateInstance_New");
}

UTcsSkillInstanceBlueprintFactory::UTcsSkillInstanceBlueprintFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bSkipClassPicker = true;
	BlueprintType = BPTYPE_Normal;
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UTcsSkillInstance::StaticClass();
}

bool UTcsSkillInstanceBlueprintFactory::ConfigureProperties()
{
	bSkipClassPicker = true;
	BlueprintType = BPTYPE_Normal;
	ParentClass = UTcsSkillInstance::StaticClass();
	return ParentClass != nullptr;
}

bool UTcsSkillInstanceBlueprintFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsSkillInstanceBlueprintFactory::GetMenuCategories() const
{
	return TcsRuntimeAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsSkillInstanceBlueprintFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategorySubMenus();
}

FText UTcsSkillInstanceBlueprintFactory::GetDisplayName() const
{
	return LOCTEXT("SkillInstanceBlueprintDisplayName", "Skill Instance Blueprint");
}

FText UTcsSkillInstanceBlueprintFactory::GetToolTip() const
{
	return LOCTEXT("SkillInstanceBlueprintToolTip", "Create a Blueprint subclass of UTcsSkillInstance.");
}

FString UTcsSkillInstanceBlueprintFactory::GetDefaultNewAssetName() const
{
	return TEXT("BP_SkillInstance_New");
}

#undef LOCTEXT_NAMESPACE