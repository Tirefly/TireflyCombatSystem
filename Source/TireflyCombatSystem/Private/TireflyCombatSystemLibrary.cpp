// Copyright Tirefly. All Rights Reserved.


#include "TireflyCombatSystemLibrary.h"

#include "TireflyCombatSystemSettings.h"


TArray<FName> UTireflyCombatSystemLibrary::GetAttributeNames()
{
	TArray<FName> AttributeNames;
	if (const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>())
	{
		if (UDataTable* AttributeDefTable = Settings->AttributeDefTable.LoadSynchronous())
		{
			AttributeNames = AttributeDefTable->GetRowNames();
		}
	}

	return AttributeNames;
}
