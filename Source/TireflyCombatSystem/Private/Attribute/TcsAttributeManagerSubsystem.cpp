// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeManagerSubsystem.h"

#include "TcsDefinitionManagerSubsystem.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"



void UTcsAttributeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsRuntimeReady = false;

	Collection.InitializeDependency<UTcsDefinitionManagerSubsystem>();
	RebuildAttributeTagMappings();

	bIsRuntimeReady = true;
}

void UTcsAttributeManagerSubsystem::Deinitialize()
{
	bIsRuntimeReady = false;
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	Super::Deinitialize();
}

void UTcsAttributeManagerSubsystem::RebuildAttributeTagMappings()
{
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager();
	if (!DefinitionManager)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to resolve DefinitionManagerSubsystem"), *FString(__FUNCTION__));
		return;
	}

	for (const FName AttributeDefId : DefinitionManager->GetAllAttributeDefIds())
	{
		const UTcsAttributeDefinition* AttrDef = DefinitionManager->GetAttributeDefinition(AttributeDefId);
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
					*AttributeDefId.ToString());
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
				*AttributeDefId.ToString());
			continue;
		}

		AttributeTagToName.Add(AttrDef->AttributeTag, AttributeDefId);
		AttributeNameToTag.Add(AttributeDefId, AttrDef->AttributeTag);
	}
}

UTcsDefinitionManagerSubsystem* UTcsAttributeManagerSubsystem::ResolveDefinitionManager() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UTcsDefinitionManagerSubsystem>() : nullptr;
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
		TEXT("[%s] AttributeTag '%s' not found in mapping. Make sure the tag is registered in an AttributeDefinition."),
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
