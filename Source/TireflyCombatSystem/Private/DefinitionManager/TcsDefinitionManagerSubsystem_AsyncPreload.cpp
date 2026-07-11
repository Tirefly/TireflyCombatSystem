// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"

#include "TcsDeveloperSettings.h"
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
	 * 从 source cache 中收集需要预加载的 PrimaryAssetId。
	 */
	void CollectPreloadAssets(
		const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
		const FTcsDefinitionLoadingConfig& Config,
		TArray<FPrimaryAssetId>& OutAssetIds)
	{
		switch (Config.LoadingStrategy)
		{
		case ETcsDefinitionLoadingStrategy::PreloadAll:
			for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : SourceCache)
			{
				OutAssetIds.Add(Pair.Value.AssetId);
			}
			break;

		case ETcsDefinitionLoadingStrategy::PreloadSelected:
			for (const TSoftObjectPtr<UPrimaryDataAsset>& AssetPtr : Config.SpecificAssets)
			{
				const FSoftObjectPath TargetPath = AssetPtr.ToSoftObjectPath();
				for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : SourceCache)
				{
					if (Pair.Value.SoftPtr.ToSoftObjectPath() == TargetPath)
					{
						OutAssetIds.Add(Pair.Value.AssetId);
						break;
					}
				}
			}
			break;

		case ETcsDefinitionLoadingStrategy::OnDemand:
			break;
		}
	}
}



void UTcsDefinitionManagerSubsystem::RequestAsyncPreload()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		bIsRuntimeReady = true;
		OnRuntimeReady.Broadcast();
		return;
	}

	TArray<FPrimaryAssetId> AssetsToPreload;

	CollectPreloadAssets(BuffDefinitionSources, Settings->BuffDefinitionLoading, AssetsToPreload);
	CollectPreloadAssets(SkillDefinitionSources, Settings->SkillDefinitionLoading, AssetsToPreload);
	CollectPreloadAssets(StateSlotDefinitionSources, Settings->StateSlotDefinitionLoading, AssetsToPreload);
	CollectPreloadAssets(AttributeDefinitionSources, Settings->AttributeDefinitionLoading, AssetsToPreload);
	CollectPreloadAssets(AttributeModifierDefinitionSources, Settings->AttributeModifierDefinitionLoading, AssetsToPreload);
	CollectPreloadAssets(SkillModifierDefinitionSources, Settings->SkillModifierDefinitionLoading, AssetsToPreload);

	if (AssetsToPreload.Num() == 0)
	{
		UE_LOG(LogTcs, Log,
			TEXT("[UTcsDefinitionManagerSubsystem] No assets to preload, marking runtime ready immediately."));
		bIsRuntimeReady = true;
		OnRuntimeReady.Broadcast();
		return;
	}

	PendingPreloadBatchCount = 1;

	TWeakObjectPtr<UTcsDefinitionManagerSubsystem> WeakThis(this);
	UAssetManager::Get().LoadPrimaryAssets(AssetsToPreload, {},
		FStreamableDelegate::CreateLambda([WeakThis, AssetsToPreload]()
		{
			if (UTcsDefinitionManagerSubsystem* Self = WeakThis.Get())
			{
				Self->OnAsyncPreloadComplete(AssetsToPreload);
			}
		}));

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionManagerSubsystem] Requested async preload for %d assets."),
		AssetsToPreload.Num());
}

void UTcsDefinitionManagerSubsystem::OnAsyncPreloadComplete(TArray<FPrimaryAssetId> LoadedAssetIds)
{
	UAssetManager& AssetManager = UAssetManager::Get();

	int32 SuccessCount = 0;
	int32 FailCount = 0;

	for (const FPrimaryAssetId& AssetId : LoadedAssetIds)
	{
		UPrimaryDataAsset* Asset = AssetManager.GetPrimaryAssetObject<UPrimaryDataAsset>(AssetId);
		if (!Asset)
		{
			UE_LOG(LogTcs, Warning,
				TEXT("[UTcsDefinitionManagerSubsystem] Async preload failed for: %s"),
				*AssetId.ToString());
			++FailCount;
			continue;
		}

		WriteLoadedAssetToCache(AssetId, Asset);
		++SuccessCount;
	}

	RebuildTagIndexes();

	PendingPreloadBatchCount = 0;
	bIsRuntimeReady = true;
	OnRuntimeReady.Broadcast();

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionManagerSubsystem] Async preload complete: %d succeeded, %d failed. Runtime ready."),
		SuccessCount,
		FailCount);
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

	AttributeTagToDefId.Empty();
	for (const TPair<FName, TObjectPtr<UTcsAttributeDefinition>>& Pair : AttributeDefinitions)
	{
		const UTcsAttributeDefinition* Definition = Pair.Value;
		if (Definition && Definition->AttributeTag.IsValid())
		{
			AttributeTagToDefId.Add(Definition->AttributeTag, Pair.Key);
		}
	}
}
