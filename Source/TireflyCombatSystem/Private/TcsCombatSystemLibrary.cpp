// Copyright Tirefly. All Rights Reserved.


#include "TcsCombatSystemLibrary.h"

#include "TcsCombatSystemSettings.h"


TArray<FName> UTcsCombatSystemLibrary::GetAttributeNames()
{
	TArray<FName> AttributeNames;
	if (UDataTable* AttributeDefTable = GetAttributeDefTable())
	{
		AttributeNames = AttributeDefTable->GetRowNames();
	}

	return AttributeNames;
}

UDataTable* UTcsCombatSystemLibrary::GetAttributeDefTable()
{
	if (const UTcsCombatSystemSettings* Settings = GetDefault<UTcsCombatSystemSettings>())
	{
		return Settings->AttributeDefTable.LoadSynchronous();
	}
	return nullptr;
}

TArray<FName> UTcsCombatSystemLibrary::GetAttributeModifierIds()
{
	TArray<FName> ModifierIds;
	if (UDataTable* AttributeModifierDefTable = GetAttributeModifierDefTable())
	{
		ModifierIds = AttributeModifierDefTable->GetRowNames();
	}

	return ModifierIds;
}

UDataTable* UTcsCombatSystemLibrary::GetAttributeModifierDefTable()
{
	if (const UTcsCombatSystemSettings* Settings = GetDefault<UTcsCombatSystemSettings>())
	{
		return Settings->AttributeModifierDefTable.LoadSynchronous();
	}
	return nullptr;
}
