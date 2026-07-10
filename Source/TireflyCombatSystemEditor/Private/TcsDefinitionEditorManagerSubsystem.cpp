// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionEditorManagerSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DataTableSync/TcsDefDataTableRows.h"
#include "DataTableEditorUtils.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "ObjectTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TcsDefinitionRegistrySubsystem.h"
#include "TcsDeveloperSettings.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogTcsEditorSync, Log, All);



namespace
{
	constexpr double DefAssetRemovalSuppressionSeconds = 5.0;

	FString NormalizeContentDirectoryPath(const FString& InPath)
	{
		FString Normalized = InPath;
		Normalized.TrimStartAndEndInline();

		while (Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1, EAllowShrinking::No);
		}

		return Normalized;
	}

	FString NormalizeObjectPath(const FString& InObjectPath)
	{
		FString Normalized = InObjectPath;
		Normalized.TrimStartAndEndInline();
		return Normalized;
	}

	FString BuildManagedDefAssetName(const FName DefId)
	{
		FString AssetName = ObjectTools::SanitizeObjectName(DefId.ToString());
		if (AssetName.IsEmpty())
		{
			return AssetName;
		}

		if (!AssetName.StartsWith(TEXT("DA_")))
		{
			AssetName = FString::Printf(TEXT("DA_%s"), *AssetName);
		}

		return AssetName;
	}

	FString GetLongPackageNameFromObjectPath(const FString& ObjectPath)
	{
		return FPackageName::ObjectPathToPackageName(ObjectPath);
	}

	FString GetLongPackagePathFromObjectPath(const FString& ObjectPath)
	{
		const FString PackageName = GetLongPackageNameFromObjectPath(ObjectPath);
		return PackageName.IsEmpty() ? FString() : NormalizeContentDirectoryPath(FPackageName::GetLongPackagePath(PackageName));
	}

	bool IsObjectPathUnderDirectory(const FString& ObjectPath, const FString& ManagedDirectoryPath)
	{
		const FString PackagePath = GetLongPackagePathFromObjectPath(ObjectPath);
		return !PackagePath.IsEmpty() && PackagePath == NormalizeContentDirectoryPath(ManagedDirectoryPath);
	}

	FString GetObjectPathString(const UObject& Object)
	{
		return Object.GetPathName();
	}

	bool IsSupportedManagedDefAssetClass(const UClass* DefAssetClass)
	{
		return ResolveExpectedDefDataTableRowStruct(DefAssetClass) != nullptr;
	}

	bool AreRowsEquivalent(const FInstancedStruct& LeftRow, const FInstancedStruct& RightRow)
	{
		return LeftRow.IsValid() && RightRow.IsValid() && LeftRow == RightRow;
	}

	bool ShouldUseSyncConfig(const UTcsDeveloperSettings& Settings, const FTcsDataTableSyncConfig& Config)
	{
		return Settings.ValidateConfig(Config).bValid && Config.DefAssetClass && IsSupportedManagedDefAssetClass(Config.DefAssetClass.Get());
	}

	bool BuildManagedDataTableRowMap(const FTcsDataTableSyncConfig& Config,
		const TArray<FAssetData>& AssetDataList,
		UEditorAssetSubsystem& EditorAssetSubsystem,
		TMap<FName, FString>& OutAssetPathByDefId,
		TArray<FString>& OutOrphanAssetPaths)
	{
		OutAssetPathByDefId.Reset();
		OutOrphanAssetPaths.Reset();

		for (const FAssetData& AssetData : AssetDataList)
		{
			const FString DefAssetObjectPath = AssetData.GetSoftObjectPath().ToString();
			UPrimaryDataAsset* const DefAsset = Cast<UPrimaryDataAsset>(EditorAssetSubsystem.LoadAsset(DefAssetObjectPath));
			if (!DefAsset || !DefAsset->IsA(Config.DefAssetClass.Get()))
			{
				continue;
			}

			FName DefId = NAME_None;
			if (!TryGetDefAssetSyncId(DefAsset, DefId) || DefId.IsNone())
			{
				OutOrphanAssetPaths.Add(DefAssetObjectPath);
				continue;
			}

			if (const FString* const ExistingPath = OutAssetPathByDefId.Find(DefId))
			{
				UE_LOG(LogTcsEditorSync, Error,
					TEXT("[UTcsDefinitionEditorManagerSubsystem] Managed directory contains duplicate DefId '%s': %s and %s"),
					*DefId.ToString(),
					**ExistingPath,
					*DefAssetObjectPath);
				OutOrphanAssetPaths.Add(DefAssetObjectPath);
				continue;
			}

			OutAssetPathByDefId.Add(DefId, DefAssetObjectPath);
		}

		return true;
	}
}

bool UTcsDefinitionEditorManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return GIsEditor;
}

void UTcsDefinitionEditorManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!GIsEditor)
	{
		return;
	}

	RebuildManagedDefAssetBindings();
	RegisterEditorCallbacks();
}

