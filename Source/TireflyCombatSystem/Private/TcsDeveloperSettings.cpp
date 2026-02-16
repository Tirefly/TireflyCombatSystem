// Copyright Tirefly. All Rights Reserved.


#include "TcsDeveloperSettings.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Attribute/TcsAttributeDefinitionAsset.h"
#include "Attribute/TcsAttributeModifierDefinitionAsset.h"
#include "State/TcsStateDefinitionAsset.h"
#include "State/TcsStateSlotDefinitionAsset.h"

void UTcsDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// 编辑器模式下注册 Asset Registry 监听并触发初始扫描
	if (GIsEditor && !IsTemplate())
	{
		ScanAndCacheDefinitions();
		RegisterAssetRegistryCallbacks();
	}
}

void UTcsDeveloperSettings::ScanAndCacheDefinitions()
{
	// 清空现有缓存
	CachedAttributeDefinitions.Empty();
	CachedStateDefinitions.Empty();
	CachedStateSlotDefinitions.Empty();
	CachedAttributeModifierDefinitions.Empty();

	// 获取 AssetManager 配置
	const UAssetManagerSettings* AssetManagerSettings = GetDefault<UAssetManagerSettings>();
	if (!AssetManagerSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("[UTcsDeveloperSettings::ScanAndCacheDefinitions] Failed to get AssetManagerSettings"));
		return;
	}

	// 获取 Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 扫描每个 PrimaryAssetType
	for (const FPrimaryAssetTypeInfo& TypeInfo : AssetManagerSettings->PrimaryAssetTypesToScan)
	{
		// 遍历配置的目录
		TArray<FDirectoryPath> Directories = TypeInfo.GetDirectories();
		for (const FDirectoryPath& Directory : Directories)
		{
			// 扫描目录中的资产
			TArray<FAssetData> AssetDataList;
			AssetRegistry.GetAssetsByPath(FName(*Directory.Path), AssetDataList, true);

			// 根据 PrimaryAssetType 分类处理
			if (TypeInfo.PrimaryAssetType == UTcsAttributeDefinitionAsset::PrimaryAssetType)
			{
				ScanAttributeDefinitions(AssetDataList);
			}
			else if (TypeInfo.PrimaryAssetType == UTcsStateDefinitionAsset::PrimaryAssetType)
			{
				ScanStateDefinitions(AssetDataList);
			}
			else if (TypeInfo.PrimaryAssetType == UTcsStateSlotDefinitionAsset::PrimaryAssetType)
			{
				ScanStateSlotDefinitions(AssetDataList);
			}
			else if (TypeInfo.PrimaryAssetType == UTcsAttributeModifierDefinitionAsset::PrimaryAssetType)
			{
				ScanAttributeModifierDefinitions(AssetDataList);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[UTcsDeveloperSettings::ScanAndCacheDefinitions] Scan complete: %d Attributes, %d States, %d StateSlots, %d Modifiers"),
		CachedAttributeDefinitions.Num(),
		CachedStateDefinitions.Num(),
		CachedStateSlotDefinitions.Num(),
		CachedAttributeModifierDefinitions.Num());
}

void UTcsDeveloperSettings::ScanAttributeDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetClassPath == UTcsAttributeDefinitionAsset::StaticClass()->GetClassPathName())
		{
			TSoftObjectPtr<UTcsAttributeDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());

			// 同步加载以获取 DefId（编辑器模式下可以接受）
			if (const UTcsAttributeDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
			{
				CachedAttributeDefinitions.Add(Asset->AttributeDefId, AssetPtr);
			}
		}
	}
}

void UTcsDeveloperSettings::ScanStateDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetClassPath == UTcsStateDefinitionAsset::StaticClass()->GetClassPathName())
		{
			TSoftObjectPtr<UTcsStateDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());

			if (const UTcsStateDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
			{
				CachedStateDefinitions.Add(Asset->StateDefId, AssetPtr);
			}
		}
	}
}

void UTcsDeveloperSettings::ScanStateSlotDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetClassPath == UTcsStateSlotDefinitionAsset::StaticClass()->GetClassPathName())
		{
			TSoftObjectPtr<UTcsStateSlotDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());

			if (const UTcsStateSlotDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
			{
				CachedStateSlotDefinitions.Add(Asset->StateSlotDefId, AssetPtr);
			}
		}
	}
}

void UTcsDeveloperSettings::ScanAttributeModifierDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetClassPath == UTcsAttributeModifierDefinitionAsset::StaticClass()->GetClassPathName())
		{
			TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());

			if (const UTcsAttributeModifierDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
			{
				CachedAttributeModifierDefinitions.Add(Asset->AttributeModifierDefId, AssetPtr);
			}
		}
	}
}

