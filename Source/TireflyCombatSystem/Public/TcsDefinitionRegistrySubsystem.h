// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/EngineSubsystem.h"
#include "TcsDefinitionRegistrySubsystem.generated.h"

class UTcsAttributeDefinition;
class UTcsAttributeModifierDefinition;
class UTcsStateDefinition;
class UTcsStateSlotDefinition;
class UObject;
struct FAssetData;
struct FPrimaryAssetTypeInfo;
struct FPropertyChangedEvent;

DECLARE_MULTICAST_DELEGATE_OneParam(FTcsDefinitionRegistryRefreshed, const UTcsDefinitionRegistrySubsystem*);

UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsDefinitionRegistrySubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>>& GetAttributeDefinitions() const
	{
		return AttributeDefinitions;
	}

	const TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>>& GetAttributeModifierDefinitions() const
	{
		return AttributeModifierDefinitions;
	}

	const TMap<FName, TSoftObjectPtr<UTcsStateDefinition>>& GetStateDefinitions() const
	{
		return StateDefinitions;
	}

	const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>>& GetStateSlotDefinitions() const
	{
		return StateSlotDefinitions;
	}

	FTcsDefinitionRegistryRefreshed& OnDefinitionsRefreshed()
	{
		return DefinitionsRefreshed;
	}

	bool HasCompletedInitialRefresh() const
	{
		return bHasCompletedInitialRefresh;
	}

	int32 GetRefreshRevision() const
	{
		return RefreshRevision;
	}

#if WITH_EDITOR
	void RequestRefresh();
	void RefreshDefinitionsNow();
#endif

private:
#if WITH_EDITOR
	void RegisterEditorCallbacks();
	void UnregisterEditorCallbacks();
	bool HandleDeferredRefresh(float DeltaTime);
	void RebuildSnapshot();
	void MirrorSnapshotToDeveloperSettings() const;
	void ScanPrimaryAssetType(const FPrimaryAssetTypeInfo& TypeInfo, class IAssetRegistry& AssetRegistry);
	void ScanAttributeDefinitions(const TArray<FAssetData>& AssetDataList);
	void ScanAttributeModifierDefinitions(const TArray<FAssetData>& AssetDataList);
	void ScanStateDefinitions(const TArray<FAssetData>& AssetDataList);
	void ScanStateSlotDefinitions(const TArray<FAssetData>& AssetDataList);
	bool IsTrackedDefinitionClass(const FAssetData& AssetData) const;
	bool IsTrackedDefinitionObject(const UObject* AssetObject) const;
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetUpdated(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
	void OnInMemoryAssetCreated(UObject* AssetObject);
	void OnInMemoryAssetDeleted(UObject* AssetObject);
	void OnAssetManagerSettingsChanged(UObject* SettingsObject, FPropertyChangedEvent& PropertyChangedEvent);
	void ClearQueuedRefresh();

	bool bHasRegisteredEditorCallbacks = false;
	bool bHasCompletedInitialRefresh = false;
	bool bIsRefreshQueued = false;
	bool bIsRefreshing = false;
	bool bRefreshRequestedWhileRefreshing = false;
	int32 RefreshRevision = 0;
	FTSTicker::FDelegateHandle DeferredRefreshHandle;
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetUpdatedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle InMemoryAssetCreatedHandle;
	FDelegateHandle InMemoryAssetDeletedHandle;
	FDelegateHandle AssetManagerSettingsChangedHandle;
#else
	bool bHasCompletedInitialRefresh = true;
	int32 RefreshRevision = 0;
#endif

	TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>> AttributeDefinitions;
	TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>> AttributeModifierDefinitions;
	TMap<FName, TSoftObjectPtr<UTcsStateDefinition>> StateDefinitions;
	TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>> StateSlotDefinitions;
	FTcsDefinitionRegistryRefreshed DefinitionsRefreshed;
};