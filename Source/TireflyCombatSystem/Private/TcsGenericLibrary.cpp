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

	// 从 DeveloperSettings 缓存获取（编辑器环境下已通过 Asset Registry 扫描）
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		Settings->GetCachedAttributeDefinitions().GetKeys(AttributeNames);
	}

	return AttributeNames;
}

TArray<FName> UTcsGenericLibrary::GetAttributeModifierIds()
{
	TArray<FName> ModifierIds;

	// 从 DeveloperSettings 缓存获取（编辑器环境下已通过 Asset Registry 扫描）
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		Settings->GetCachedAttributeModifierDefinitions().GetKeys(ModifierIds);
	}

	return ModifierIds;
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

	// 从 DeveloperSettings 缓存获取（编辑器环境下已通过 Asset Registry 扫描）
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		Settings->GetCachedStateDefinitions().GetKeys(StateDefNames);
	}

	return StateDefNames;
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
