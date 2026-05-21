// Copyright Tirefly. All Rights Reserved.

#include "TireflyCombatSystemEditor.h"

#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "TireflyCombatSystemEditor"

namespace TcsCombatSystemEditor
{
	const FName ModuleName(TEXT("TireflyCombatSystemEditor"));
}

FTireflyCombatSystemEditorModule& FTireflyCombatSystemEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FTireflyCombatSystemEditorModule>(GetModuleName());
}

bool FTireflyCombatSystemEditorModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(GetModuleName());
}

FName FTireflyCombatSystemEditorModule::GetModuleName()
{
	return TcsCombatSystemEditor::ModuleName;
}

FText FTireflyCombatSystemEditorModule::GetAssetCategoryDisplayName()
{
	return LOCTEXT("TireflyCombatSystemAssetCategory", "Tirefly Combat System");
}

FText FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryDisplayName()
{
	return LOCTEXT("TireflyCombatSystemDefinitionAssetCategory", "Definition Asset");
}

FText FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategoryDisplayName()
{
	return LOCTEXT("TireflyCombatSystemGameplayRuntimeCategory", "Gameplay Runtime");
}

TConstArrayView<FAssetCategoryPath> FTireflyCombatSystemEditorModule::GetDefinitionAssetCategoryPaths()
{
	static const TArray<FAssetCategoryPath, TFixedAllocator<1>> Categories =
	{
		FAssetCategoryPath(GetAssetCategoryDisplayName(), GetDefinitionAssetCategoryDisplayName())
	};

	return Categories;
}

const TArray<FText>& FTireflyCombatSystemEditorModule::GetDefinitionAssetCategorySubMenus()
{
	static const TArray<FText> SubMenus =
	{
		GetDefinitionAssetCategoryDisplayName()
	};

	return SubMenus;
}

const TArray<FText>& FTireflyCombatSystemEditorModule::GetGameplayRuntimeCategorySubMenus()
{
	static const TArray<FText> SubMenus =
	{
		GetGameplayRuntimeCategoryDisplayName()
	};

	return SubMenus;
}

void FTireflyCombatSystemEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	RegisteredAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
		TEXT("TireflyCombatSystem"),
		GetAssetCategoryDisplayName());
}

void FTireflyCombatSystemEditorModule::ShutdownModule()
{
	RegisteredAssetCategory = 0;
}

IMPLEMENT_MODULE(FTireflyCombatSystemEditorModule, TireflyCombatSystemEditor)

#undef LOCTEXT_NAMESPACE