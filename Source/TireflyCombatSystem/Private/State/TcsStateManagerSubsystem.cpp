// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"

#include "TcsDeveloperSettings.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinitionAsset.h"
#include "State/TcsStateSlotDefinitionAsset.h"

#if !WITH_EDITOR
#include "Engine/AssetManager.h"
#endif


void UTcsStateManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	LoadFromDeveloperSettings();
#else
	LoadFromAssetManager();
#endif
}

void UTcsStateManagerSubsystem::LoadFromDeveloperSettings()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get TcsDeveloperSettings"),
			*FString(__FUNCTION__));
		return;
	}

	StateSlotDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedStateSlotDefinitions())
	{
		const UTcsStateSlotDefinitionAsset* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			StateSlotDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load StateSlotDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (StateSlotDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] No StateSlotDefinitions loaded from DeveloperSettings"),
			*FString(__FUNCTION__));
	}

	const ETcsStateLoadingStrategy LoadingStrategy = Settings->StateLoadingStrategy;
	StateDefinitions.Empty();
	StateTagToDefId.Empty();

	switch (LoadingStrategy)
	{
	case ETcsStateLoadingStrategy::PreloadAll:
		PreloadAllStates();
		UE_LOG(LogTcsState, Log, TEXT("[%s] PreloadAll strategy: Loaded %d State definitions"),
			*FString(__FUNCTION__),
			StateDefinitions.Num());
		break;
	case ETcsStateLoadingStrategy::OnDemand:
		UE_LOG(LogTcsState, Log, TEXT("[%s] OnDemand strategy: State definitions will be loaded on first access"),
			*FString(__FUNCTION__));
		break;
	case ETcsStateLoadingStrategy::Hybrid:
		PreloadCommonStates();
		UE_LOG(LogTcsState, Log, TEXT("[%s] Hybrid strategy: Preloaded %d common State definitions"),
			*FString(__FUNCTION__),
			StateDefinitions.Num());
		break;
	}

	UE_LOG(LogTcsState, Log, TEXT("[%s] Initialized: %d StateSlots, %d States, %d Tag mappings, Strategy: %s"),
		*FString(__FUNCTION__),
		StateSlotDefinitions.Num(),
		StateDefinitions.Num(),
		StateTagToDefId.Num(),
		LoadingStrategy == ETcsStateLoadingStrategy::PreloadAll ? TEXT("PreloadAll") :
		LoadingStrategy == ETcsStateLoadingStrategy::OnDemand ? TEXT("OnDemand") : TEXT("Hybrid"));
}

void UTcsStateManagerSubsystem::PreloadAllStates()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		return;
	}

	for (const auto& Pair : Settings->GetCachedStateDefinitions())
	{
		const FName& DefId = Pair.Key;
		const UTcsStateDefinitionAsset* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			StateDefinitions.Add(DefId, Asset);

			if (Asset->StateTag.IsValid())
			{
				if (StateTagToDefId.Contains(Asset->StateTag))
				{
					const FName ExistingDefId = StateTagToDefId[Asset->StateTag];
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Duplicate StateTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
						*FString(__FUNCTION__),
						*Asset->StateTag.ToString(),
						*ExistingDefId.ToString(),
						*DefId.ToString());
				}
				else
				{
					StateTagToDefId.Add(Asset->StateTag, DefId);
				}
			}
		}
		else
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load StateDefinition: %s"),
				*FString(__FUNCTION__),
				*DefId.ToString());
		}
	}
}

