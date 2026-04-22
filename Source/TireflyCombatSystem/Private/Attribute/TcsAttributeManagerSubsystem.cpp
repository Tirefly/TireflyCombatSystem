// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeManagerSubsystem.h"

#include "TcsDeveloperSettings.h"
#include "TcsEntityInterface.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttribute.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Attribute/TcsAttributeDefinitionAsset.h"
#include "Attribute/TcsAttributeModifierDefinitionAsset.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"

#if !WITH_EDITOR
#include "Engine/AssetManager.h"
#endif


void UTcsAttributeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	LoadFromDeveloperSettings();
#else
	LoadFromAssetManager();
#endif
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

	// 从 DeveloperSettings 的缓存中加载属性定义
	AttributeDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedAttributeDefinitions())
	{
		const UTcsAttributeDefinitionAsset* Asset = Pair.Value.LoadSynchronous();
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

	// 从 DeveloperSettings 的缓存中加载属性修改器定义
	AttributeModifierDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedAttributeModifierDefinitions())
	{
		const UTcsAttributeModifierDefinitionAsset* Asset = Pair.Value.LoadSynchronous();
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

	// 构建 AttributeTag -> AttributeName 映射
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	for (const auto& Pair : AttributeDefinitions)
	{
		const FName& AttributeName = Pair.Key;
		const UTcsAttributeDefinitionAsset* AttrDef = Pair.Value;

		if (!AttrDef)
		{
			continue;
		}

		// 检查 AttributeTag 是否有效
		if (!AttrDef->AttributeTag.IsValid())
		{
			// 空或无效 Tag，记录 Warning 但不影响运行
			if (AttrDef->AttributeTag != FGameplayTag::EmptyTag)
			{
				UE_LOG(LogTcsAttribute, Warning,
					TEXT("[%s] Attribute '%s' has invalid AttributeTag, skipping Tag mapping"),
					*FString(__FUNCTION__),
					*AttributeName.ToString());
			}
			continue;
		}

		// 检查是否重复
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

		// 添加到映射
		AttributeTagToName.Add(AttrDef->AttributeTag, AttributeName);
		AttributeNameToTag.Add(AttributeName, AttrDef->AttributeTag);
	}

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Initialized: %d Attributes, %d AttributeModifiers, %d Tag mappings"),
		*FString(__FUNCTION__),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		AttributeTagToName.Num());
}

bool UTcsAttributeManagerSubsystem::AddAttribute(
	AActor* CombatEntity,
	FName AttributeName,
	float InitValue)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->AddAttribute(AttributeName, InitValue);
}

void UTcsAttributeManagerSubsystem::AddAttributes(AActor* CombatEntity, const TArray<FName>& AttributeNames)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->AddAttributes(AttributeNames);
}

bool UTcsAttributeManagerSubsystem::AddAttributeByTag(
	AActor* CombatEntity,
	const FGameplayTag& AttributeTag,
	float InitValue)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->AddAttributeByTag(AttributeTag, InitValue);
}

bool UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag(
	const FGameplayTag& AttributeTag,
	FName& OutAttributeName) const
{
	if (!AttributeTag.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Invalid AttributeTag provided"),
			*FString(__FUNCTION__));
		return false;
	}

	const FName* FoundName = AttributeTagToName.Find(AttributeTag);
	if (FoundName)
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
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Invalid AttributeName provided"),
			*FString(__FUNCTION__));
		return false;
	}

	const FGameplayTag* FoundTag = AttributeNameToTag.Find(AttributeName);
	if (FoundTag)
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

bool UTcsAttributeManagerSubsystem::SetAttributeBaseValue(
	AActor* CombatEntity,
	FName AttributeName,
	float NewValue,
	bool bTriggerEvents)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->SetAttributeBaseValue(AttributeName, NewValue, bTriggerEvents);
}

bool UTcsAttributeManagerSubsystem::SetAttributeCurrentValue(
	AActor* CombatEntity,
	FName AttributeName,
	float NewValue,
	bool bTriggerEvents)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->SetAttributeCurrentValue(AttributeName, NewValue, bTriggerEvents);
}

bool UTcsAttributeManagerSubsystem::ResetAttribute(
	AActor* CombatEntity,
	FName AttributeName)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->ResetAttribute(AttributeName);
}

bool UTcsAttributeManagerSubsystem::RemoveAttribute(
	AActor* CombatEntity,
	FName AttributeName)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->RemoveAttribute(AttributeName);
}

UTcsAttributeComponent* UTcsAttributeManagerSubsystem::GetAttributeComponent(const AActor* CombatEntity)
{
	if (!IsValid(CombatEntity))
	{
		return nullptr;
	}

	if (CombatEntity->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetAttributeComponent(CombatEntity);
	}

	return CombatEntity->FindComponentByClass<UTcsAttributeComponent>();
}

bool UTcsAttributeManagerSubsystem::CreateAttributeModifier(
	FName ModifierId,
	AActor* Instigator,
	AActor* Target,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(Target);
	if (!IsValid(AC)) return false;
	return AC->CreateAttributeModifier(ModifierId, Instigator, OutModifierInst);
}

