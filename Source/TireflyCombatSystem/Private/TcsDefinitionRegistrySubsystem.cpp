// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionRegistrySubsystem.h"

#include "TcsDeveloperSettings.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateSlotDefinition.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManagerSettings.h"
#endif

namespace TcsDefinitionRegistryPrivate
{
	template <typename AssetType>
	void AddDefinition(
		TMap<FName, TSoftObjectPtr<AssetType>>& Cache,
		FName DefinitionId,
		const TSoftObjectPtr<AssetType>& AssetPtr,
		const TCHAR* DefinitionLabel)
	{
		if (DefinitionId.IsNone())
		{
			UE_LOG(LogTcs, Warning,
				TEXT("[UTcsDefinitionRegistrySubsystem] Skipping %s with empty DefId: %s"),
				DefinitionLabel,
				*AssetPtr.ToSoftObjectPath().ToString());
			return;
		}

		if (const TSoftObjectPtr<AssetType>* ExistingAsset = Cache.Find(DefinitionId))
		{
			if (ExistingAsset->ToSoftObjectPath() != AssetPtr.ToSoftObjectPath())
			{
				UE_LOG(LogTcs, Error,
					TEXT("[UTcsDefinitionRegistrySubsystem] Duplicate %s DefId '%s': '%s' conflicts with '%s'"),
					DefinitionLabel,
					*DefinitionId.ToString(),
					*ExistingAsset->ToSoftObjectPath().ToString(),
					*AssetPtr.ToSoftObjectPath().ToString());
			}
			return;
		}

		Cache.Add(DefinitionId, AssetPtr);
	}
}

void UTcsDefinitionRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	if (GIsEditor)
	{
		RegisterEditorCallbacks();
		RefreshDefinitionsNow();
	}
#endif
}

void UTcsDefinitionRegistrySubsystem::Deinitialize()
{
#if WITH_EDITOR
	UnregisterEditorCallbacks();
	ClearQueuedRefresh();
#endif

	DefinitionsRefreshed.Clear();
	Super::Deinitialize();
}

#if WITH_EDITOR
void UTcsDefinitionRegistrySubsystem::RequestRefresh()
{
	if (!GIsEditor)
	{
		return;
	}

	if (bIsRefreshing)
	{
		bRefreshRequestedWhileRefreshing = true;
		return;
	}

	if (bIsRefreshQueued)
	{
		return;
	}

	bIsRefreshQueued = true;
	DeferredRefreshHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UTcsDefinitionRegistrySubsystem::HandleDeferredRefresh),
		0.0f);
}

void UTcsDefinitionRegistrySubsystem::RefreshDefinitionsNow()
{
	if (!GIsEditor)
	{
		return;
	}

	if (bIsRefreshing)
	{
		bRefreshRequestedWhileRefreshing = true;
		return;
	}

	ClearQueuedRefresh();

	TGuardValue<bool> RefreshGuard(bIsRefreshing, true);
	RebuildSnapshot();
	MirrorSnapshotToDeveloperSettings();
	bHasCompletedInitialRefresh = true;
	++RefreshRevision;

	DefinitionsRefreshed.Broadcast(this);

	if (bRefreshRequestedWhileRefreshing)
	{
		bRefreshRequestedWhileRefreshing = false;
		RequestRefresh();
	}

	UE_LOG(LogTcs, Verbose,
		TEXT("[UTcsDefinitionRegistrySubsystem] Published refresh revision %d"),
		RefreshRevision);
}

