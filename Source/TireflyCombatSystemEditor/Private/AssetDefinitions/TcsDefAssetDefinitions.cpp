// Copyright Tirefly. All Rights Reserved.

#include "AssetDefinitions/TcsDefAssetDefinitions.h"

#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateSlotDefinition.h"
#include "TireflyCombatSystemEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TcsDefAssetDefinitions)

#define LOCTEXT_NAMESPACE "TcsDefAssetDefinitions"

FText UAssetDefinition_TcsAttributeDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AttributeDefinitionAssetName", "Attribute Definition");
}

FLinearColor UAssetDefinition_TcsAttributeDefinition::GetAssetColor() const
{
	return FColor(76, 175, 80);
}

TSoftClassPtr<UObject> UAssetDefinition_TcsAttributeDefinition::GetAssetClass() const
{
	return UTcsAttributeDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_TcsAttributeDefinition::GetAssetCategories() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths();
}

FText UAssetDefinition_TcsAttributeModifierDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AttributeModifierDefinitionAssetName", "Attribute Modifier Definition");
}

FLinearColor UAssetDefinition_TcsAttributeModifierDefinition::GetAssetColor() const
{
	return FColor(255, 167, 38);
}

TSoftClassPtr<UObject> UAssetDefinition_TcsAttributeModifierDefinition::GetAssetClass() const
{
	return UTcsAttributeModifierDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_TcsAttributeModifierDefinition::GetAssetCategories() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths();
}

FText UAssetDefinition_TcsBuffDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("BuffDefinitionAssetName", "Buff Definition");
}

FLinearColor UAssetDefinition_TcsBuffDefinition::GetAssetColor() const
{
	return FColor(121, 85, 72);
}

TSoftClassPtr<UObject> UAssetDefinition_TcsBuffDefinition::GetAssetClass() const
{
	return UTcsBuffDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_TcsBuffDefinition::GetAssetCategories() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths();
}

FText UAssetDefinition_TcsStateSlotDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("StateSlotDefinitionAssetName", "State Slot Definition");
}

FLinearColor UAssetDefinition_TcsStateSlotDefinition::GetAssetColor() const
{
	return FColor(0, 188, 212);
}

TSoftClassPtr<UObject> UAssetDefinition_TcsStateSlotDefinition::GetAssetClass() const
{
	return UTcsStateSlotDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_TcsStateSlotDefinition::GetAssetCategories() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths();
}

FText UAssetDefinition_TcsSkillModifierDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("SkillModifierDefinitionAssetName", "Skill Modifier Definition");
}

FLinearColor UAssetDefinition_TcsSkillModifierDefinition::GetAssetColor() const
{
	return FColor(156, 39, 176);
}

TSoftClassPtr<UObject> UAssetDefinition_TcsSkillModifierDefinition::GetAssetClass() const
{
	return UTcsSkillModifierDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_TcsSkillModifierDefinition::GetAssetCategories() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths();
}

FText UAssetDefinition_TcsSkillDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("SkillDefinitionAssetName", "Skill Definition");
}

FLinearColor UAssetDefinition_TcsSkillDefinition::GetAssetColor() const
{
	return FColor(63, 81, 181);
}

TSoftClassPtr<UObject> UAssetDefinition_TcsSkillDefinition::GetAssetClass() const
{
	return UTcsSkillDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_TcsSkillDefinition::GetAssetCategories() const
{
	return FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths();
}

#undef LOCTEXT_NAMESPACE