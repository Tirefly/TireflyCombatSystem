// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeManagerSubsystem.h"

#include "TcsDefinitionRegistrySubsystem.h"
#include "TcsDeveloperSettings.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#endif

#if !WITH_EDITOR
#include "Engine/AssetManager.h"
#endif


void UTcsAttributeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	LoadFromDefinitionRegistry();

	if (UTcsDefinitionRegistrySubsystem* Registry = GetDefinitionRegistry())
	{
		DefinitionRegistryRefreshedHandle = Registry->OnDefinitionsRefreshed().AddUObject(
			this,
			&UTcsAttributeManagerSubsystem::HandleDefinitionRegistryRefreshed);
	}
#else
	LoadFromAssetManager();
#endif
}

void UTcsAttributeManagerSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (DefinitionRegistryRefreshedHandle.IsValid())
	{
		if (UTcsDefinitionRegistrySubsystem* Registry = GetDefinitionRegistry())
		{
			Registry->OnDefinitionsRefreshed().Remove(DefinitionRegistryRefreshedHandle);
		}

		DefinitionRegistryRefreshedHandle.Reset();
	}
#endif

	Super::Deinitialize();
}

void UTcsAttributeManagerSubsystem::LoadFromDeveloperSettings()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Failed to get TcsDeveloperSettings"),
			*FString(__FUNCTION__));
		return;
	}

	AttributeDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedAttributeDefinitions())
	{
		const UTcsAttributeDefinition* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			AttributeDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (AttributeDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] No AttributeDefinitions loaded from DeveloperSettings"),
			*FString(__FUNCTION__));
		return;
	}

	AttributeModifierDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedAttributeModifierDefinitions())
	{
		const UTcsAttributeModifierDefinition* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			AttributeModifierDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeModifierDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (AttributeModifierDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] No AttributeModifierDefinitions loaded from DeveloperSettings"),
			*FString(__FUNCTION__));
	}

	RebuildAttributeTagMappings();

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Initialized: %d Attributes, %d AttributeModifiers, %d Tag mappings"),
		*FString(__FUNCTION__),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		AttributeTagToName.Num());
}

void UTcsAttributeManagerSubsystem::LoadFromDefinitionRegistry()
{
#if WITH_EDITOR
	UTcsDefinitionRegistrySubsystem* Registry = GetDefinitionRegistry();
	if (!Registry)
	{
		LoadFromDeveloperSettings();
		return;
	}

	if (!Registry->HasCompletedInitialRefresh())
	{
		Registry->RefreshDefinitionsNow();
	}

	AttributeDefinitions.Empty();
	for (const auto& Pair : Registry->GetAttributeDefinitions())
	{
		const UTcsAttributeDefinition* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			AttributeDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (AttributeDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] No AttributeDefinitions loaded from DefinitionRegistry"),
			*FString(__FUNCTION__));
	}

	AttributeModifierDefinitions.Empty();
	for (const auto& Pair : Registry->GetAttributeModifierDefinitions())
	{
		const UTcsAttributeModifierDefinition* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			AttributeModifierDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeModifierDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (AttributeModifierDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] No AttributeModifierDefinitions loaded from DefinitionRegistry"),
			*FString(__FUNCTION__));
	}
#else
	LoadFromDeveloperSettings();
#endif

	RebuildAttributeTagMappings();

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Initialized: %d Attributes, %d AttributeModifiers, %d Tag mappings"),
		*FString(__FUNCTION__),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		AttributeTagToName.Num());
}

void UTcsAttributeManagerSubsystem::RebuildAttributeTagMappings()
{

	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	for (const auto& Pair : AttributeDefinitions)
	{
		const FName& AttributeName = Pair.Key;
		const UTcsAttributeDefinition* AttrDef = Pair.Value;
		if (!AttrDef)
		{
			continue;
		}

		if (!AttrDef->AttributeTag.IsValid())
		{
			if (AttrDef->AttributeTag != FGameplayTag::EmptyTag)
			{
				UE_LOG(LogTcsAttribute, Warning,
					TEXT("[%s] Attribute '%s' has invalid AttributeTag, skipping Tag mapping"),
					*FString(__FUNCTION__),
					*AttributeName.ToString());
			}
			continue;
		}

		if (AttributeTagToName.Contains(AttrDef->AttributeTag))
		{
			const FName ExistingName = AttributeTagToName[AttrDef->AttributeTag];
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Duplicate AttributeTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
				*FString(__FUNCTION__),
				*AttrDef->AttributeTag.ToString(),
				*ExistingName.ToString(),
				*AttributeName.ToString());
			continue;
		}

		AttributeTagToName.Add(AttrDef->AttributeTag, AttributeName);
		AttributeNameToTag.Add(AttributeName, AttrDef->AttributeTag);
	}
}