void UTcsStateManagerSubsystem::PreloadCommonStates()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		return;
	}

	for (const TSoftObjectPtr<UTcsStateDefinitionAsset>& AssetPtr : Settings->CommonStateDefinitions)
	{
		if (AssetPtr.IsNull())
		{
			continue;
		}

		const UTcsStateDefinitionAsset* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load common State from explicit list: %s"),
				*FString(__FUNCTION__),
				*AssetPtr.ToSoftObjectPath().ToString());
			continue;
		}

		const FName DefId = Asset->StateDefId;
		if (StateDefinitions.Contains(DefId))
		{
			continue;
		}

		StateDefinitions.Add(DefId, Asset);
		if (Asset->StateTag.IsValid())
		{
			if (StateTagToDefId.Contains(Asset->StateTag))
			{
				const FName ExistingDefId = StateTagToDefId[Asset->StateTag];
				UE_LOG(LogTcsState, Error,
					TEXT("[%s] Duplicate StateTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
					*FString(__FUNCTION__),
					*Asset->StateTag.ToString(),
					*ExistingDefId.ToString(),
					*DefId.ToString());
			}
			else
			{
				StateTagToDefId.Add(Asset->StateTag, DefId);
			}
		}

		UE_LOG(LogTcsState, Verbose, TEXT("[%s] Preloaded common State (explicit): %s"),
			*FString(__FUNCTION__),
			*DefId.ToString());
	}

	for (const auto& Pair : Settings->GetCachedStateDefinitions())
	{
		const FName& DefId = Pair.Key;
		if (StateDefinitions.Contains(DefId))
		{
			continue;
		}

		const TSoftObjectPtr<UTcsStateDefinitionAsset>& AssetPtr = Pair.Value;
		const FString AssetPath = AssetPtr.ToSoftObjectPath().ToString();

		bool bIsCommon = false;
		for (const FDirectoryPath& CommonPath : Settings->CommonStateDefinitionPaths)
		{
			if (AssetPath.StartsWith(CommonPath.Path))
			{
				bIsCommon = true;
				break;
			}
		}

		if (!bIsCommon)
		{
			continue;
		}

		const UTcsStateDefinitionAsset* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load common StateDefinition: %s"),
				*FString(__FUNCTION__),
				*DefId.ToString());
			continue;
		}

		StateDefinitions.Add(DefId, Asset);
		if (Asset->StateTag.IsValid())
		{
			if (StateTagToDefId.Contains(Asset->StateTag))
			{
				const FName ExistingDefId = StateTagToDefId[Asset->StateTag];
				UE_LOG(LogTcsState, Error,
					TEXT("[%s] Duplicate StateTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
					*FString(__FUNCTION__),
					*Asset->StateTag.ToString(),
					*ExistingDefId.ToString(),
					*DefId.ToString());
			}
			else
			{
				StateTagToDefId.Add(Asset->StateTag, DefId);
			}
		}

		UE_LOG(LogTcsState, Verbose, TEXT("[%s] Preloaded common State (path): %s"),
			*FString(__FUNCTION__),
			*DefId.ToString());
	}
}

const UTcsStateDefinitionAsset* UTcsStateManagerSubsystem::LoadStateOnDemand(FName StateDefId)
{
	if (const UTcsStateDefinitionAsset* const* AssetPtr = StateDefinitions.Find(StateDefId))
	{
		return *AssetPtr;
	}

	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	if (const TSoftObjectPtr<UTcsStateDefinitionAsset>* AssetPtr = Settings->GetCachedStateDefinitions().Find(StateDefId))
	{
		const UTcsStateDefinitionAsset* Asset = AssetPtr->LoadSynchronous();
		if (Asset)
		{
			StateDefinitions.Add(StateDefId, Asset);
			if (Asset->StateTag.IsValid() && !StateTagToDefId.Contains(Asset->StateTag))
			{
				StateTagToDefId.Add(Asset->StateTag, StateDefId);
			}

			UE_LOG(LogTcsState, Verbose, TEXT("[%s] Loaded State on demand: %s"),
				*FString(__FUNCTION__),
				*StateDefId.ToString());
			return Asset;
		}
	}

	UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load State on demand: %s"),
		*FString(__FUNCTION__),
		*StateDefId.ToString());
	return nullptr;
}

const UTcsStateDefinitionAsset* UTcsStateManagerSubsystem::GetStateDefinitionAsset(FName DefId)
{
	if (const UTcsStateDefinitionAsset* const* AssetPtr = StateDefinitions.Find(DefId))
	{
		return *AssetPtr;
	}

	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (Settings && (Settings->StateLoadingStrategy == ETcsStateLoadingStrategy::OnDemand ||
		Settings->StateLoadingStrategy == ETcsStateLoadingStrategy::Hybrid))
	{
		return LoadStateOnDemand(DefId);
	}

	UE_LOG(LogTcsState, Warning, TEXT("[%s] StateDefinition not found: %s"),
		*FString(__FUNCTION__),
		*DefId.ToString());
	return nullptr;
}

