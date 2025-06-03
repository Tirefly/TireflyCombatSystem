// Copyright Tirefly. All Rights Reserved.


#include "TireflyCombatSystemLibrary.h"

#include "TireflyCombatSystemSettings.h"


TArray<FName> UTireflyCombatSystemLibrary::GetAttributeNames()
{
	TArray<FName> AttributeNames;
	if (UDataTable* AttributeDefTable = GetAttributeDefTable())
	{
		AttributeNames = AttributeDefTable->GetRowNames();
	}

	return AttributeNames;
}

UDataTable* UTireflyCombatSystemLibrary::GetAttributeDefTable()
{
	if (const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>())
	{
		return Settings->AttributeDefTable.LoadSynchronous();
	}
	return nullptr;
}

TArray<FName> UTireflyCombatSystemLibrary::GetAttributeModifierIds()
{
	TArray<FName> ModifierIds;
	if (UDataTable* AttributeModifierDefTable = GetAttributeModifierDefTable())
	{
		ModifierIds = AttributeModifierDefTable->GetRowNames();
	}

	return ModifierIds;
}

UDataTable* UTireflyCombatSystemLibrary::GetAttributeModifierDefTable()
{
	if (const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>())
	{
		return Settings->AttributeModifierDefTable.LoadSynchronous();
	}
	return nullptr;
}
