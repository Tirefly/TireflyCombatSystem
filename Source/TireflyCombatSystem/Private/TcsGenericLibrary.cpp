// Copyright Tirefly. All Rights Reserved.

#include "TcsGenericLibrary.h"

#include "TcsEntityInterface.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Buff/TcsBuffComponent.h"
#include "Engine/AssetManager.h"
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

void AppendPrimaryAssetNames(const FPrimaryAssetType& PrimaryAssetType, TArray<FName>& Names)
{
	TArray<FPrimaryAssetId> PrimaryAssetIds;
	UAssetManager::Get().GetPrimaryAssetIdList(PrimaryAssetType, PrimaryAssetIds);
	for (const FPrimaryAssetId& PrimaryAssetId : PrimaryAssetIds)
	{
		Names.Add(PrimaryAssetId.PrimaryAssetName);
	}
}
}



TArray<FName> UTcsGenericLibrary::GetAttributeNames()
{
	TArray<FName> AttributeNames;

	AppendPrimaryAssetNames(UTcsAttributeDefinition::PrimaryAssetType, AttributeNames);

	NormalizeEditorOptionNames(AttributeNames);

	return AttributeNames;
}

TArray<FName> UTcsGenericLibrary::GetAttributeModifierIds()
{
	TArray<FName> ModifierIds;

	AppendPrimaryAssetNames(UTcsAttributeModifierDefinition::PrimaryAssetType, ModifierIds);

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
UTcsStateComponent* UTcsGenericLibrary::GetStateComponent(AActor *Actor)
{
    if (IsValid(Actor) && Actor->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetStateComponent(Actor);
	}
	return nullptr;
}

UTcsBuffComponent* UTcsGenericLibrary::GetBuffComponent(AActor* Actor)
{
	if (IsValid(Actor) && Actor->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetBuffComponent(Actor);
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
