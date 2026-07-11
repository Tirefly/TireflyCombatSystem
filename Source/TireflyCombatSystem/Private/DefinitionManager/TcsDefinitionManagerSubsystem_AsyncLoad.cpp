// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Engine/AssetManager.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateSlotDefinition.h"



void UTcsDefinitionManagerSubsystem::LoadBuffDefinitionAsync(FName BuffDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsBuffDefinition>* Found = BuffDefinitions.Find(BuffDefId))
	{
		Callback.ExecuteIfBound(BuffDefId, true, *Found);
		return;
	}

	StartAsyncLoad(BuffDefinitionSources, BuffDefId, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadSkillDefinitionAsync(FName SkillDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsSkillDefinition>* Found = SkillDefinitions.Find(SkillDefId))
	{
		Callback.ExecuteIfBound(SkillDefId, true, *Found);
		return;
	}

	StartAsyncLoad(SkillDefinitionSources, SkillDefId, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadStateSlotDefinitionAsync(FName StateSlotDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsStateSlotDefinition>* Found = StateSlotDefinitions.Find(StateSlotDefId))
	{
		Callback.ExecuteIfBound(StateSlotDefId, true, *Found);
		return;
	}

	StartAsyncLoad(StateSlotDefinitionSources, StateSlotDefId, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadAttributeDefinitionAsync(FName AttributeDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsAttributeDefinition>* Found = AttributeDefinitions.Find(AttributeDefId))
	{
		Callback.ExecuteIfBound(AttributeDefId, true, *Found);
		return;
	}

	StartAsyncLoad(AttributeDefinitionSources, AttributeDefId, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadAttributeModifierDefinitionAsync(FName AttributeModifierDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsAttributeModifierDefinition>* Found = AttributeModifierDefinitions.Find(AttributeModifierDefId))
	{
		Callback.ExecuteIfBound(AttributeModifierDefId, true, *Found);
		return;
	}

	StartAsyncLoad(AttributeModifierDefinitionSources, AttributeModifierDefId, Callback);
}

void UTcsDefinitionManagerSubsystem::LoadSkillModifierDefinitionAsync(FName SkillModifierDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsSkillModifierDefinition>* Found = SkillModifierDefinitions.Find(SkillModifierDefId))
	{
		Callback.ExecuteIfBound(SkillModifierDefId, true, *Found);
		return;
	}

	StartAsyncLoad(SkillModifierDefinitionSources, SkillModifierDefId, Callback);
}

void UTcsDefinitionManagerSubsystem::StartAsyncLoad(
	const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
	FName DefId,
	const FOnTcsDefinitionAsyncLoaded& Callback)
{
	const FTcsDefinitionSourceEntry* Entry = SourceCache.Find(DefId);
	if (!Entry)
	{
		Callback.ExecuteIfBound(DefId, false, nullptr);
		return;
	}

	// 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播
	if (TArray<FOnTcsDefinitionAsyncLoaded>* Pending = PendingAsyncLoads.Find(Entry->AssetId))
	{
		Pending->Add(Callback);
		return;
	}

	PendingAsyncLoads.Add(Entry->AssetId, {Callback});

	const FPrimaryAssetId AssetId = Entry->AssetId;
	TWeakObjectPtr<UTcsDefinitionManagerSubsystem> WeakThis(this);
	UAssetManager::Get().LoadPrimaryAsset(AssetId, {},
		FStreamableDelegate::CreateLambda([WeakThis, AssetId, DefId]()
		{
			if (UTcsDefinitionManagerSubsystem* Self = WeakThis.Get())
			{
				TArray<FOnTcsDefinitionAsyncLoaded> Callbacks;
				Self->PendingAsyncLoads.RemoveAndCopyValue(AssetId, Callbacks);
				Self->OnAsyncDefinitionLoaded(AssetId, DefId, MoveTemp(Callbacks));
			}
		}));
}

void UTcsDefinitionManagerSubsystem::OnAsyncDefinitionLoaded(
	FPrimaryAssetId AssetId,
	FName DefId,
	TArray<FOnTcsDefinitionAsyncLoaded> Callbacks)
{
	UPrimaryDataAsset* Asset = UAssetManager::Get().GetPrimaryAssetObject<UPrimaryDataAsset>(AssetId);
	const bool bSuccess = Asset != nullptr;

	if (Asset)
	{
		WriteLoadedAssetToCache(AssetId, Asset);
	}
	else
	{
		UE_LOG(LogTcs, Warning,
			TEXT("[UTcsDefinitionManagerSubsystem] Async load failed for DefId=%s AssetId=%s"),
			*DefId.ToString(),
			*AssetId.ToString());
	}

	for (const FOnTcsDefinitionAsyncLoaded& Callback : Callbacks)
	{
		Callback.ExecuteIfBound(DefId, bSuccess, Asset);
	}
}

void UTcsDefinitionManagerSubsystem::WriteLoadedAssetToCache(const FPrimaryAssetId& AssetId, UPrimaryDataAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	if (AssetId.PrimaryAssetType == UTcsBuffDefinition::PrimaryAssetType)
	{
		if (UTcsBuffDefinition* Def = Cast<UTcsBuffDefinition>(Asset))
		{
			const FName DefId = Def->StateDefId;
			BuffDefinitions.Add(DefId, Def);
			StateDefinitions.Add(DefId, Def);
			if (Def->StateTag.IsValid())
			{
				BuffTagToDefId.Add(Def->StateTag, DefId);
			}
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsSkillDefinition::PrimaryAssetType)
	{
		if (UTcsSkillDefinition* Def = Cast<UTcsSkillDefinition>(Asset))
		{
			const FName DefId = Def->StateDefId;
			SkillDefinitions.Add(DefId, Def);
			StateDefinitions.Add(DefId, Def);
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsStateSlotDefinition::PrimaryAssetType)
	{
		if (UTcsStateSlotDefinition* Def = Cast<UTcsStateSlotDefinition>(Asset))
		{
			const FName DefId = Def->StateSlotDefId;
			StateSlotDefinitions.Add(DefId, Def);
			if (Def->SlotTag.IsValid())
			{
				StateSlotTagToDefId.Add(Def->SlotTag, DefId);
			}
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsAttributeDefinition::PrimaryAssetType)
	{
		if (UTcsAttributeDefinition* Def = Cast<UTcsAttributeDefinition>(Asset))
		{
			AttributeDefinitions.Add(Def->AttributeDefId, Def);
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
	{
		if (UTcsAttributeModifierDefinition* Def = Cast<UTcsAttributeModifierDefinition>(Asset))
		{
			AttributeModifierDefinitions.Add(Def->AttributeModifierDefId, Def);
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsSkillModifierDefinition::PrimaryAssetType)
	{
		if (UTcsSkillModifierDefinition* Def = Cast<UTcsSkillModifierDefinition>(Asset))
		{
			SkillModifierDefinitions.Add(Def->ModifierId, Def);
		}
	}
}