void UTcsDefinitionEditorManagerSubsystem::Deinitialize()
{
	UnregisterEditorCallbacks();

	if (DeferredSyncHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeferredSyncHandle);
		DeferredSyncHandle.Reset();
	}

	PendingSyncRequests.Reset();
	CachedDefAssetBindings.Reset();
	PendingRemovedSnapshots.Reset();
	SuppressedDefAssetRemovalExpirations.Reset();
	NextRequestSequence = 1;
	Super::Deinitialize();
}

void UTcsDefinitionEditorManagerSubsystem::RegisterEditorCallbacks()
{
	if (bHasRegisteredCallbacks || !FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddUObject(this, &UTcsDefinitionEditorManagerSubsystem::OnAssetRemoved);
	AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddUObject(this, &UTcsDefinitionEditorManagerSubsystem::OnAssetRenamed);
	InMemoryAssetDeletedHandle = AssetRegistry.OnInMemoryAssetDeleted().AddUObject(this, &UTcsDefinitionEditorManagerSubsystem::OnInMemoryAssetDeleted);
	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddUObject(this, &UTcsDefinitionEditorManagerSubsystem::OnPackageSaved);
	bHasRegisteredCallbacks = true;
}

void UTcsDefinitionEditorManagerSubsystem::UnregisterEditorCallbacks()
{
	if (!bHasRegisteredCallbacks)
	{
		return;
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if (AssetRemovedHandle.IsValid())
		{
			AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
			AssetRemovedHandle.Reset();
		}

		if (AssetRenamedHandle.IsValid())
		{
			AssetRegistry.OnAssetRenamed().Remove(AssetRenamedHandle);
			AssetRenamedHandle.Reset();
		}

		if (InMemoryAssetDeletedHandle.IsValid())
		{
			AssetRegistry.OnInMemoryAssetDeleted().Remove(InMemoryAssetDeletedHandle);
			InMemoryAssetDeletedHandle.Reset();
		}
	}

	if (PackageSavedHandle.IsValid())
	{
		UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
		PackageSavedHandle.Reset();
	}

	bHasRegisteredCallbacks = false;
}

bool UTcsDefinitionEditorManagerSubsystem::HandleDeferredSync(float DeltaTime)
{
	DeferredSyncHandle.Reset();

	if (bIsProcessingRequests || PendingSyncRequests.IsEmpty())
	{
		return false;
	}

	bIsProcessingRequests = true;
	bIsApplyingSync = true;

	PendingSyncRequests.Sort([](const FTcsPendingSyncRequest& Left, const FTcsPendingSyncRequest& Right)
	{
		return Left.Sequence < Right.Sequence;
	});

	TArray<FTcsPendingSyncRequest> Requests = MoveTemp(PendingSyncRequests);
	PendingSyncRequests.Reset();

	bool bAnyChangeApplied = false;
	for (const FTcsPendingSyncRequest& Request : Requests)
	{
		switch (Request.Type)
		{
		case ETcsPendingSyncRequestType::SyncDataTable:
			bAnyChangeApplied |= ProcessDataTableSync(Request.ObjectPath);
			break;

		case ETcsPendingSyncRequestType::SyncDefAsset:
			bAnyChangeApplied |= ProcessDefAssetSync(Request.ObjectPath);
			break;

		case ETcsPendingSyncRequestType::RemoveDefAsset:
			bAnyChangeApplied |= ProcessDefAssetRemoval(Request.ObjectPath);
			break;
		}
	}

	bIsApplyingSync = false;
	bIsProcessingRequests = false;

	if (bAnyChangeApplied)
	{
		RefreshDefinitionRegistry();
	}

	if (!PendingSyncRequests.IsEmpty())
	{
		DeferredSyncHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UTcsDefinitionEditorManagerSubsystem::HandleDeferredSync),
			0.0f);
	}

	return false;
}

void UTcsDefinitionEditorManagerSubsystem::QueueSyncRequest(const ETcsPendingSyncRequestType Type, const FString& ObjectPath)
{
	if (ObjectPath.IsEmpty())
	{
		return;
	}

	FTcsPendingSyncRequest& PendingRequest = PendingSyncRequests.Emplace_GetRef();
	PendingRequest.Type = Type;
	PendingRequest.ObjectPath = NormalizeObjectPath(ObjectPath);
	PendingRequest.Sequence = NextRequestSequence++;

	if (!DeferredSyncHandle.IsValid())
	{
		DeferredSyncHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UTcsDefinitionEditorManagerSubsystem::HandleDeferredSync),
			0.0f);
	}
}

void UTcsDefinitionEditorManagerSubsystem::QueueManagedDataTableSync(const FString& DataTableObjectPath)
{
	QueueSyncRequest(ETcsPendingSyncRequestType::SyncDataTable, DataTableObjectPath);
}

