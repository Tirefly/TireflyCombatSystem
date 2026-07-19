// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"

#include "TcsLogChannels.h"
#include "Engine/AssetManager.h"



void UTcsDefinitionManagerSubsystem::LoadBuffDefinitionsBatch(const TArray<FName>& BuffDefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
{
	StartBatchAsyncLoad(BuffDefinitionSources, TEXT("LoadBuffDefinitionsBatch"), BuffDefIds, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadSkillDefinitionsBatch(const TArray<FName>& SkillDefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
{
	StartBatchAsyncLoad(SkillDefinitionSources, TEXT("LoadSkillDefinitionsBatch"), SkillDefIds, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadStateSlotDefinitionsBatch(const TArray<FName>& StateSlotDefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
{
	StartBatchAsyncLoad(StateSlotDefinitionSources, TEXT("LoadStateSlotDefinitionsBatch"), StateSlotDefIds, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadAttributeDefinitionsBatch(const TArray<FName>& AttributeDefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
{
	StartBatchAsyncLoad(AttributeDefinitionSources, TEXT("LoadAttributeDefinitionsBatch"), AttributeDefIds, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadAttributeModifierDefinitionsBatch(const TArray<FName>& AttributeModifierDefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
{
	StartBatchAsyncLoad(AttributeModifierDefinitionSources, TEXT("LoadAttributeModifierDefinitionsBatch"), AttributeModifierDefIds, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadSkillModifierDefinitionsBatch(const TArray<FName>& SkillModifierDefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
{
	StartBatchAsyncLoad(SkillModifierDefinitionSources, TEXT("LoadSkillModifierDefinitionsBatch"), SkillModifierDefIds, Callback);
}

void UTcsDefinitionManagerSubsystem::StartBatchAsyncLoad(
	const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
	FName EntryName,
	const TArray<FName>& DefIds,
	const FOnTcsDefinitionsBatchLoaded& Callback)
{
	TArray<UPrimaryDataAsset*> AlreadyLoaded;
	TArray<FName> PendingDefIds;

	for (const FName DefId : DefIds)
	{
		const FTcsDefinitionSourceEntry* Entry = SourceCache.Find(DefId);
		if (!Entry)
		{
			LogDefinitionQueryFailure(DefId, *EntryName.ToString(), TEXT("NotRegistered"));
			continue;
		}

		UPrimaryDataAsset* CachedAsset = UAssetManager::Get().GetPrimaryAssetObject<UPrimaryDataAsset>(Entry->AssetId);
		if (CachedAsset && WriteLoadedAssetToCache(Entry->AssetId, CachedAsset))
		{
			AlreadyLoaded.Add(CachedAsset);
		}
		else if (CachedAsset)
		{
			LogDefinitionQueryFailure(DefId, *EntryName.ToString(), TEXT("TypeMismatch"));
		}
		else
		{
			PendingDefIds.Add(DefId);
		}
	}

	// 全部已在 cache 中，立即回调
	if (PendingDefIds.Num() == 0)
	{
		Callback.ExecuteIfBound(DefIds, AlreadyLoaded);
		return;
	}

	// 用共享计数器跟踪未完成数量
	const int32 TotalPending = PendingDefIds.Num();
	TSharedRef<TAtomic<int32>> RemainingCount = MakeShared<TAtomic<int32>>(TotalPending);

	// 共享结果收集器
	TSharedRef<TArray<UPrimaryDataAsset*>> LoadedResults = MakeShared<TArray<UPrimaryDataAsset*>>();
	LoadedResults->Append(AlreadyLoaded);

	const TArray<FName> RequestedDefIds = DefIds;
	TWeakObjectPtr<UTcsDefinitionManagerSubsystem> WeakThis(this);

	for (const FName DefId : PendingDefIds)
	{
		StartAsyncLoad(SourceCache, DefId, EntryName,
			FOnTcsDefinitionAsyncLoaded::CreateLambda(
				[WeakThis, LoadedResults, RemainingCount, TotalPending, RequestedDefIds, Callback]
				(FName LoadedDefId, bool bSuccess, UPrimaryDataAsset* Asset)
				{
					if (bSuccess && Asset)
					{
						LoadedResults->Add(Asset);
					}

					const int32 NewCount = (--(*RemainingCount));
					if (NewCount <= 0)
					{
						Callback.ExecuteIfBound(RequestedDefIds, *LoadedResults);
					}
				}));
	}
}