const UTcsStateDefinitionAsset* UTcsStateManagerSubsystem::GetStateDefinitionAssetByTag(FGameplayTag StateTag)
{
	if (const FName* DefId = StateTagToDefId.Find(StateTag))
	{
		return GetStateDefinitionAsset(*DefId);
	}

	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (Settings && (Settings->StateLoadingStrategy == ETcsStateLoadingStrategy::OnDemand ||
		Settings->StateLoadingStrategy == ETcsStateLoadingStrategy::Hybrid))
	{
		for (const auto& Pair : Settings->GetCachedStateDefinitions())
		{
			const FName& DefId = Pair.Key;
			const UTcsStateDefinitionAsset* Asset = LoadStateOnDemand(DefId);
			if (Asset && Asset->StateTag == StateTag)
			{
				return Asset;
			}
		}
	}

	UE_LOG(LogTcsState, Warning, TEXT("[%s] StateDefinition not found by tag: %s"),
		*FString(__FUNCTION__),
		*StateTag.ToString());
	return nullptr;
}

const UTcsStateSlotDefinitionAsset* UTcsStateManagerSubsystem::GetStateSlotDefinitionAsset(FName DefId)
{
	if (const UTcsStateSlotDefinitionAsset* const* AssetPtr = StateSlotDefinitions.Find(DefId))
	{
		return *AssetPtr;
	}

	UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDefinition not found: %s"),
		*FString(__FUNCTION__),
		*DefId.ToString());
	return nullptr;
}

const UTcsStateSlotDefinitionAsset* UTcsStateManagerSubsystem::GetStateSlotDefinitionAssetByTag(FGameplayTag SlotTag)
{
	for (const auto& Pair : StateSlotDefinitions)
	{
		const UTcsStateSlotDefinitionAsset* Asset = Pair.Value;
		if (Asset && Asset->SlotTag == SlotTag)
		{
			return Asset;
		}
	}

	UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDefinition not found for tag: %s"),
		*FString(__FUNCTION__),
		*SlotTag.ToString());
	return nullptr;
}

TArray<FName> UTcsStateManagerSubsystem::GetAllStateDefNames() const
{
	TArray<FName> Names;
	StateDefinitions.GetKeys(Names);
	return Names;
}

TArray<FName> UTcsStateManagerSubsystem::GetAllStateSlotDefNames() const
{
	TArray<FName> Names;
	StateSlotDefinitions.GetKeys(Names);
	return Names;
}

bool UTcsStateManagerSubsystem::TryApplyStateToTarget(
	AActor* Target,
	FName StateDefId,
	AActor* Instigator,
	int32 StateLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	if (!IsValid(Target) || !IsValid(Instigator) || StateDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid target, instigator, or StateDefId."),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsStateComponent* TargetStateCmp = UTcsGenericLibrary::GetStateComponent(Target);
	if (!IsValid(TargetStateCmp))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Target does not have state component: %s"),
			*FString(__FUNCTION__),
			*Target->GetName());
		return false;
	}

	return TargetStateCmp->TryApplyState(StateDefId, Instigator, StateLevel, ParentSourceHandle);
}