void UTcsDefinitionEditorManagerSubsystem::QueueManagedDefAssetSync(const FString& DefAssetObjectPath)
{
	QueueSyncRequest(ETcsPendingSyncRequestType::SyncDefAsset, DefAssetObjectPath);
}

void UTcsDefinitionEditorManagerSubsystem::QueueManagedDefAssetRemoval(const FString& DefAssetObjectPath)
{
	QueueSyncRequest(ETcsPendingSyncRequestType::RemoveDefAsset, DefAssetObjectPath);
}

void UTcsDefinitionEditorManagerSubsystem::OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext)
{
	if (bIsApplyingSync || !Package)
	{
		return;
	}

	TArray<UObject*> PackageObjects;
	GetObjectsWithPackage(Package, PackageObjects, false);

	for (UObject* const PackageObject : PackageObjects)
	{
		if (UDataTable* const DataTable = Cast<UDataTable>(PackageObject))
		{
			const FString DataTableObjectPath = GetObjectPathString(*DataTable);
			if (FindSyncConfigByDataTablePath(DataTableObjectPath))
			{
				QueueManagedDataTableSync(DataTableObjectPath);
			}
			continue;
		}

		if (UPrimaryDataAsset* const DefAsset = Cast<UPrimaryDataAsset>(PackageObject))
		{
			const FString DefAssetObjectPath = GetObjectPathString(*DefAsset);
			if (FindSyncConfigByDefAssetPath(DefAssetObjectPath))
			{
				QueueManagedDefAssetSync(DefAssetObjectPath);
			}
		}
	}
}

void UTcsDefinitionEditorManagerSubsystem::OnAssetRemoved(const FAssetData& AssetData)
{
	if (bIsApplyingSync)
	{
		return;
	}

	const FString DefAssetObjectPath = AssetData.GetSoftObjectPath().ToString();
	if (ConsumeSuppressedDefAssetRemoval(DefAssetObjectPath))
	{
		return;
	}

	FTcsRemovedDefAssetSnapshot Snapshot;
	if (TryBuildRemovedSnapshot(DefAssetObjectPath, Snapshot))
	{
		PendingRemovedSnapshots.Add(DefAssetObjectPath, Snapshot);
		QueueManagedDefAssetRemoval(DefAssetObjectPath);
	}
}

void UTcsDefinitionEditorManagerSubsystem::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	if (bIsApplyingSync)
	{
		return;
	}

	const FString NewObjectPath = AssetData.GetSoftObjectPath().ToString();
	FTcsRemovedDefAssetSnapshot Snapshot;
	if (TryBuildRemovedSnapshot(OldObjectPath, Snapshot))
	{
		PendingRemovedSnapshots.Add(OldObjectPath, Snapshot);
		QueueManagedDefAssetRemoval(OldObjectPath);
	}

	if (FindSyncConfigByDefAssetPath(NewObjectPath))
	{
		QueueManagedDefAssetSync(NewObjectPath);
	}
}

void UTcsDefinitionEditorManagerSubsystem::OnInMemoryAssetDeleted(UObject* AssetObject)
{
	if (bIsApplyingSync)
	{
		return;
	}

	UPrimaryDataAsset* const DefAsset = Cast<UPrimaryDataAsset>(AssetObject);
	if (!DefAsset)
	{
		return;
	}

	const FString DefAssetObjectPath = GetObjectPathString(*DefAsset);
	if (!FindSyncConfigByDefAssetPath(DefAssetObjectPath))
	{
		return;
	}

	FTcsRemovedDefAssetSnapshot Snapshot;
	Snapshot.ObjectPath = NormalizeObjectPath(DefAssetObjectPath);
	TryGetDefAssetSyncId(DefAsset, Snapshot.DefId);

	if (const FTcsCachedDefAssetBinding* const CachedBinding = CachedDefAssetBindings.Find(Snapshot.ObjectPath))
	{
		if (Snapshot.DefId.IsNone())
		{
			Snapshot.DefId = CachedBinding->DefId;
		}
		Snapshot.TargetDataTable = CachedBinding->TargetDataTable;
	}

	if (Snapshot.TargetDataTable.IsNull())
	{
		if (const FTcsDataTableSyncConfig* const Config = FindSyncConfigByDefAssetPath(DefAssetObjectPath))
		{
			Snapshot.TargetDataTable = Config->TargetDataTable.ToSoftObjectPath();
		}
	}

	if (!Snapshot.DefId.IsNone() && !Snapshot.TargetDataTable.IsNull())
	{
		PendingRemovedSnapshots.Add(DefAssetObjectPath, Snapshot);
		QueueManagedDefAssetRemoval(DefAssetObjectPath);
		return;
	}

	if (TryBuildRemovedSnapshot(DefAssetObjectPath, Snapshot))
	{
		PendingRemovedSnapshots.Add(DefAssetObjectPath, Snapshot);
		QueueManagedDefAssetRemoval(DefAssetObjectPath);
	}
}

