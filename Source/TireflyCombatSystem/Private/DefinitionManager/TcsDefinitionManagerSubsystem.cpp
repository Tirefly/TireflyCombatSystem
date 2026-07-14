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
	 * 所有受管 Definition 的 GetPrimaryAssetId 均以其 DefId 作为 PrimaryAssetName。
	 *
	 * @param Cache 要填充的具体 Definition source cache。
	 * @param PrimaryAssetType 要从 AssetManager 枚举的具体 PrimaryAsset 类型。
	 * @param DefinitionLabel 用于失败诊断的 Definition 类型名称。
	 */
	void AddPrimaryAssetsToSourceCache(
		TMap<FName, FTcsDefinitionSourceEntry>& Cache,
		const FPrimaryAssetType& PrimaryAssetType,
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

			const FName DefinitionId = PrimaryAssetId.PrimaryAssetName;
			if (DefinitionId.IsNone())
			{
				UE_LOG(LogTcs, Warning,
					TEXT("[UTcsDefinitionManagerSubsystem] Skipping %s with empty PrimaryAssetName/DefId: %s"),
					DefinitionLabel,
					*PrimaryAssetId.ToString());
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
	AmbiguousStateDefinitionIds.Empty();

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
	AmbiguousStateDefinitionIds.Empty();
	AddPrimaryAssetsToSourceCache(
		AttributeDefinitionSources,
		UTcsAttributeDefinition::PrimaryAssetType,
		TEXT("UTcsAttributeDefinition"));

	AddPrimaryAssetsToSourceCache(
		AttributeModifierDefinitionSources,
		UTcsAttributeModifierDefinition::PrimaryAssetType,
		TEXT("UTcsAttributeModifierDefinition"));

	AddPrimaryAssetsToSourceCache(
		BuffDefinitionSources,
		UTcsBuffDefinition::PrimaryAssetType,
		TEXT("UTcsBuffDefinition"));

	AddPrimaryAssetsToSourceCache(
		SkillDefinitionSources,
		UTcsSkillDefinition::PrimaryAssetType,
		TEXT("UTcsSkillDefinition"));

	AddPrimaryAssetsToSourceCache(
		SkillModifierDefinitionSources,
		UTcsSkillModifierDefinition::PrimaryAssetType,
		TEXT("UTcsSkillModifierDefinition"));

	AddPrimaryAssetsToSourceCache(
		StateSlotDefinitionSources,
		UTcsStateSlotDefinition::PrimaryAssetType,
		TEXT("UTcsStateSlotDefinition"));

	// State 查询复用具体 Buff / Skill 的发现结果，不建立独立的 AssetManager 扫描或加载配置族。
	const auto AddStateDefinitionSources = [this](
		const TMap<FName, FTcsDefinitionSourceEntry>& ConcreteSources,
		const TCHAR* DefinitionLabel)
	{
		for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : ConcreteSources)
		{
			if (const FTcsDefinitionSourceEntry* ExistingSource = StateDefinitionSources.Find(Pair.Key))
			{
				if (ExistingSource->SoftPtr.ToSoftObjectPath() != Pair.Value.SoftPtr.ToSoftObjectPath())
				{
					AmbiguousStateDefinitionIds.Add(Pair.Key);
					UE_LOG(LogTcs, Error,
						TEXT("[UTcsDefinitionManagerSubsystem] StateDefId '%s' is duplicated across State-like Definition types: '%s' conflicts with %s '%s'. State internal query is disabled for this ID."),
						*Pair.Key.ToString(),
						*ExistingSource->SoftPtr.ToSoftObjectPath().ToString(),
						DefinitionLabel,
						*Pair.Value.SoftPtr.ToSoftObjectPath().ToString());
				}
				continue;
			}

			StateDefinitionSources.Add(Pair.Key, Pair.Value);
		}
	};

	AddStateDefinitionSources(BuffDefinitionSources, TEXT("UTcsBuffDefinition"));
	AddStateDefinitionSources(SkillDefinitionSources, TEXT("UTcsSkillDefinition"));

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionManagerSubsystem] Rebuilt source cache without loading assets: %d Attributes, %d AttributeModifiers, %d Buffs, %d Skills, %d SkillModifiers, %d StateSlots, %d StateDefs, %d ambiguous StateDefIds"),
		AttributeDefinitionSources.Num(),
		AttributeModifierDefinitionSources.Num(),
		BuffDefinitionSources.Num(),
		SkillDefinitionSources.Num(),
		SkillModifierDefinitionSources.Num(),
		StateSlotDefinitionSources.Num(),
		StateDefinitionSources.Num(),
		AmbiguousStateDefinitionIds.Num());
}
