// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionManagerSubsystem.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Engine/AssetManager.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateSlotDefinition.h"



namespace
{
	template <typename AssetType>
	void AddPrimaryAssetsToCache(
		TMap<FName, TObjectPtr<AssetType>>& Cache,
		const FPrimaryAssetType& PrimaryAssetType,
		TFunctionRef<FName(const AssetType&)> GetDefinitionId,
		const TCHAR* DefinitionLabel)
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FPrimaryAssetId> PrimaryAssetIds;
		AssetManager.GetPrimaryAssetIdList(PrimaryAssetType, PrimaryAssetIds);

		for (const FPrimaryAssetId& PrimaryAssetId : PrimaryAssetIds)
		{
			const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(PrimaryAssetId);
			if (!AssetPath.IsValid())
			{
				UE_LOG(LogTcs, Warning,
					TEXT("[UTcsDefinitionManagerSubsystem] %s primary asset has invalid path: %s"),
					DefinitionLabel,
					*PrimaryAssetId.ToString());
				continue;
			}

			AssetType* Asset = TSoftObjectPtr<AssetType>(AssetPath).LoadSynchronous();
			if (!Asset)
			{
				UE_LOG(LogTcs, Warning,
					TEXT("[UTcsDefinitionManagerSubsystem] Failed to load %s: %s"),
					DefinitionLabel,
					*PrimaryAssetId.ToString());
				continue;
			}

			const FName DefinitionId = GetDefinitionId(*Asset);
			if (DefinitionId.IsNone())
			{
				UE_LOG(LogTcs, Warning,
					TEXT("[UTcsDefinitionManagerSubsystem] Skipping %s with empty DefId: %s"),
					DefinitionLabel,
					*Asset->GetPathName());
				continue;
			}

			if (const TObjectPtr<AssetType>* ExistingAsset = Cache.Find(DefinitionId))
			{
				if (*ExistingAsset != Asset)
				{
					UE_LOG(LogTcs, Error,
						TEXT("[UTcsDefinitionManagerSubsystem] Duplicate %s DefId '%s': '%s' conflicts with '%s'"),
						DefinitionLabel,
						*DefinitionId.ToString(),
						*(*ExistingAsset)->GetPathName(),
						*Asset->GetPathName());
				}
				continue;
			}

			Cache.Add(DefinitionId, Asset);
		}
	}

	template <typename AssetType>
	const AssetType* LoadDefinitionFromCache(
		const TMap<FName, TObjectPtr<AssetType>>& Cache,
		FName DefinitionId)
	{
		if (const TObjectPtr<AssetType>* Found = Cache.Find(DefinitionId))
		{
			return Found->Get();
		}
		return nullptr;
	}
}



void UTcsDefinitionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsRuntimeReady = false;

	RebuildLoadedCache();
	RebuildTagIndexes();

	bIsRuntimeReady = true;
}

void UTcsDefinitionManagerSubsystem::Deinitialize()
{
	bIsRuntimeReady = false;
	AttributeDefinitions.Empty();
	AttributeModifierDefinitions.Empty();
	BuffDefinitions.Empty();
	BuffTagToDefId.Empty();
	SkillDefinitions.Empty();
	SkillModifierDefinitions.Empty();
	StateDefinitions.Empty();
	StateSlotDefinitions.Empty();
	StateSlotTagToDefId.Empty();

	Super::Deinitialize();
}