bool UTcsDefinitionEditorManagerSubsystem::ProcessDataTableSync(const FString& DataTableObjectPath)
{
	const FTcsDataTableSyncConfig* const Config = FindSyncConfigByDataTablePath(DataTableObjectPath);
	if (!Config)
	{
		return false;
	}

	UDataTable* DataTable = nullptr;
	if (!LoadManagedDataTable(*Config, DataTable) || !DataTable)
	{
		UE_LOG(LogTcsEditorSync, Warning,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to load managed DataTable: %s"),
			*DataTableObjectPath);
		return false;
	}

	return SyncDataTableToManagedDefAssets(*Config, *DataTable);
}

bool UTcsDefinitionEditorManagerSubsystem::ProcessDefAssetSync(const FString& DefAssetObjectPath)
{
	const FTcsDataTableSyncConfig* const Config = FindSyncConfigByDefAssetPath(DefAssetObjectPath);
	if (!Config)
	{
		return false;
	}

	UPrimaryDataAsset* DefAsset = nullptr;
	if (!LoadManagedDefAsset(DefAssetObjectPath, DefAsset) || !DefAsset)
	{
		return false;
	}

	FName DefId = NAME_None;
	if (!TryGetDefAssetSyncId(DefAsset, DefId) || DefId.IsNone())
	{
		UE_LOG(LogTcsEditorSync, Warning,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] Managed DefAsset is missing DefId and cannot sync: %s"),
			*DefAssetObjectPath);
		return false;
	}

	CacheDefAssetBinding(DefAssetObjectPath, DefId, *Config);

	if (!Config->bSyncDefAssetToDataTable)
	{
		return false;
	}

	return SyncManagedDefAssetToDataTable(*Config, *DefAsset);
}

bool UTcsDefinitionEditorManagerSubsystem::ProcessDefAssetRemoval(const FString& DefAssetObjectPath)
{
	FTcsRemovedDefAssetSnapshot Snapshot;
	if (!PendingRemovedSnapshots.RemoveAndCopyValue(DefAssetObjectPath, Snapshot))
	{
		return false;
	}

	RemoveCachedDefAssetBinding(DefAssetObjectPath);
	return RemoveDataTableRowForDeletedDefAsset(Snapshot);
}

