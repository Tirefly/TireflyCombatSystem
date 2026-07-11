// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Engine/AssetManager.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateSlotDefinition.h"



namespace
{
	/**
	 * 从 AssetManager 获取 PrimaryAssetId 列表，填充 source cache（不加载资产）。
	 */
	template <typename AssetType>
	void AddPrimaryAssetsToSourceCache(
		TMap<FName, FTcsDefinitionSourceEntry>& Cache,
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

			// 同步加载一次以读取 DefId，但不保留硬引用——source cache 只存软引用
			AssetType* Asset = TSoftObjectPtr<AssetType>(AssetPath).LoadSynchronous();
			if (!Asset)
			{
				UE_LOG(LogTcs, Warning,
					TEXT("[UTcsDefinitionManagerSubsystem] Failed to load %s for DefId extraction: %s"),
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

			if (const FTcsDefinitionSourceEntry* ExistingAsset = Cache.Find(DefinitionId))
			{
				if (ExistingAsset->SoftPtr.ToSoftObjectPath() != AssetPath)
				{
					UE_LOG(LogTcs, Error,
						TEXT("[UTcsDefinitionManagerSubsystem] Duplicate %s DefId '%s': '%s' conflicts with '%s'"),
						DefinitionLabel,
						*DefinitionId.ToString(),
						*ExistingAsset->SoftPtr.ToSoftObjectPath().ToString(),
						*AssetPath.ToString());
				}
				continue;
			}

			Cache.Add(DefinitionId, FTcsDefinitionSourceEntry{TSoftObjectPtr<UPrimaryDataAsset>(AssetPath), PrimaryAssetId});
		}
	}
}



void UTcsDefinitionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsRuntimeReady = false;

	RebuildSourceCache();
	RequestAsyncPreload();
}

void UTcsDefinitionManagerSubsystem::Deinitialize()
{
	bIsRuntimeReady = false;
	PendingPreloadBatchCount = 0;
	PendingAsyncLoads.Empty();

	BuffDefinitionSources.Empty();
	SkillDefinitionSources.Empty();
	StateSlotDefinitionSources.Empty();
	AttributeDefinitionSources.Empty();
	AttributeModifierDefinitionSources.Empty();
	SkillModifierDefinitionSources.Empty();
	StateDefinitionSources.Empty();

	BuffDefinitions.Empty();
	SkillDefinitions.Empty();
	StateDefinitions.Empty();
	StateSlotDefinitions.Empty();
	AttributeDefinitions.Empty();
	AttributeModifierDefinitions.Empty();
	SkillModifierDefinitions.Empty();

	BuffTagToDefId.Empty();
	StateSlotTagToDefId.Empty();

	Super::Deinitialize();
}

void UTcsDefinitionManagerSubsystem::RebuildSourceCache()
{
	BuffDefinitionSources.Empty();
	SkillDefinitionSources.Empty();
	StateSlotDefinitionSources.Empty();
	AttributeDefinitionSources.Empty();
	AttributeModifierDefinitionSources.Empty();
	SkillModifierDefinitionSources.Empty();
	StateDefinitionSources.Empty();

	AddPrimaryAssetsToSourceCache<UTcsAttributeDefinition>(
		AttributeDefinitionSources,
		UTcsAttributeDefinition::PrimaryAssetType,
		[](const UTcsAttributeDefinition& Definition) { return Definition.AttributeDefId; },
		TEXT("UTcsAttributeDefinition"));

	AddPrimaryAssetsToSourceCache<UTcsAttributeModifierDefinition>(
		AttributeModifierDefinitionSources,
		UTcsAttributeModifierDefinition::PrimaryAssetType,
		[](const UTcsAttributeModifierDefinition& Definition) { return Definition.AttributeModifierDefId; },
		TEXT("UTcsAttributeModifierDefinition"));

	AddPrimaryAssetsToSourceCache<UTcsBuffDefinition>(
		BuffDefinitionSources,
		UTcsBuffDefinition::PrimaryAssetType,
		[](const UTcsBuffDefinition& Definition) { return Definition.StateDefId; },
		TEXT("UTcsBuffDefinition"));

	AddPrimaryAssetsToSourceCache<UTcsSkillDefinition>(
		SkillDefinitionSources,
		UTcsSkillDefinition::PrimaryAssetType,
		[](const UTcsSkillDefinition& Definition) { return Definition.StateDefId; },
		TEXT("UTcsSkillDefinition"));

	AddPrimaryAssetsToSourceCache<UTcsSkillModifierDefinition>(
		SkillModifierDefinitionSources,
		UTcsSkillModifierDefinition::PrimaryAssetType,
		[](const UTcsSkillModifierDefinition& Definition) { return Definition.ModifierId; },
		TEXT("UTcsSkillModifierDefinition"));

	AddPrimaryAssetsToSourceCache<UTcsStateSlotDefinition>(
		StateSlotDefinitionSources,
		UTcsStateSlotDefinition::PrimaryAssetType,
		[](const UTcsStateSlotDefinition& Definition) { return Definition.StateSlotDefId; },
		TEXT("UTcsStateSlotDefinition"));

	// 构建合并的 StateDefinitionSources
	for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : BuffDefinitionSources)
	{
		StateDefinitionSources.Add(Pair.Key, Pair.Value);
	}

	for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : SkillDefinitionSources)
	{
		StateDefinitionSources.Add(Pair.Key, Pair.Value);
	}

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionManagerSubsystem] Rebuilt source cache: %d Attributes, %d AttributeModifiers, %d Buffs, %d Skills, %d SkillModifiers, %d StateSlots, %d StateDefs"),
		AttributeDefinitionSources.Num(),
		AttributeModifierDefinitionSources.Num(),
		BuffDefinitionSources.Num(),
		SkillDefinitionSources.Num(),
		SkillModifierDefinitionSources.Num(),
		StateSlotDefinitionSources.Num(),
		StateDefinitionSources.Num());
}
