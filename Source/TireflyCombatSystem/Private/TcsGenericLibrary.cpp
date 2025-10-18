// Copyright Tirefly. All Rights Reserved.


#include "TcsGenericLibrary.h"

#include "TcsDeveloperSettings.h"


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
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		return Settings->AttributeDefTable.LoadSynchronous();
	}
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
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		return Settings->AttributeModifierDefTable.LoadSynchronous();
	}
	return nullptr;
}

UDataTable* UTcsGenericLibrary::GetSkillModifierDefTable()
{
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		return Settings->SkillModifierDefTable.LoadSynchronous();
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