void UTcsStateManagerSubsystem::LoadFromAssetManager()
{
#if !WITH_EDITOR
	UAssetManager& AssetManager = UAssetManager::Get();

	StateSlotDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> StateSlotDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsStateSlotDefinitionAsset::PrimaryAssetType, StateSlotDefIds);

		for (const FPrimaryAssetId& AssetId : StateSlotDefIds)
		{
			const UTcsStateSlotDefinitionAsset* Asset = Cast<UTcsStateSlotDefinitionAsset>(AssetManager.LoadPrimaryAsset(AssetId));
			if (Asset)
			{
				StateSlotDefinitions.Add(Asset->StateSlotDefId, Asset);
			}
			else
			{
				UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load StateSlotDefinition: %s"),
					*FString(__FUNCTION__),
					*AssetId.ToString());
			}
		}

		UE_LOG(LogTcsState, Log, TEXT("[%s] Loaded %d StateSlotDefinitions from AssetManager"),
			*FString(__FUNCTION__),
			StateSlotDefinitions.Num());
	}

	StateDefinitions.Empty();
	StateTagToDefId.Empty();

	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	const ETcsStateLoadingStrategy LoadingStrategy = Settings ? Settings->StateLoadingStrategy : ETcsStateLoadingStrategy::PreloadAll;

	switch (LoadingStrategy)
	{
	case ETcsStateLoadingStrategy::PreloadAll:
		{
			TArray<FPrimaryAssetId> StateDefIds;
			AssetManager.GetPrimaryAssetIdList(UTcsStateDefinitionAsset::PrimaryAssetType, StateDefIds);

			for (const FPrimaryAssetId& AssetId : StateDefIds)
			{
				const UTcsStateDefinitionAsset* Asset = Cast<UTcsStateDefinitionAsset>(AssetManager.LoadPrimaryAsset(AssetId));
				if (Asset)
				{
					StateDefinitions.Add(Asset->StateDefId, Asset);
					if (Asset->StateTag.IsValid() && !StateTagToDefId.Contains(Asset->StateTag))
					{
						StateTagToDefId.Add(Asset->StateTag, Asset->StateDefId);
					}
				}
			}

			UE_LOG(LogTcsState, Log, TEXT("[%s] PreloadAll strategy: Loaded %d State definitions from AssetManager"),
				*FString(__FUNCTION__),
				StateDefinitions.Num());
		}
		break;

	case ETcsStateLoadingStrategy::OnDemand:
		UE_LOG(LogTcsState, Log, TEXT("[%s] OnDemand strategy: State definitions will be loaded on first access"),
			*FString(__FUNCTION__));
		break;

	case ETcsStateLoadingStrategy::Hybrid:
		{
			for (const TSoftObjectPtr<UTcsStateDefinitionAsset>& AssetPtr : Settings->CommonStateDefinitions)
			{
				if (AssetPtr.IsNull())
				{
					continue;
				}

				const UTcsStateDefinitionAsset* Asset = AssetPtr.LoadSynchronous();
				if (!Asset)
				{
					UE_LOG(LogTcsState, Warning, TEXT("[%s] Hybrid: Failed to load common State (explicit): %s"),
						*FString(__FUNCTION__),
						*AssetPtr.ToSoftObjectPath().ToString());
					continue;
				}

				if (!StateDefinitions.Contains(Asset->StateDefId))
				{
					StateDefinitions.Add(Asset->StateDefId, Asset);
					if (Asset->StateTag.IsValid() && !StateTagToDefId.Contains(Asset->StateTag))
					{
						StateTagToDefId.Add(Asset->StateTag, Asset->StateDefId);
					}

					UE_LOG(LogTcsState, Verbose, TEXT("[%s] Hybrid: Preloaded common State (explicit): %s"),
						*FString(__FUNCTION__),
						*Asset->StateDefId.ToString());
				}
			}

			if (Settings->CommonStateDefinitionPaths.Num() > 0)
			{
				TArray<FPrimaryAssetId> AllStateDefIds;
				AssetManager.GetPrimaryAssetIdList(UTcsStateDefinitionAsset::PrimaryAssetType, AllStateDefIds);

				for (const FPrimaryAssetId& AssetId : AllStateDefIds)
				{
					FAssetData AssetData;
					if (!AssetManager.GetPrimaryAssetData(AssetId, AssetData))
					{
						continue;
					}

					const FString AssetPath = AssetData.GetObjectPathString();
					bool bIsCommon = false;
					for (const FDirectoryPath& CommonPath : Settings->CommonStateDefinitionPaths)
					{
						if (AssetPath.StartsWith(CommonPath.Path))
						{
							bIsCommon = true;
							break;
						}
					}

					if (!bIsCommon)
					{
						continue;
					}

					const UTcsStateDefinitionAsset* Asset = Cast<UTcsStateDefinitionAsset>(AssetManager.LoadPrimaryAsset(AssetId));
					if (!Asset)
					{
						UE_LOG(LogTcsState, Warning, TEXT("[%s] Hybrid: Failed to load common State (path): %s"),
							*FString(__FUNCTION__),
							*AssetId.ToString());
						continue;
					}

					if (!StateDefinitions.Contains(Asset->StateDefId))
					{
						StateDefinitions.Add(Asset->StateDefId, Asset);
						if (Asset->StateTag.IsValid() && !StateTagToDefId.Contains(Asset->StateTag))
						{
							StateTagToDefId.Add(Asset->StateTag, Asset->StateDefId);
						}

						UE_LOG(LogTcsState, Verbose, TEXT("[%s] Hybrid: Preloaded common State (path): %s"),
							*FString(__FUNCTION__),
							*Asset->StateDefId.ToString());
					}
				}
			}

			UE_LOG(LogTcsState, Log, TEXT("[%s] Hybrid strategy: Preloaded %d common State definitions, remaining will be loaded on demand"),
				*FString(__FUNCTION__),
				StateDefinitions.Num());
		}
		break;
	}

	UE_LOG(LogTcsState, Log, TEXT("[%s] Initialized from AssetManager: %d StateSlots, %d States, %d Tag mappings"),
		*FString(__FUNCTION__),
		StateSlotDefinitions.Num(),
		StateDefinitions.Num(),
		StateTagToDefId.Num());
#endif
}