void UTcsDefinitionRegistrySubsystem::RegisterEditorCallbacks()
{
	if (bHasRegisteredEditorCallbacks)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetAddedHandle = AssetRegistry.OnAssetAdded().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetAdded);
	AssetUpdatedHandle = AssetRegistry.OnAssetUpdated().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetUpdated);
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetRemoved);
	AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetRenamed);
	InMemoryAssetCreatedHandle = AssetRegistry.OnInMemoryAssetCreated().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnInMemoryAssetCreated);
	InMemoryAssetDeletedHandle = AssetRegistry.OnInMemoryAssetDeleted().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnInMemoryAssetDeleted);

	if (UAssetManagerSettings* AssetManagerSettings = GetMutableDefault<UAssetManagerSettings>())
	{
		AssetManagerSettingsChangedHandle = AssetManagerSettings->OnSettingChanged().AddUObject(
			this,
			&UTcsDefinitionRegistrySubsystem::OnAssetManagerSettingsChanged);
	}

	bHasRegisteredEditorCallbacks = true;
}

void UTcsDefinitionRegistrySubsystem::UnregisterEditorCallbacks()
{
	if (!bHasRegisteredEditorCallbacks)
	{
		return;
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if (AssetAddedHandle.IsValid())
		{
			AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
		}

		if (AssetUpdatedHandle.IsValid())
		{
			AssetRegistry.OnAssetUpdated().Remove(AssetUpdatedHandle);
		}

		if (AssetRemovedHandle.IsValid())
		{
			AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
		}

		if (AssetRenamedHandle.IsValid())
		{
			AssetRegistry.OnAssetRenamed().Remove(AssetRenamedHandle);
		}

		if (InMemoryAssetCreatedHandle.IsValid())
		{
			AssetRegistry.OnInMemoryAssetCreated().Remove(InMemoryAssetCreatedHandle);
		}

		if (InMemoryAssetDeletedHandle.IsValid())
		{
			AssetRegistry.OnInMemoryAssetDeleted().Remove(InMemoryAssetDeletedHandle);
		}
	}

	if (UAssetManagerSettings* AssetManagerSettings = GetMutableDefault<UAssetManagerSettings>())
	{
		if (AssetManagerSettingsChangedHandle.IsValid())
		{
			AssetManagerSettings->OnSettingChanged().Remove(AssetManagerSettingsChangedHandle);
		}
	}

	AssetAddedHandle.Reset();
	AssetUpdatedHandle.Reset();
	AssetRemovedHandle.Reset();
	AssetRenamedHandle.Reset();
	InMemoryAssetCreatedHandle.Reset();
	InMemoryAssetDeletedHandle.Reset();
	AssetManagerSettingsChangedHandle.Reset();
	bHasRegisteredEditorCallbacks = false;
}

bool UTcsDefinitionRegistrySubsystem::HandleDeferredRefresh(float DeltaTime)
{
	ClearQueuedRefresh();
	RefreshDefinitionsNow();
	return false;
}

void UTcsDefinitionRegistrySubsystem::RebuildSnapshot()
{
	AttributeDefinitions.Empty();
	AttributeModifierDefinitions.Empty();
	StateDefinitions.Empty();
	StateSlotDefinitions.Empty();

	const UAssetManagerSettings* AssetManagerSettings = GetDefault<UAssetManagerSettings>();
	if (!AssetManagerSettings)
	{
		UE_LOG(LogTcs, Error,
			TEXT("[UTcsDefinitionRegistrySubsystem] Failed to get AssetManagerSettings while rebuilding snapshot"));
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	for (const FPrimaryAssetTypeInfo& TypeInfo : AssetManagerSettings->PrimaryAssetTypesToScan)
	{
		ScanPrimaryAssetType(TypeInfo, AssetRegistry);
	}

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionRegistrySubsystem] Rebuilt snapshot: %d Attributes, %d Modifiers, %d States, %d StateSlots"),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		StateDefinitions.Num(),
		StateSlotDefinitions.Num());
}

void UTcsDefinitionRegistrySubsystem::MirrorSnapshotToDeveloperSettings() const
{
	UTcsDeveloperSettings* Settings = GetMutableDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		UE_LOG(LogTcs, Error,
			TEXT("[UTcsDefinitionRegistrySubsystem] Failed to get TcsDeveloperSettings while mirroring snapshot"));
		return;
	}

	Settings->SetCachedAttributeDefinitions(AttributeDefinitions);
	Settings->SetCachedAttributeModifierDefinitions(AttributeModifierDefinitions);
	Settings->SetCachedStateDefinitions(StateDefinitions);
	Settings->SetCachedStateSlotDefinitions(StateSlotDefinitions);
}