bool UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands(
	FName ModifierId,
	AActor* Instigator,
	AActor* Target,
	const TMap<FName, float>& Operands,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(Target);
	if (!IsValid(AC)) return false;
	return AC->CreateAttributeModifierWithOperands(ModifierId, Instigator, Operands, OutModifierInst);
}

void UTcsAttributeManagerSubsystem::ApplyModifier(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->ApplyModifier(Modifiers);
}

bool UTcsAttributeManagerSubsystem::ApplyModifierWithSourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle,
	const TArray<FName>& ModifierIds,
	TArray<FTcsAttributeModifierInstance>& OutModifiers)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->ApplyModifierWithSourceHandle(SourceHandle, ModifierIds, OutModifiers);
}

void UTcsAttributeManagerSubsystem::RemoveModifier(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->RemoveModifier(Modifiers);
}

bool UTcsAttributeManagerSubsystem::RemoveModifiersBySourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->RemoveModifiersBySourceHandle(SourceHandle);
}

bool UTcsAttributeManagerSubsystem::GetModifiersBySourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsAttributeModifierInstance>& OutModifiers) const
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return false;
	return AC->GetModifiersBySourceHandle(SourceHandle, OutModifiers);
}

void UTcsAttributeManagerSubsystem::HandleModifierUpdated(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->HandleModifierUpdated(Modifiers);
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeBaseValues(
	const AActor* CombatEntity,
	const TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->RecalculateAttributeBaseValues(Modifiers);
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeCurrentValues(const AActor* CombatEntity, int64 ChangeBatchId)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->RecalculateAttributeCurrentValues(ChangeBatchId);
}

void UTcsAttributeManagerSubsystem::MergeAttributeModifiers(
	const AActor* CombatEntity,
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	UTcsAttributeComponent* AC = GetAttributeComponent(CombatEntity);
	if (!IsValid(AC)) return;
	AC->MergeAttributeModifiers(Modifiers, MergedModifiers);
}

void UTcsAttributeManagerSubsystem::ClampAttributeValueInRange(
	UTcsAttributeComponent* AttributeComponent,
	const FName& AttributeName,
	float& NewValue,
	float* OutMinValue,
	float* OutMaxValue,
	const TMap<FName, float>* WorkingValues)
{
	if (!IsValid(AttributeComponent)) return;
	AttributeComponent->ClampAttributeValueInRange(AttributeName, NewValue, OutMinValue, OutMaxValue, WorkingValues);
}

void UTcsAttributeManagerSubsystem::EnforceAttributeRangeConstraints(UTcsAttributeComponent* AttributeComponent)
{
	if (!IsValid(AttributeComponent)) return;
	AttributeComponent->EnforceAttributeRangeConstraints();
}

FTcsSourceHandle UTcsAttributeManagerSubsystem::CreateSourceHandle(
	const TArray<FPrimaryAssetId>& CausalityChain,
	AActor* Instigator,
	const FGameplayTagContainer& SourceTags)
{
	// 生成全局唯一ID
	++GlobalSourceHandleIdMgr;

	// 创建并返回SourceHandle
	return FTcsSourceHandle(GlobalSourceHandleIdMgr, CausalityChain, Instigator, SourceTags);
}

const UTcsAttributeDefinitionAsset* UTcsAttributeManagerSubsystem::GetAttributeDefinitionAsset(FName AttributeName) const
{
	if (const UTcsAttributeDefinitionAsset* const* Found = AttributeDefinitions.Find(AttributeName))
	{
		return *Found;
	}
	return nullptr;
}

const UTcsAttributeModifierDefinitionAsset* UTcsAttributeManagerSubsystem::GetModifierDefinitionAsset(FName ModifierId) const
{
	if (const UTcsAttributeModifierDefinitionAsset* const* Found = AttributeModifierDefinitions.Find(ModifierId))
	{
		return *Found;
	}
	return nullptr;
}

void UTcsAttributeManagerSubsystem::LoadFromAssetManager()
{
#if !WITH_EDITOR
	UAssetManager& AssetManager = UAssetManager::Get();

	// 加载属性定义
	AttributeDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> AttributeDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsAttributeDefinitionAsset::PrimaryAssetType, AttributeDefIds);

		for (const FPrimaryAssetId& AssetId : AttributeDefIds)
		{
			const UTcsAttributeDefinitionAsset* Asset = Cast<UTcsAttributeDefinitionAsset>(
				AssetManager.LoadPrimaryAsset(AssetId));

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

	// 加载属性修改器定义
	AttributeModifierDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> ModifierDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsAttributeModifierDefinitionAsset::PrimaryAssetType, ModifierDefIds);

		for (const FPrimaryAssetId& AssetId : ModifierDefIds)
		{
			const UTcsAttributeModifierDefinitionAsset* Asset = Cast<UTcsAttributeModifierDefinitionAsset>(
				AssetManager.LoadPrimaryAsset(AssetId));

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

	// 构建 AttributeTag -> AttributeName 映射
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	for (const auto& Pair : AttributeDefinitions)
	{
		const FName& AttributeName = Pair.Key;
		const UTcsAttributeDefinitionAsset* AttrDef = Pair.Value;

		if (!AttrDef || !AttrDef->AttributeTag.IsValid())
		{
			continue;
		}

		// 检查重复 Tag
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

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Built %d AttributeTag mappings"),
		*FString(__FUNCTION__),
		AttributeTagToName.Num());
#endif
}