const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::GetBuffDefinition(FName BuffDefId) const
{
	const UTcsBuffDefinition* Definition = LoadDefinitionFromCache(BuffDefinitions, BuffDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(BuffDefId, TEXT("GetBuffDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::GetBuffDefinitionByTag(FGameplayTag BuffTag) const
{
	const FName* BuffDefId = BuffTagToDefId.Find(BuffTag);
	if (!BuffDefId)
	{
		LogDefinitionQueryFailure(BuffTag.GetTagName(), TEXT("GetBuffDefinitionByTag"), TEXT("NotRegistered"));
		return nullptr;
	}

	return GetBuffDefinition(*BuffDefId);
}

const UTcsSkillDefinition* UTcsDefinitionManagerSubsystem::GetSkillDefinition(FName SkillDefId) const
{
	const UTcsSkillDefinition* Definition = LoadDefinitionFromCache(SkillDefinitions, SkillDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(SkillDefId, TEXT("GetSkillDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsStateDefinition* UTcsDefinitionManagerSubsystem::GetStateDefinition(FName StateDefId) const
{
	const UTcsStateDefinition* Definition = LoadDefinitionFromCache(StateDefinitions, StateDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(StateDefId, TEXT("GetStateDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::GetStateSlotDefinition(FName StateSlotDefId) const
{
	const UTcsStateSlotDefinition* Definition = LoadDefinitionFromCache(StateSlotDefinitions, StateSlotDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(StateSlotDefId, TEXT("GetStateSlotDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::GetStateSlotDefinitionByTag(FGameplayTag StateSlotTag) const
{
	const FName* StateSlotDefId = StateSlotTagToDefId.Find(StateSlotTag);
	if (!StateSlotDefId)
	{
		LogDefinitionQueryFailure(StateSlotTag.GetTagName(), TEXT("GetStateSlotDefinitionByTag"), TEXT("NotRegistered"));
		return nullptr;
	}

	return GetStateSlotDefinition(*StateSlotDefId);
}

const UTcsAttributeDefinition* UTcsDefinitionManagerSubsystem::GetAttributeDefinition(FName AttributeDefId) const
{
	const UTcsAttributeDefinition* Definition = LoadDefinitionFromCache(AttributeDefinitions, AttributeDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeDefId, TEXT("GetAttributeDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsAttributeModifierDefinition* UTcsDefinitionManagerSubsystem::GetAttributeModifierDefinition(FName AttributeModifierDefId) const
{
	const UTcsAttributeModifierDefinition* Definition = LoadDefinitionFromCache(AttributeModifierDefinitions, AttributeModifierDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeModifierDefId, TEXT("GetAttributeModifierDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsSkillModifierDefinition* UTcsDefinitionManagerSubsystem::GetSkillModifierDefinition(FName SkillModifierDefId) const
{
	const UTcsSkillModifierDefinition* Definition = LoadDefinitionFromCache(SkillModifierDefinitions, SkillModifierDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(SkillModifierDefId, TEXT("GetSkillModifierDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllAttributeDefIds() const
{
	TArray<FName> DefinitionIds;
	AttributeDefinitions.GetKeys(DefinitionIds);
	return DefinitionIds;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllAttributeModifierDefIds() const
{
	TArray<FName> DefinitionIds;
	AttributeModifierDefinitions.GetKeys(DefinitionIds);
	return DefinitionIds;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllStateSlotDefIds() const
{
	TArray<FName> DefinitionIds;
	StateSlotDefinitions.GetKeys(DefinitionIds);
	return DefinitionIds;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllStateLikeDefIds() const
{
	TArray<FName> DefinitionIds;
	StateDefinitions.GetKeys(DefinitionIds);
	return DefinitionIds;
}

void UTcsDefinitionManagerSubsystem::RebuildLoadedCache()
{
	AttributeDefinitions.Empty();
	AttributeModifierDefinitions.Empty();
	BuffDefinitions.Empty();
	SkillDefinitions.Empty();
	SkillModifierDefinitions.Empty();
	StateDefinitions.Empty();
	StateSlotDefinitions.Empty();

	AddPrimaryAssetsToCache<UTcsAttributeDefinition>(
		AttributeDefinitions,
		UTcsAttributeDefinition::PrimaryAssetType,
		[](const UTcsAttributeDefinition& Definition) { return Definition.AttributeDefId; },
		TEXT("UTcsAttributeDefinition"));

	AddPrimaryAssetsToCache<UTcsAttributeModifierDefinition>(
		AttributeModifierDefinitions,
		UTcsAttributeModifierDefinition::PrimaryAssetType,
		[](const UTcsAttributeModifierDefinition& Definition) { return Definition.AttributeModifierDefId; },
		TEXT("UTcsAttributeModifierDefinition"));

	AddPrimaryAssetsToCache<UTcsBuffDefinition>(
		BuffDefinitions,
		UTcsBuffDefinition::PrimaryAssetType,
		[](const UTcsBuffDefinition& Definition) { return Definition.StateDefId; },
		TEXT("UTcsBuffDefinition"));

	AddPrimaryAssetsToCache<UTcsSkillDefinition>(
		SkillDefinitions,
		UTcsSkillDefinition::PrimaryAssetType,
		[](const UTcsSkillDefinition& Definition) { return Definition.StateDefId; },
		TEXT("UTcsSkillDefinition"));

	AddPrimaryAssetsToCache<UTcsSkillModifierDefinition>(
		SkillModifierDefinitions,
		UTcsSkillModifierDefinition::PrimaryAssetType,
		[](const UTcsSkillModifierDefinition& Definition) { return Definition.ModifierId; },
		TEXT("UTcsSkillModifierDefinition"));

	AddPrimaryAssetsToCache<UTcsStateSlotDefinition>(
		StateSlotDefinitions,
		UTcsStateSlotDefinition::PrimaryAssetType,
		[](const UTcsStateSlotDefinition& Definition) { return Definition.StateSlotDefId; },
		TEXT("UTcsStateSlotDefinition"));

	for (const TPair<FName, TObjectPtr<UTcsBuffDefinition>>& Pair : BuffDefinitions)
	{
		StateDefinitions.Add(Pair.Key, Pair.Value);
	}

	for (const TPair<FName, TObjectPtr<UTcsSkillDefinition>>& Pair : SkillDefinitions)
	{
		StateDefinitions.Add(Pair.Key, Pair.Value);
	}

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionManagerSubsystem] Rebuilt loaded cache: %d Attributes, %d AttributeModifiers, %d Buffs, %d Skills, %d SkillModifiers, %d StateSlots, %d StateDefs"),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		BuffDefinitions.Num(),
		SkillDefinitions.Num(),
		SkillModifierDefinitions.Num(),
		StateSlotDefinitions.Num(),
		StateDefinitions.Num());
}

void UTcsDefinitionManagerSubsystem::RebuildTagIndexes()
{
	BuffTagToDefId.Empty();
	for (const TPair<FName, TObjectPtr<UTcsBuffDefinition>>& Pair : BuffDefinitions)
	{
		const UTcsBuffDefinition* Definition = Pair.Value;
		if (Definition && Definition->StateTag.IsValid())
		{
			BuffTagToDefId.Add(Definition->StateTag, Pair.Key);
		}
	}

	StateSlotTagToDefId.Empty();
	for (const TPair<FName, TObjectPtr<UTcsStateSlotDefinition>>& Pair : StateSlotDefinitions)
	{
		const UTcsStateSlotDefinition* Definition = Pair.Value;
		if (Definition && Definition->SlotTag.IsValid())
		{
			StateSlotTagToDefId.Add(Definition->SlotTag, Pair.Key);
		}
	}
}

void UTcsDefinitionManagerSubsystem::LogDefinitionQueryFailure(
	FName QueryKey,
	const TCHAR* EntryName,
	const TCHAR* FailureCategory) const
{
	UE_LOG(LogTcs, Warning,
		TEXT("[UTcsDefinitionManagerSubsystem::%s] Definition query failed. Key=%s Category=%s"),
		EntryName,
		*QueryKey.ToString(),
		FailureCategory);
}