void UTcsDeveloperSettings::RegisterAssetRegistryCallbacks()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 监听资产添加
	AssetRegistry.OnAssetAdded().AddUObject(this, &UTcsDeveloperSettings::OnAssetAdded);

	// 监听资产删除
	AssetRegistry.OnAssetRemoved().AddUObject(this, &UTcsDeveloperSettings::OnAssetRemoved);

	// 监听资产重命名
	AssetRegistry.OnAssetRenamed().AddUObject(this, &UTcsDeveloperSettings::OnAssetRenamed);
}

void UTcsDeveloperSettings::OnAssetAdded(const FAssetData& AssetData)
{
	// 检查是否是我们关心的资产类型
	if (AssetData.AssetClassPath == UTcsAttributeDefinitionAsset::StaticClass()->GetClassPathName())
	{
		TSoftObjectPtr<UTcsAttributeDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());
		if (const UTcsAttributeDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
		{
			CachedAttributeDefinitions.Add(Asset->AttributeDefId, AssetPtr);
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Added AttributeDef: %s"), *Asset->AttributeDefId.ToString());
		}
	}
	else if (AssetData.AssetClassPath == UTcsStateDefinitionAsset::StaticClass()->GetClassPathName())
	{
		TSoftObjectPtr<UTcsStateDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());
		if (const UTcsStateDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
		{
			CachedStateDefinitions.Add(Asset->StateDefId, AssetPtr);
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Added StateDef: %s"), *Asset->StateDefId.ToString());
		}
	}
	else if (AssetData.AssetClassPath == UTcsStateSlotDefinitionAsset::StaticClass()->GetClassPathName())
	{
		TSoftObjectPtr<UTcsStateSlotDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());
		if (const UTcsStateSlotDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
		{
			CachedStateSlotDefinitions.Add(Asset->StateSlotDefId, AssetPtr);
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Added StateSlotDef: %s"), *Asset->StateSlotDefId.ToString());
		}
	}
	else if (AssetData.AssetClassPath == UTcsAttributeModifierDefinitionAsset::StaticClass()->GetClassPathName())
	{
		TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset> AssetPtr(AssetData.ToSoftObjectPath());
		if (const UTcsAttributeModifierDefinitionAsset* Asset = AssetPtr.LoadSynchronous())
		{
			CachedAttributeModifierDefinitions.Add(Asset->AttributeModifierDefId, AssetPtr);
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Added AttributeModifierDef: %s"), *Asset->AttributeModifierDefId.ToString());
		}
	}
}

void UTcsDeveloperSettings::OnAssetRemoved(const FAssetData& AssetData)
{
	// 从缓存中移除（需要遍历查找匹配的路径）
	FSoftObjectPath RemovedPath = AssetData.ToSoftObjectPath();

	// 检查并移除 AttributeDef
	for (auto It = CachedAttributeDefinitions.CreateIterator(); It; ++It)
	{
		if (It.Value().ToSoftObjectPath() == RemovedPath)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Removed AttributeDef: %s"), *It.Key().ToString());
			It.RemoveCurrent();
			break;
		}
	}

	// 检查并移除 StateDef
	for (auto It = CachedStateDefinitions.CreateIterator(); It; ++It)
	{
		if (It.Value().ToSoftObjectPath() == RemovedPath)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Removed StateDef: %s"), *It.Key().ToString());
			It.RemoveCurrent();
			break;
		}
	}

	// 检查并移除 StateSlotDef
	for (auto It = CachedStateSlotDefinitions.CreateIterator(); It; ++It)
	{
		if (It.Value().ToSoftObjectPath() == RemovedPath)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Removed StateSlotDef: %s"), *It.Key().ToString());
			It.RemoveCurrent();
			break;
		}
	}

	// 检查并移除 AttributeModifierDef
	for (auto It = CachedAttributeModifierDefinitions.CreateIterator(); It; ++It)
	{
		if (It.Value().ToSoftObjectPath() == RemovedPath)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[UTcsDeveloperSettings] Removed AttributeModifierDef: %s"), *It.Key().ToString());
			It.RemoveCurrent();
			break;
		}
	}
}

void UTcsDeveloperSettings::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	// 先移除旧的，再添加新的
	FAssetData OldAssetData = AssetData;
	OldAssetData.PackageName = FName(*FPackageName::ObjectPathToPackageName(OldObjectPath));
	OnAssetRemoved(OldAssetData);
	OnAssetAdded(AssetData);
}

#endif // WITH_EDITOR
