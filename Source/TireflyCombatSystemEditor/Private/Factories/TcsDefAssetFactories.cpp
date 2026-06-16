// Copyright Tirefly. All Rights Reserved.

#include "Factories/TcsDefAssetFactories.h"

#include "AssetTypeCategories.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateSlotDefinition.h"
#include "TireflyCombatSystemEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TcsDefAssetFactories)

#define LOCTEXT_NAMESPACE "TcsDefAssetFactories"

namespace TcsDefAssetFactories
{
	/**
	 * 创建 canonical Def 资产实例。
	 *
	 * @tparam TAssetClass 要创建的 Def 类型。
	 * @param InParent 新资产的 Outer。
	 * @param InName 新资产名称。
	 * @param Flags 新资产对象标志。
	 * @return 创建出的 canonical Def 资产对象。
	 */
	template <typename TAssetClass>
	UObject* CreateDefinitionAsset(UObject* InParent, FName InName, EObjectFlags Flags)
	{
		return NewObject<TAssetClass>(InParent, TAssetClass::StaticClass(), InName, Flags | RF_Transactional);
	}

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

UTcsAttributeDefinitionFactory::UTcsAttributeDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTcsAttributeDefinition::StaticClass();
}

UObject* UTcsAttributeDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return TcsDefAssetFactories::CreateDefinitionAsset<UTcsAttributeDefinition>(InParent, InName, Flags);
}

bool UTcsAttributeDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsAttributeDefinitionFactory::GetMenuCategories() const
{
	return TcsDefAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsAttributeDefinitionFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus();
}

FText UTcsAttributeDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("AttributeDefinitionDisplayName", "Attribute Definition");
}

FText UTcsAttributeDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("AttributeDefinitionToolTip", "Create a Tirefly Combat System Attribute Definition asset.");
}

FString UTcsAttributeDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("DA_Attr_New");
}

UTcsAttributeModifierDefinitionFactory::UTcsAttributeModifierDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTcsAttributeModifierDefinition::StaticClass();
}

UObject* UTcsAttributeModifierDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent,
	FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return TcsDefAssetFactories::CreateDefinitionAsset<UTcsAttributeModifierDefinition>(InParent, InName, Flags);
}

bool UTcsAttributeModifierDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsAttributeModifierDefinitionFactory::GetMenuCategories() const
{
	return TcsDefAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsAttributeModifierDefinitionFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus();
}

FText UTcsAttributeModifierDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("AttributeModifierDefinitionDisplayName", "Attribute Modifier Definition");
}

FText UTcsAttributeModifierDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("AttributeModifierDefinitionToolTip", "Create a Tirefly Combat System Attribute Modifier Definition asset.");
}

FString UTcsAttributeModifierDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("DA_AttrMod_New");
}

UTcsBuffDefinitionFactory::UTcsBuffDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTcsBuffDefinition::StaticClass();
}

UObject* UTcsBuffDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return TcsDefAssetFactories::CreateDefinitionAsset<UTcsBuffDefinition>(InParent, InName, Flags);
}

bool UTcsBuffDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsBuffDefinitionFactory::GetMenuCategories() const
{
	return TcsDefAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsBuffDefinitionFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus();
}

FText UTcsBuffDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("BuffDefinitionDisplayName", "Buff Definition");
}

FText UTcsBuffDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("BuffDefinitionToolTip", "Create a Tirefly Combat System Buff Definition asset.");
}

FString UTcsBuffDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("DA_Buff_New");
}

UTcsStateSlotDefinitionFactory::UTcsStateSlotDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTcsStateSlotDefinition::StaticClass();
}

UObject* UTcsStateSlotDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return TcsDefAssetFactories::CreateDefinitionAsset<UTcsStateSlotDefinition>(InParent, InName, Flags);
}

bool UTcsStateSlotDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsStateSlotDefinitionFactory::GetMenuCategories() const
{
	return TcsDefAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsStateSlotDefinitionFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus();
}

FText UTcsStateSlotDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("StateSlotDefinitionDisplayName", "State Slot Definition");
}

FText UTcsStateSlotDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("StateSlotDefinitionToolTip", "Create a Tirefly Combat System State Slot Definition asset.");
}

FString UTcsStateSlotDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("DA_StateSlot_New");
}

UTcsSkillModifierDefinitionFactory::UTcsSkillModifierDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTcsSkillModifierDefinition::StaticClass();
}

UObject* UTcsSkillModifierDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent,
	FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return TcsDefAssetFactories::CreateDefinitionAsset<UTcsSkillModifierDefinition>(InParent, InName, Flags);
}

bool UTcsSkillModifierDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsSkillModifierDefinitionFactory::GetMenuCategories() const
{
	return TcsDefAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsSkillModifierDefinitionFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus();
}

FText UTcsSkillModifierDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("SkillModifierDefinitionDisplayName", "Skill Modifier Definition");
}

FText UTcsSkillModifierDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("SkillModifierDefinitionToolTip", "Create a Tirefly Combat System Skill Modifier Definition asset.");
}

FString UTcsSkillModifierDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("DA_SkillMod_New");
}

UTcsSkillDefinitionFactory::UTcsSkillDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTcsSkillDefinition::StaticClass();
}

UObject* UTcsSkillDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent,
	FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return TcsDefAssetFactories::CreateDefinitionAsset<UTcsSkillDefinition>(InParent, InName, Flags);
}

bool UTcsSkillDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

uint32 UTcsSkillDefinitionFactory::GetMenuCategories() const
{
	return TcsDefAssetFactories::GetTcsMenuCategory();
}

const TArray<FText>& UTcsSkillDefinitionFactory::GetMenuCategorySubMenus() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus();
}

FText UTcsSkillDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("SkillDefinitionDisplayName", "Skill Definition");
}

FText UTcsSkillDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("SkillDefinitionToolTip", "Create a Tirefly Combat System Skill Definition asset.");
}

FString UTcsSkillDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("DA_Skill_New");
}

#undef LOCTEXT_NAMESPACE