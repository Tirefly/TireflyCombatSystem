// Copyright Tirefly. All Rights Reserved.

#include "TcsGenericLibrary.h"

#include "TcsDeveloperSettings.h"
#include "TcsEntityInterface.h"
#include "Attribute/TcsAttributeComponent.h"
#include "State/TcsStateComponent.h"
#include "Skill/TcsSkillComponent.h"

namespace
{
void NormalizeEditorOptionNames(TArray<FName>& Names)
{
	Names.RemoveAll([](const FName& Name)
	{
		return Name.IsNone();
	});

	Names.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}
}



TArray<FName> UTcsGenericLibrary::GetAttributeNames()
{
	TArray<FName> AttributeNames;

	// 从 DeveloperSettings 缓存获取（编辑器环境下已通过 Asset Registry 扫描）
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		Settings->GetCachedAttributeDefinitions().GetKeys(AttributeNames);
	}

	NormalizeEditorOptionNames(AttributeNames);

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

	NormalizeEditorOptionNames(ModifierIds);

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

	NormalizeEditorOptionNames(StateDefNames);

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

UTcsSkillComponent *UTcsGenericLibrary::GetSkillComponent(AActor *Actor)
{
	if (IsValid(Actor) && Actor->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetSkillComponent(Actor);
	}
    return nullptr;
}