bool UTcsDefinitionEditorManagerSubsystem::SyncDataTableToManagedDefAssets(const FTcsDataTableSyncConfig& Config, UDataTable& DataTable)
{
	const UScriptStruct* const ExpectedRowStruct = ResolveExpectedDefDataTableRowStruct(Config.DefAssetClass.Get());
	if (!ExpectedRowStruct || DataTable.GetRowStruct() != ExpectedRowStruct)
	{
		UE_LOG(LogTcsEditorSync, Error,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] DataTable RowStruct mismatch. DataTable=%s Expected=%s Actual=%s"),
			*GetObjectPathString(DataTable),
			ExpectedRowStruct ? *ExpectedRowStruct->GetPathName() : TEXT("None"),
			DataTable.GetRowStruct() ? *DataTable.GetRowStruct()->GetPathName() : TEXT("None"));
		return false;
	}

	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	if (!EditorAssetSubsystem)
	{
		return false;
	}

	TArray<FAssetData> ManagedAssetDataList;
	CollectManagedDefAssets(Config, ManagedAssetDataList);

	TMap<FName, FString> ExistingAssetPathByDefId;
	TArray<FString> OrphanAssetPaths;
	BuildManagedDataTableRowMap(Config, ManagedAssetDataList, *EditorAssetSubsystem, ExistingAssetPathByDefId, OrphanAssetPaths);

	bool bAnyChangeApplied = false;
	TSet<FName> SeenRowNames;
	for (const FName RowName : DataTable.GetRowNames())
	{
		SeenRowNames.Add(RowName);

		FInstancedStruct RowData;
		if (!ExtractRowDataFromDataTable(DataTable, RowName, RowData))
		{
			UE_LOG(LogTcsEditorSync, Warning,
				TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to extract row '%s' from DataTable: %s"),
				*RowName.ToString(),
				*GetObjectPathString(DataTable));
			continue;
		}

		const FString* const ExistingAssetPath = ExistingAssetPathByDefId.Find(RowName);
		if (ExistingAssetPath)
		{
			UPrimaryDataAsset* ExistingDefAsset = nullptr;
			if (!LoadManagedDefAsset(*ExistingAssetPath, ExistingDefAsset) || !ExistingDefAsset)
			{
				continue;
			}

			FName ExistingRowName = NAME_None;
			FInstancedStruct ExistingRowData;
			const bool bHasExistingRowData = TryBuildDefAssetDataTableRow(ExistingDefAsset, ExistingRowName, ExistingRowData);
			if (bHasExistingRowData && ExistingRowName == RowName && AreRowsEquivalent(ExistingRowData, RowData))
			{
				CacheDefAssetBinding(*ExistingAssetPath, RowName, Config);
				continue;
			}

			if (!TryApplyDefAssetDataTableRow(RowName, RowData, ExistingDefAsset))
			{
				UE_LOG(LogTcsEditorSync, Error,
					TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to apply DataTable row '%s' to DefAsset: %s"),
					*RowName.ToString(),
					**ExistingAssetPath);
				continue;
			}

			bAnyChangeApplied |= MarkLoadedAssetDirty(ExistingDefAsset);
			CacheDefAssetBinding(*ExistingAssetPath, RowName, Config);
			continue;
		}

		const FString ManagedDirectoryPath = NormalizeContentDirectoryPath(Config.ManagedDefAssetDirectory.Path);
		if (!EnsureManagedDirectoryExists(ManagedDirectoryPath))
		{
			continue;
		}

		FString AssetName = BuildManagedDefAssetName(RowName);
		if (AssetName.IsEmpty())
		{
			UE_LOG(LogTcsEditorSync, Error,
				TEXT("[UTcsDefinitionEditorManagerSubsystem] Cannot create DefAsset for row '%s' because the sanitized asset name is empty."),
				*RowName.ToString());
			continue;
		}

		const FString PackagePath = FString::Printf(TEXT("%s/%s"), *ManagedDirectoryPath, *AssetName);
		const FString NewAssetObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);

		UPrimaryDataAsset* NewDefAsset = nullptr;
		if (EditorAssetSubsystem->DoesAssetExist(NewAssetObjectPath))
		{
			if (!LoadManagedDefAsset(NewAssetObjectPath, NewDefAsset) || !NewDefAsset)
			{
				continue;
			}
		}
		else
		{
			UPackage* const AssetPackage = CreatePackage(*PackagePath);
			if (!AssetPackage)
			{
				UE_LOG(LogTcsEditorSync, Error,
					TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to create package for new DefAsset: %s"),
					*PackagePath);
				continue;
			}

			UObject* const NewAssetObject = NewObject<UObject>(AssetPackage,
				Config.DefAssetClass.Get(),
				*AssetName,
				RF_Public | RF_Standalone | RF_Transactional);
			NewDefAsset = Cast<UPrimaryDataAsset>(NewAssetObject);
			if (!NewDefAsset)
			{
				UE_LOG(LogTcsEditorSync, Error,
					TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to instantiate managed DefAsset class '%s'."),
					*GetNameSafe(Config.DefAssetClass.Get()));
				continue;
			}

			FAssetRegistryModule::AssetCreated(NewDefAsset);
		}

		if (!TryApplyDefAssetDataTableRow(RowName, RowData, NewDefAsset))
		{
			UE_LOG(LogTcsEditorSync, Error,
				TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to initialize new DefAsset from row '%s': %s"),
				*RowName.ToString(),
				*NewAssetObjectPath);
			continue;
		}

		bAnyChangeApplied |= MarkLoadedAssetDirty(NewDefAsset);
		CacheDefAssetBinding(NewAssetObjectPath, RowName, Config);
	}

	if (Config.bAllowDeleteOrphanDefAssets)
	{
		for (const FString& OrphanAssetPath : OrphanAssetPaths)
		{
			UPrimaryDataAsset* OrphanDefAsset = nullptr;
			if (!LoadManagedDefAsset(OrphanAssetPath, OrphanDefAsset) || !OrphanDefAsset)
			{
				continue;
			}

			TrackSuppressedDefAssetRemoval(OrphanAssetPath);
			RemoveCachedDefAssetBinding(OrphanAssetPath);
			bAnyChangeApplied |= DeleteLoadedAsset(OrphanDefAsset);
		}

		for (const TPair<FName, FString>& Pair : ExistingAssetPathByDefId)
		{
			if (SeenRowNames.Contains(Pair.Key))
			{
				continue;
			}

			UPrimaryDataAsset* OrphanDefAsset = nullptr;
			if (!LoadManagedDefAsset(Pair.Value, OrphanDefAsset) || !OrphanDefAsset)
			{
				continue;
			}

			TrackSuppressedDefAssetRemoval(Pair.Value);
			RemoveCachedDefAssetBinding(Pair.Value);
			bAnyChangeApplied |= DeleteLoadedAsset(OrphanDefAsset);
		}
	}

	return bAnyChangeApplied;
}

