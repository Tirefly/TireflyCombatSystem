// Copyright Tirefly. All Rights Reserved.

#include "TcsGenericLibrary.h"

#include "TcsDeveloperSettings.h"
#include "TcsEntityInterface.h"
#include "Attribute/TcsAttributeComponent.h"
#include "State/TcsStateComponent.h"
#include "Skill/TcsSkillComponent.h"



TArray<FName> UTcsGenericLibrary::GetAttributeNames()
{
	TArray<FName> AttributeNames;
	if (UDataTable* AttributeDefTable = GetAttributeDefTable())
	{
		AttributeNames = AttributeDefTable->GetRowNames();
	}

	return AttributeNames;
}
UDataTable* UTcsGenericLibrary::GetAttributeDefTable()
{
	// TODO: [DataAsset Migration] This function will be removed in Phase 1.3
	// Temporarily return nullptr to allow compilation
	// if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	// {
	// 	return Settings->AttributeDefTable.LoadSynchronous();
	// }
	return nullptr;
}
TArray<FName> UTcsGenericLibrary::GetAttributeModifierIds()
{
	TArray<FName> ModifierIds;
	if (UDataTable* AttributeModifierDefTable = GetAttributeModifierDefTable())
	{
		ModifierIds = AttributeModifierDefTable->GetRowNames();
	}

	return ModifierIds;
}
UDataTable* UTcsGenericLibrary::GetAttributeModifierDefTable()
{
	// TODO: [DataAsset Migration] This function will be removed in Phase 1.3
	// Temporarily return nullptr to allow compilation
	// if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	// {
	// 	return Settings->AttributeModifierDefTable.LoadSynchronous();
	// }
	return nullptr;
}
UTcsAttributeComponent *UTcsGenericLibrary::GetAttributeComponent(AActor *Actor)
{
	if (IsValid(Actor) && Actor->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetAttributeComponent(Actor);
	}
    return nullptr;
}
TArray<FName> UTcsGenericLibrary::GetStateDefNames()
{
	TArray<FName> StateDefNames;
	if (UDataTable* StateDefTable = GetStateDefTable())
	{
		StateDefNames = StateDefTable->GetRowNames();
	}
	return StateDefNames;
}
UDataTable* UTcsGenericLibrary::GetStateDefTable()
{
	// TODO: [DataAsset Migration] This function will be removed in Phase 1.4
	// Temporarily return nullptr to allow compilation
	// if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	// {
	// 	return Settings->StateDefTable.LoadSynchronous();
	// }
	return nullptr;
}
UDataTable* UTcsGenericLibrary::GetStateSlotDefTable()
{
	// TODO: [DataAsset Migration] This function will be removed in Phase 1.4
	// Temporarily return nullptr to allow compilation
	// if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	// {
	// 	return Settings->StateSlotDefTable.LoadSynchronous();
	// }
	return nullptr;
}
UTcsStateComponent* UTcsGenericLibrary::GetStateComponent(AActor *Actor)
{
    if (IsValid(Actor) && Actor->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetStateComponent(Actor);
	}
	return nullptr;
}
TArray<FName> UTcsGenericLibrary::GetSkillModifierIds()
{
	TArray<FName> ModifierIds;
	if (UDataTable* SkillModifierDefTable = GetSkillModifierDefTable())
	{
		ModifierIds = SkillModifierDefTable->GetRowNames();
	}

	return ModifierIds;
}
UDataTable* UTcsGenericLibrary::GetSkillModifierDefTable()
{
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		return Settings->SkillModifierDefTable.LoadSynchronous();
	}
	return nullptr;
}
UTcsSkillComponent *UTcsGenericLibrary::GetSkillComponent(AActor *Actor)
{
	if (IsValid(Actor) && Actor->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetSkillComponent(Actor);
	}
    return nullptr;
}
