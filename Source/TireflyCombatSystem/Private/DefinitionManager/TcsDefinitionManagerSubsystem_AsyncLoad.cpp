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

	StartAsyncLoad(BuffDefinitionSources, BuffDefId, TEXT("LoadBuffDefinitionAsync"), Callback);
}

void UTcsDefinitionManagerSubsystem::LoadSkillDefinitionAsync(FName SkillDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsSkillDefinition>* Found = SkillDefinitions.Find(SkillDefId))
	{
		Callback.ExecuteIfBound(SkillDefId, true, *Found);
		return;
	}

	StartAsyncLoad(SkillDefinitionSources, SkillDefId, TEXT("LoadSkillDefinitionAsync"), Callback);
}

void UTcsDefinitionManagerSubsystem::LoadStateSlotDefinitionAsync(FName StateSlotDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsStateSlotDefinition>* Found = StateSlotDefinitions.Find(StateSlotDefId))
	{
		Callback.ExecuteIfBound(StateSlotDefId, true, *Found);
		return;
	}

	StartAsyncLoad(StateSlotDefinitionSources, StateSlotDefId, TEXT("LoadStateSlotDefinitionAsync"), Callback);
}

void UTcsDefinitionManagerSubsystem::LoadAttributeDefinitionAsync(FName AttributeDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsAttributeDefinition>* Found = AttributeDefinitions.Find(AttributeDefId))
	{
		Callback.ExecuteIfBound(AttributeDefId, true, *Found);
		return;
	}

	StartAsyncLoad(AttributeDefinitionSources, AttributeDefId, TEXT("LoadAttributeDefinitionAsync"), Callback);
}

void UTcsDefinitionManagerSubsystem::LoadAttributeModifierDefinitionAsync(FName AttributeModifierDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsAttributeModifierDefinition>* Found = AttributeModifierDefinitions.Find(AttributeModifierDefId))
	{
		Callback.ExecuteIfBound(AttributeModifierDefId, true, *Found);
		return;
	}

	StartAsyncLoad(AttributeModifierDefinitionSources, AttributeModifierDefId, TEXT("LoadAttributeModifierDefinitionAsync"), Callback);
}

void UTcsDefinitionManagerSubsystem::LoadSkillModifierDefinitionAsync(FName SkillModifierDefId, const FOnTcsDefinitionAsyncLoaded& Callback)
{
	if (const TObjectPtr<UTcsSkillModifierDefinition>* Found = SkillModifierDefinitions.Find(SkillModifierDefId))
	{
		Callback.ExecuteIfBound(SkillModifierDefId, true, *Found);
		return;
	}

	StartAsyncLoad(SkillModifierDefinitionSources, SkillModifierDefId, TEXT("LoadSkillModifierDefinitionAsync"), Callback);
}

void UTcsDefinitionManagerSubsystem::StartAsyncLoad(
	const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
	FName DefId,
	FName EntryName,
	const FOnTcsDefinitionAsyncLoaded& Callback)
{
	const FTcsDefinitionSourceEntry* Entry = SourceCache.Find(DefId);
	if (!Entry)
	{
		LogDefinitionQueryFailure(DefId, *EntryName.ToString(), TEXT("NotRegistered"));
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
		FStreamableDelegate::CreateLambda([WeakThis, AssetId, DefId, EntryName]()
		{
			if (UTcsDefinitionManagerSubsystem* Self = WeakThis.Get())
			{
				TArray<FOnTcsDefinitionAsyncLoaded> Callbacks;
				Self->PendingAsyncLoads.RemoveAndCopyValue(AssetId, Callbacks);
				Self->OnAsyncDefinitionLoaded(AssetId, DefId, EntryName, MoveTemp(Callbacks));
			}
		}));
}

void UTcsDefinitionManagerSubsystem::OnAsyncDefinitionLoaded(
	FPrimaryAssetId AssetId,
	FName DefId,
	FName EntryName,
	TArray<FOnTcsDefinitionAsyncLoaded> Callbacks)
{
	UPrimaryDataAsset* Asset = UAssetManager::Get().GetPrimaryAssetObject<UPrimaryDataAsset>(AssetId);
	bool bSuccess = false;

	if (Asset)
	{
		bSuccess = WriteLoadedAssetToCache(AssetId, Asset);
		if (!bSuccess)
		{
			LogDefinitionQueryFailure(DefId, *EntryName.ToString(), TEXT("TypeMismatch"));
		}
	}
	else
	{
		LogDefinitionQueryFailure(DefId, *EntryName.ToString(), TEXT("LoadFailed"));
	}

	for (const FOnTcsDefinitionAsyncLoaded& Callback : Callbacks)
	{
		Callback.ExecuteIfBound(DefId, bSuccess, bSuccess ? Asset : nullptr);
	}
}

bool UTcsDefinitionManagerSubsystem::WriteLoadedAssetToCache(const FPrimaryAssetId& AssetId, UPrimaryDataAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	if (AssetId.PrimaryAssetType == UTcsBuffDefinition::PrimaryAssetType)
	{
		if (UTcsBuffDefinition* Def = Cast<UTcsBuffDefinition>(Asset))
		{
			const FName DefId = Def->StateDefId;
			BuffDefinitions.Add(DefId, Def);
			if (!AmbiguousStateDefinitionIds.Contains(DefId))
			{
				StateDefinitions.Add(DefId, Def);
			}
			if (Def->StateTag.IsValid())
			{
				BuffTagToDefId.Add(Def->StateTag, DefId);
			}
			return true;
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsSkillDefinition::PrimaryAssetType)
	{
		if (UTcsSkillDefinition* Def = Cast<UTcsSkillDefinition>(Asset))
		{
			const FName DefId = Def->StateDefId;
			SkillDefinitions.Add(DefId, Def);
			if (!AmbiguousStateDefinitionIds.Contains(DefId))
			{
				StateDefinitions.Add(DefId, Def);
			}
			return true;
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
			return true;
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsAttributeDefinition::PrimaryAssetType)
	{
		if (UTcsAttributeDefinition* Def = Cast<UTcsAttributeDefinition>(Asset))
		{
			AttributeDefinitions.Add(Def->AttributeDefId, Def);
			return true;
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
	{
		if (UTcsAttributeModifierDefinition* Def = Cast<UTcsAttributeModifierDefinition>(Asset))
		{
			AttributeModifierDefinitions.Add(Def->AttributeModifierDefId, Def);
			return true;
		}
	}
	else if (AssetId.PrimaryAssetType == UTcsSkillModifierDefinition::PrimaryAssetType)
	{
		if (UTcsSkillModifierDefinition* Def = Cast<UTcsSkillModifierDefinition>(Asset))
		{
			SkillModifierDefinitions.Add(Def->SkillModifierDefId, Def);
			return true;
		}
	}

	return false;
}