bool UTcsDefinitionEditorManagerSubsystem::SyncManagedDefAssetToDataTable(const FTcsDataTableSyncConfig& Config, UPrimaryDataAsset& DefAsset)
{
	UDataTable* DataTable = nullptr;
	if (!LoadManagedDataTable(Config, DataTable) || !DataTable)
	{
		return false;
	}

	const UScriptStruct* const ExpectedRowStruct = ResolveExpectedDefDataTableRowStruct(Config.DefAssetClass.Get());
	if (!ExpectedRowStruct || DataTable->GetRowStruct() != ExpectedRowStruct)
	{
		UE_LOG(LogTcsEditorSync, Error,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] DataTable RowStruct mismatch while syncing DefAsset '%s'."),
			*GetObjectPathString(DefAsset));
		return false;
	}

	FName RowName = NAME_None;
	FInstancedStruct RowData;
	if (!TryBuildDefAssetDataTableRow(&DefAsset, RowName, RowData) || RowName.IsNone())
	{
		UE_LOG(LogTcsEditorSync, Warning,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to build DataTable row from DefAsset: %s"),
			*GetObjectPathString(DefAsset));
		return false;
	}

	const FString DefAssetObjectPath = GetObjectPathString(DefAsset);
	bool bDataTableChanged = false;
	if (const FTcsCachedDefAssetBinding* const CachedBinding = CachedDefAssetBindings.Find(DefAssetObjectPath))
	{
		if (!CachedBinding->DefId.IsNone() && CachedBinding->DefId != RowName)
		{
			FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			DataTable->RemoveRow(CachedBinding->DefId);
			FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			bDataTableChanged |= MarkLoadedAssetDirty(DataTable);
		}
	}

	FInstancedStruct ExistingRowData;
	const bool bHasExistingRow = ExtractRowDataFromDataTable(*DataTable, RowName, ExistingRowData);
	if (bHasExistingRow && AreRowsEquivalent(ExistingRowData, RowData))
	{
		CacheDefAssetBinding(DefAssetObjectPath, RowName, Config);
		return bDataTableChanged;
	}

	if (!UpsertDataTableRow(*DataTable, RowName, RowData))
	{
		UE_LOG(LogTcsEditorSync, Error,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to write row '%s' into DataTable: %s"),
			*RowName.ToString(),
			*GetObjectPathString(*DataTable));
		return false;
	}

	bDataTableChanged |= MarkLoadedAssetDirty(DataTable);
	CacheDefAssetBinding(DefAssetObjectPath, RowName, Config);
	return bDataTableChanged;
}

bool UTcsDefinitionEditorManagerSubsystem::RemoveDataTableRowForDeletedDefAsset(const FTcsRemovedDefAssetSnapshot& Snapshot)
{
	if (Snapshot.DefId.IsNone() || Snapshot.TargetDataTable.IsNull())
	{
		UE_LOG(LogTcsEditorSync, Error,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] Cannot remove DataTable row for deleted DefAsset because DefId or TargetDataTable is missing. Asset=%s"),
			*Snapshot.ObjectPath);
		return false;
	}

	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	if (!EditorAssetSubsystem)
	{
		return false;
	}

	UDataTable* const DataTable = Cast<UDataTable>(EditorAssetSubsystem->LoadAsset(Snapshot.TargetDataTable.ToString()));
	if (!DataTable)
	{
		UE_LOG(LogTcsEditorSync, Warning,
			TEXT("[UTcsDefinitionEditorManagerSubsystem] Failed to load target DataTable while removing deleted DefAsset row. Asset=%s DataTable=%s"),
			*Snapshot.ObjectPath,
			*Snapshot.TargetDataTable.ToString());
		return false;
	}

	if (!DataTable->GetRowMap().Contains(Snapshot.DefId))
	{
		return false;
	}

	FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	DataTable->RemoveRow(Snapshot.DefId);
	FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	return MarkLoadedAssetDirty(DataTable);
}

bool UTcsDefinitionEditorManagerSubsystem::LoadManagedDataTable(const FTcsDataTableSyncConfig& Config, UDataTable*& OutDataTable) const
{
	OutDataTable = nullptr;

	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	if (!EditorAssetSubsystem)
	{
		return false;
	}

	const FString DataTableObjectPath = Config.TargetDataTable.ToSoftObjectPath().ToString();
	if (DataTableObjectPath.IsEmpty())
	{
		return false;
	}

	OutDataTable = Cast<UDataTable>(EditorAssetSubsystem->LoadAsset(DataTableObjectPath));
	return OutDataTable != nullptr;
}

bool UTcsDefinitionEditorManagerSubsystem::LoadManagedDefAsset(const FString& DefAssetObjectPath, UPrimaryDataAsset*& OutDefAsset) const
{
	OutDefAsset = nullptr;

	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	if (!EditorAssetSubsystem)
	{
		return false;
	}

	OutDefAsset = Cast<UPrimaryDataAsset>(EditorAssetSubsystem->LoadAsset(DefAssetObjectPath));
	return OutDefAsset != nullptr;
}

bool UTcsDefinitionEditorManagerSubsystem::ExtractRowDataFromDataTable(const UDataTable& DataTable, const FName RowName, FInstancedStruct& OutRowData) const
{
	OutRowData.Reset();

	const uint8* const* const RowDataPtr = DataTable.GetRowMap().Find(RowName);
	if (!RowDataPtr || !*RowDataPtr || !DataTable.GetRowStruct())
	{
		return false;
	}

	OutRowData.InitializeAs(DataTable.GetRowStruct(), *RowDataPtr);
	return true;
}