void UTcsDefinitionRegistrySubsystem::ScanPrimaryAssetType(const FPrimaryAssetTypeInfo& TypeInfo, IAssetRegistry& AssetRegistry)
{
	if (TypeInfo.PrimaryAssetType != UTcsAttributeDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsAttributeModifierDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsStateDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsStateSlotDefinition::PrimaryAssetType)
	{
		return;
	}

	const TArray<FDirectoryPath> Directories = TypeInfo.GetDirectories();
	for (const FDirectoryPath& Directory : Directories)
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByPath(FName(*Directory.Path), AssetDataList, true);

		if (TypeInfo.PrimaryAssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			ScanAttributeDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			ScanAttributeModifierDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsStateDefinition::PrimaryAssetType)
		{
			ScanStateDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsStateSlotDefinition::PrimaryAssetType)
		{
			ScanStateSlotDefinitions(AssetDataList);
		}
	}
}

void UTcsDefinitionRegistrySubsystem::ScanAttributeDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsAttributeDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsAttributeDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			AttributeDefinitions,
			Asset->AttributeDefId,
			AssetPtr,
			TEXT("Attribute"));
	}
}

void UTcsDefinitionRegistrySubsystem::ScanAttributeModifierDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsAttributeModifierDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsAttributeModifierDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			AttributeModifierDefinitions,
			Asset->AttributeModifierDefId,
			AssetPtr,
			TEXT("AttributeModifier"));
	}
	}

void UTcsDefinitionRegistrySubsystem::ScanStateDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsStateDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsStateDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			StateDefinitions,
			Asset->StateDefId,
			AssetPtr,
			TEXT("State"));
	}
}

void UTcsDefinitionRegistrySubsystem::ScanStateSlotDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsStateSlotDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsStateSlotDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			StateSlotDefinitions,
			Asset->StateSlotDefId,
			AssetPtr,
			TEXT("StateSlot"));
	}
}

bool UTcsDefinitionRegistrySubsystem::IsTrackedDefinitionClass(const FAssetData& AssetData) const
{
	if (!AssetData.IsValid())
	{
		return false;
	}

	UClass* AssetClass = AssetData.GetClass(EResolveClass::Yes);
	if (!AssetClass)
	{
		return false;
	}

	return AssetClass->IsChildOf(UTcsAttributeDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsAttributeModifierDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsStateDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsStateSlotDefinition::StaticClass());
}

bool UTcsDefinitionRegistrySubsystem::IsTrackedDefinitionObject(const UObject* AssetObject) const
{
	return AssetObject && (
		AssetObject->IsA(UTcsAttributeDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsAttributeModifierDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsStateDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsStateSlotDefinition::StaticClass()));
}

void UTcsDefinitionRegistrySubsystem::OnAssetAdded(const FAssetData& AssetData)
{
	if (IsTrackedDefinitionClass(AssetData))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnAssetUpdated(const FAssetData& AssetData)
{
	if (IsTrackedDefinitionClass(AssetData))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnAssetRemoved(const FAssetData& AssetData)
{
	RequestRefresh();
}

void UTcsDefinitionRegistrySubsystem::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	RequestRefresh();
}

void UTcsDefinitionRegistrySubsystem::OnInMemoryAssetCreated(UObject* AssetObject)
{
	if (IsTrackedDefinitionObject(AssetObject))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnInMemoryAssetDeleted(UObject* AssetObject)
{
	if (IsTrackedDefinitionObject(AssetObject))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnAssetManagerSettingsChanged(UObject* SettingsObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	RequestRefresh();
}

void UTcsDefinitionRegistrySubsystem::ClearQueuedRefresh()
{
	bIsRefreshQueued = false;
	if (DeferredRefreshHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeferredRefreshHandle);
		DeferredRefreshHandle.Reset();
	}
	}
#endif