bool UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag(
	const FGameplayTag& AttributeTag,
	FName& OutAttributeName) const
{
	if (!AttributeTag.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Invalid AttributeTag provided"), *FString(__FUNCTION__));
		return false;
	}

	if (const FName* FoundName = AttributeTagToName.Find(AttributeTag))
	{
		OutAttributeName = *FoundName;
		return true;
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("[%s] AttributeTag '%s' not found in mapping. Make sure the tag is registered in AttributeDefTable."),
		*FString(__FUNCTION__),
		*AttributeTag.ToString());
	return false;
}

bool UTcsAttributeManagerSubsystem::TryGetAttributeTagByName(
	FName AttributeName,
	FGameplayTag& OutAttributeTag) const
{
	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Invalid AttributeName provided"), *FString(__FUNCTION__));
		return false;
	}

	if (const FGameplayTag* FoundTag = AttributeNameToTag.Find(AttributeName))
	{
		OutAttributeTag = *FoundTag;
		return true;
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("[%s] AttributeName '%s' not found in mapping or has no AttributeTag configured."),
		*FString(__FUNCTION__),
		*AttributeName.ToString());
	return false;
}

FTcsSourceHandle UTcsAttributeManagerSubsystem::CreateSourceHandle(
	const TArray<FPrimaryAssetId>& CausalityChain,
	AActor* Instigator,
	const FGameplayTagContainer& SourceTags)
{
	++GlobalSourceHandleIdMgr;
	return FTcsSourceHandle(GlobalSourceHandleIdMgr, CausalityChain, Instigator, SourceTags);
}

const UTcsAttributeDefinition* UTcsAttributeManagerSubsystem::GetAttributeDefinition(FName AttributeName) const
{
	if (const UTcsAttributeDefinition* const* Found = AttributeDefinitions.Find(AttributeName))
	{
		return *Found;
	}
	return nullptr;
}

const UTcsAttributeModifierDefinition* UTcsAttributeManagerSubsystem::GetModifierDefinition(FName ModifierId) const
{
	if (const UTcsAttributeModifierDefinition* const* Found = AttributeModifierDefinitions.Find(ModifierId))
	{
		return *Found;
	}
	return nullptr;
}

void UTcsAttributeManagerSubsystem::LoadFromAssetManager()
{
#if !WITH_EDITOR
	UAssetManager& AssetManager = UAssetManager::Get();

	AttributeDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> AttributeDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsAttributeDefinition::PrimaryAssetType, AttributeDefIds);

		for (const FPrimaryAssetId& AssetId : AttributeDefIds)
		{
			const UTcsAttributeDefinition* Asset = Cast<UTcsAttributeDefinition>(AssetManager.LoadPrimaryAsset(AssetId));
			if (Asset)
			{
				AttributeDefinitions.Add(Asset->AttributeDefId, Asset);
			}
			else
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeDefinition: %s"),
					*FString(__FUNCTION__),
					*AssetId.ToString());
			}
		}

		UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Loaded %d AttributeDefinitions from AssetManager"),
			*FString(__FUNCTION__),
			AttributeDefinitions.Num());
	}

	AttributeModifierDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> ModifierDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsAttributeModifierDefinition::PrimaryAssetType, ModifierDefIds);

		for (const FPrimaryAssetId& AssetId : ModifierDefIds)
		{
			const UTcsAttributeModifierDefinition* Asset = Cast<UTcsAttributeModifierDefinition>(AssetManager.LoadPrimaryAsset(AssetId));
			if (Asset)
			{
				AttributeModifierDefinitions.Add(Asset->AttributeModifierDefId, Asset);
			}
			else
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeModifierDefinition: %s"),
					*FString(__FUNCTION__),
					*AssetId.ToString());
			}
		}

		UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Loaded %d AttributeModifierDefinitions from AssetManager"),
			*FString(__FUNCTION__),
			AttributeModifierDefinitions.Num());
	}

	RebuildAttributeTagMappings();

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Built %d AttributeTag mappings"),
		*FString(__FUNCTION__),
		AttributeTagToName.Num());
#endif
}

#if WITH_EDITOR
UTcsDefinitionRegistrySubsystem* UTcsAttributeManagerSubsystem::GetDefinitionRegistry() const
{
	return GEngine ? GEngine->GetEngineSubsystem<UTcsDefinitionRegistrySubsystem>() : nullptr;
}

void UTcsAttributeManagerSubsystem::HandleDefinitionRegistryRefreshed(const UTcsDefinitionRegistrySubsystem* Registry)
{
	LoadFromDefinitionRegistry();
}
#endif