bool UTcsDefinitionEditorManagerSubsystem::UpsertDataTableRow(UDataTable& DataTable, const FName RowName, const FInstancedStruct& RowData) const
{
	if (RowName.IsNone() || !RowData.IsValid() || DataTable.GetRowStruct() != RowData.GetScriptStruct())
	{
		return false;
	}

	FDataTableEditorUtils::BroadcastPreChange(&DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	DataTable.AddRow(RowName, RowData.GetMemory(), RowData.GetScriptStruct());
	FDataTableEditorUtils::BroadcastPostChange(&DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	return true;
}

bool UTcsDefinitionEditorManagerSubsystem::MarkLoadedAssetDirty(UObject* AssetObject) const
{
	if (!AssetObject)
	{
		return false;
	}

	AssetObject->MarkPackageDirty();
	return true;
}

bool UTcsDefinitionEditorManagerSubsystem::SaveLoadedAsset(UObject* AssetObject) const
{
	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	return EditorAssetSubsystem && AssetObject && EditorAssetSubsystem->SaveLoadedAsset(AssetObject, true);
}

bool UTcsDefinitionEditorManagerSubsystem::DeleteLoadedAsset(UObject* AssetObject)
{
	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	return EditorAssetSubsystem && AssetObject && EditorAssetSubsystem->DeleteLoadedAsset(AssetObject);
}

bool UTcsDefinitionEditorManagerSubsystem::EnsureManagedDirectoryExists(const FString& ManagedDirectoryPath) const
{
	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	if (!EditorAssetSubsystem || ManagedDirectoryPath.IsEmpty())
	{
		return false;
	}

	return EditorAssetSubsystem->DoesDirectoryExist(ManagedDirectoryPath)
		|| EditorAssetSubsystem->MakeDirectory(ManagedDirectoryPath);
}

void UTcsDefinitionEditorManagerSubsystem::RebuildManagedDefAssetBindings()
{
	CachedDefAssetBindings.Reset();

	const UTcsDeveloperSettings* const Settings = GetDeveloperSettings();
	UEditorAssetSubsystem* const EditorAssetSubsystem = GetEditorAssetSubsystem();
	if (!Settings || !EditorAssetSubsystem || !Settings->bEnableDataTableAutoSync)
	{
		return;
	}

	for (const FTcsDataTableSyncConfig& Config : Settings->DataTableSyncConfigs)
	{
		if (!ShouldUseSyncConfig(*Settings, Config))
		{
			continue;
		}

		TArray<FAssetData> ManagedAssetDataList;
		CollectManagedDefAssets(Config, ManagedAssetDataList);

		for (const FAssetData& AssetData : ManagedAssetDataList)
		{
			const FString DefAssetObjectPath = AssetData.GetSoftObjectPath().ToString();
			UPrimaryDataAsset* const DefAsset = Cast<UPrimaryDataAsset>(EditorAssetSubsystem->LoadAsset(DefAssetObjectPath));
			if (!DefAsset || !DefAsset->IsA(Config.DefAssetClass.Get()))
			{
				continue;
			}

			FName DefId = NAME_None;
			if (TryGetDefAssetSyncId(DefAsset, DefId) && !DefId.IsNone())
			{
				CacheDefAssetBinding(DefAssetObjectPath, DefId, Config);
			}
		}
	}
}

void UTcsDefinitionEditorManagerSubsystem::RefreshDefinitionRegistry() const
{
	if (UTcsDefinitionRegistrySubsystem* const DefinitionRegistry = GEditor ? GEditor->GetEditorSubsystem<UTcsDefinitionRegistrySubsystem>() : nullptr)
	{
		DefinitionRegistry->RequestRefresh();
	}
}

const UTcsDeveloperSettings* UTcsDefinitionEditorManagerSubsystem::GetDeveloperSettings() const
{
	return GetDefault<UTcsDeveloperSettings>();
}

UEditorAssetSubsystem* UTcsDefinitionEditorManagerSubsystem::GetEditorAssetSubsystem() const
{
	return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
}

const FTcsDataTableSyncConfig* UTcsDefinitionEditorManagerSubsystem::FindSyncConfigByDataTablePath(const FString& DataTableObjectPath) const
{
	const UTcsDeveloperSettings* const Settings = GetDeveloperSettings();
	if (!Settings || !Settings->bEnableDataTableAutoSync)
	{
		return nullptr;
	}

	const FString NormalizedDataTablePath = NormalizeObjectPath(DataTableObjectPath);
	for (const FTcsDataTableSyncConfig& Config : Settings->DataTableSyncConfigs)
	{
		if (!ShouldUseSyncConfig(*Settings, Config))
		{
			continue;
		}

		if (Config.TargetDataTable.ToSoftObjectPath().ToString() == NormalizedDataTablePath)
		{
			return &Config;
		}
	}

	return nullptr;
}

const FTcsDataTableSyncConfig* UTcsDefinitionEditorManagerSubsystem::FindSyncConfigByDefAssetPath(const FString& DefAssetObjectPath) const
{
	const UTcsDeveloperSettings* const Settings = GetDeveloperSettings();
	if (!Settings || !Settings->bEnableDataTableAutoSync)
	{
		return nullptr;
	}

	const FString NormalizedDefAssetPath = NormalizeObjectPath(DefAssetObjectPath);
	for (const FTcsDataTableSyncConfig& Config : Settings->DataTableSyncConfigs)
	{
		if (!ShouldUseSyncConfig(*Settings, Config))
		{
			continue;
		}

		if (IsObjectPathUnderDirectory(NormalizedDefAssetPath, Config.ManagedDefAssetDirectory.Path))
		{
			return &Config;
		}
	}

	return nullptr;
}

bool UTcsDefinitionEditorManagerSubsystem::CollectManagedDefAssets(const FTcsDataTableSyncConfig& Config, TArray<FAssetData>& OutAssetData) const
{
	OutAssetData.Reset();

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		return false;
	}

	const FString ManagedDirectoryPath = NormalizeContentDirectoryPath(Config.ManagedDefAssetDirectory.Path);
	if (ManagedDirectoryPath.IsEmpty())
	{
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	return AssetRegistryModule.Get().GetAssetsByPath(FName(*ManagedDirectoryPath), OutAssetData, true, false);
}

void UTcsDefinitionEditorManagerSubsystem::CacheDefAssetBinding(const FString& DefAssetObjectPath, const FName DefId, const FTcsDataTableSyncConfig& Config)
{
	FTcsCachedDefAssetBinding& Binding = CachedDefAssetBindings.FindOrAdd(NormalizeObjectPath(DefAssetObjectPath));
	Binding.DefId = DefId;
	Binding.TargetDataTable = Config.TargetDataTable.ToSoftObjectPath();
}

void UTcsDefinitionEditorManagerSubsystem::RemoveCachedDefAssetBinding(const FString& DefAssetObjectPath)
{
	CachedDefAssetBindings.Remove(NormalizeObjectPath(DefAssetObjectPath));
	PendingRemovedSnapshots.Remove(NormalizeObjectPath(DefAssetObjectPath));
}

bool UTcsDefinitionEditorManagerSubsystem::TryBuildRemovedSnapshot(const FString& DefAssetObjectPath, FTcsRemovedDefAssetSnapshot& OutSnapshot) const
{
	const FString NormalizedDefAssetPath = NormalizeObjectPath(DefAssetObjectPath);
	if (const FTcsRemovedDefAssetSnapshot* const ExistingSnapshot = PendingRemovedSnapshots.Find(NormalizedDefAssetPath))
	{
		OutSnapshot = *ExistingSnapshot;
		return true;
	}

	if (const FTcsCachedDefAssetBinding* const CachedBinding = CachedDefAssetBindings.Find(NormalizedDefAssetPath))
	{
		OutSnapshot.ObjectPath = NormalizedDefAssetPath;
		OutSnapshot.DefId = CachedBinding->DefId;
		OutSnapshot.TargetDataTable = CachedBinding->TargetDataTable;
		return !OutSnapshot.DefId.IsNone() && !OutSnapshot.TargetDataTable.IsNull();
	}

	const FTcsDataTableSyncConfig* const Config = FindSyncConfigByDefAssetPath(NormalizedDefAssetPath);
	if (!Config)
	{
		return false;
	}

	OutSnapshot.ObjectPath = NormalizedDefAssetPath;
	OutSnapshot.TargetDataTable = Config->TargetDataTable.ToSoftObjectPath();
	return !OutSnapshot.TargetDataTable.IsNull();
}

void UTcsDefinitionEditorManagerSubsystem::TrackSuppressedDefAssetRemoval(const FString& DefAssetObjectPath)
{
	SuppressedDefAssetRemovalExpirations.Add(NormalizeObjectPath(DefAssetObjectPath), FPlatformTime::Seconds() + DefAssetRemovalSuppressionSeconds);
}

bool UTcsDefinitionEditorManagerSubsystem::ConsumeSuppressedDefAssetRemoval(const FString& DefAssetObjectPath)
{
	const FString NormalizedDefAssetPath = NormalizeObjectPath(DefAssetObjectPath);
	const double Now = FPlatformTime::Seconds();

	for (auto It = SuppressedDefAssetRemovalExpirations.CreateIterator(); It; ++It)
	{
		if (It.Value() <= Now)
		{
			It.RemoveCurrent();
		}
	}

	if (const double* const Expiration = SuppressedDefAssetRemovalExpirations.Find(NormalizedDefAssetPath))
	{
		if (*Expiration > Now)
		{
			return true;
		}

		SuppressedDefAssetRemovalExpirations.Remove(NormalizedDefAssetPath);
	}

	return false;
}
