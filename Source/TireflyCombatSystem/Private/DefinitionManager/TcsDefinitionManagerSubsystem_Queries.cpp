// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateSlotDefinition.h"



namespace
{
	/**
	 * 从 source cache 按需同步加载资产并写入 loaded cache。
	 */
	template <typename AssetType>
	AssetType* LoadFromSource(
		const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
		TMap<FName, TObjectPtr<AssetType>>& LoadedCache,
		FName DefinitionId)
	{
		if (const TObjectPtr<AssetType>* Found = LoadedCache.Find(DefinitionId))
		{
			return Found->Get();
		}

		const FTcsDefinitionSourceEntry* Source = SourceCache.Find(DefinitionId);
		if (!Source)
		{
			return nullptr;
		}

		UPrimaryDataAsset* Asset = Source->SoftPtr.LoadSynchronous();
		if (!Asset)
		{
			return nullptr;
		}

		AssetType* TypedAsset = Cast<AssetType>(Asset);
		if (!TypedAsset)
		{
			return nullptr;
		}

		LoadedCache.Add(DefinitionId, TypedAsset);
		return TypedAsset;
	}

	/**
	 * 从 source cache 逐条同步加载并匹配 tag。
	 */
	template <typename AssetType>
	const AssetType* LoadByTagFromSource(
		const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
		TMap<FName, TObjectPtr<AssetType>>& LoadedCache,
		TMap<FGameplayTag, FName>& TagToDefId,
		FGameplayTag Tag,
		TFunctionRef<FGameplayTag(const AssetType&)> GetTag)
	{
		for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : SourceCache)
		{
			AssetType* Asset = LoadFromSource(SourceCache, LoadedCache, Pair.Key);
			if (Asset && GetTag(*Asset) == Tag)
			{
				TagToDefId.Add(Tag, Pair.Key);
				return Asset;
			}
		}

		return nullptr;
	}
}



const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::GetBuffDefinition(FName BuffDefId) const
{
	if (const TObjectPtr<UTcsBuffDefinition>* Found = BuffDefinitions.Find(BuffDefId))
	{
		return Found->Get();
	}

	const UTcsBuffDefinition* Definition = LoadBuffDefinitionSync(BuffDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(BuffDefId, TEXT("GetBuffDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::GetBuffDefinitionByTag(FGameplayTag BuffTag) const
{
	if (const FName* BuffDefId = BuffTagToDefId.Find(BuffTag))
	{
		return GetBuffDefinition(*BuffDefId);
	}

	const UTcsBuffDefinition* Definition = LoadByTagFromSource(
		BuffDefinitionSources, BuffDefinitions, BuffTagToDefId, BuffTag,
		TFunctionRef<FGameplayTag(const UTcsBuffDefinition&)>([](const UTcsBuffDefinition& Def) { return Def.StateTag; }));

	if (!Definition)
	{
		LogDefinitionQueryFailure(BuffTag.GetTagName(), TEXT("GetBuffDefinitionByTag"), TEXT("NotRegistered"));
	}
	return Definition;
}

const UTcsSkillDefinition* UTcsDefinitionManagerSubsystem::GetSkillDefinition(FName SkillDefId) const
{
	if (const TObjectPtr<UTcsSkillDefinition>* Found = SkillDefinitions.Find(SkillDefId))
	{
		return Found->Get();
	}

	const UTcsSkillDefinition* Definition = LoadSkillDefinitionSync(SkillDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(SkillDefId, TEXT("GetSkillDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsStateDefinition* UTcsDefinitionManagerSubsystem::GetStateDefinition(FName StateDefId) const
{
	if (const TObjectPtr<UTcsStateDefinition>* Found = StateDefinitions.Find(StateDefId))
	{
		return Found->Get();
	}

	if (const UTcsStateDefinition* Definition = LoadBuffDefinitionSync(StateDefId))
	{
		return Definition;
	}

	if (const UTcsStateDefinition* Definition = LoadSkillDefinitionSync(StateDefId))
	{
		return Definition;
	}

	LogDefinitionQueryFailure(StateDefId, TEXT("GetStateDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	return nullptr;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::GetStateSlotDefinition(FName StateSlotDefId) const
{
	if (const TObjectPtr<UTcsStateSlotDefinition>* Found = StateSlotDefinitions.Find(StateSlotDefId))
	{
		return Found->Get();
	}

	const UTcsStateSlotDefinition* Definition = LoadStateSlotDefinitionSync(StateSlotDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(StateSlotDefId, TEXT("GetStateSlotDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::GetStateSlotDefinitionByTag(FGameplayTag StateSlotTag) const
{
	if (const FName* StateSlotDefId = StateSlotTagToDefId.Find(StateSlotTag))
	{
		return GetStateSlotDefinition(*StateSlotDefId);
	}

	const UTcsStateSlotDefinition* Definition = LoadByTagFromSource(
		StateSlotDefinitionSources, StateSlotDefinitions, StateSlotTagToDefId, StateSlotTag,
		TFunctionRef<FGameplayTag(const UTcsStateSlotDefinition&)>([](const UTcsStateSlotDefinition& Def) { return Def.SlotTag; }));

	if (!Definition)
	{
		LogDefinitionQueryFailure(StateSlotTag.GetTagName(), TEXT("GetStateSlotDefinitionByTag"), TEXT("NotRegistered"));
	}
	return Definition;
}

const UTcsAttributeDefinition* UTcsDefinitionManagerSubsystem::GetAttributeDefinition(FName AttributeDefId) const
{
	if (const TObjectPtr<UTcsAttributeDefinition>* Found = AttributeDefinitions.Find(AttributeDefId))
	{
		return Found->Get();
	}

	const UTcsAttributeDefinition* Definition = LoadAttributeDefinitionSync(AttributeDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeDefId, TEXT("GetAttributeDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsAttributeModifierDefinition* UTcsDefinitionManagerSubsystem::GetAttributeModifierDefinition(FName AttributeModifierDefId) const
{
	if (const TObjectPtr<UTcsAttributeModifierDefinition>* Found = AttributeModifierDefinitions.Find(AttributeModifierDefId))
	{
		return Found->Get();
	}

	const UTcsAttributeModifierDefinition* Definition = LoadAttributeModifierDefinitionSync(AttributeModifierDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeModifierDefId, TEXT("GetAttributeModifierDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

const UTcsSkillModifierDefinition* UTcsDefinitionManagerSubsystem::GetSkillModifierDefinition(FName SkillModifierDefId) const
{
	if (const TObjectPtr<UTcsSkillModifierDefinition>* Found = SkillModifierDefinitions.Find(SkillModifierDefId))
	{
		return Found->Get();
	}

	const UTcsSkillModifierDefinition* Definition = LoadSkillModifierDefinitionSync(SkillModifierDefId);
	if (!Definition)
	{
		LogDefinitionQueryFailure(SkillModifierDefId, TEXT("GetSkillModifierDefinition"), TEXT("NotRegisteredOrLoadFailed"));
	}
	return Definition;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllAttributeDefIds() const
{
	TArray<FName> DefinitionIds;
	AttributeDefinitionSources.GetKeys(DefinitionIds);
	return DefinitionIds;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllAttributeModifierDefIds() const
{
	TArray<FName> DefinitionIds;
	AttributeModifierDefinitionSources.GetKeys(DefinitionIds);
	return DefinitionIds;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllStateSlotDefIds() const
{
	TArray<FName> DefinitionIds;
	StateSlotDefinitionSources.GetKeys(DefinitionIds);
	return DefinitionIds;
}

TArray<FName> UTcsDefinitionManagerSubsystem::GetAllStateLikeDefIds() const
{
	TArray<FName> DefinitionIds;
	StateDefinitionSources.GetKeys(DefinitionIds);
	return DefinitionIds;
}

const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::LoadBuffDefinitionSync(FName BuffDefId) const
{
	UTcsBuffDefinition* Asset = LoadFromSource(BuffDefinitionSources, BuffDefinitions, BuffDefId);
	if (Asset)
	{
		StateDefinitions.Add(BuffDefId, Asset);
		if (Asset->StateTag.IsValid())
		{
			BuffTagToDefId.Add(Asset->StateTag, BuffDefId);
		}
	}
	return Asset;
}

const UTcsSkillDefinition* UTcsDefinitionManagerSubsystem::LoadSkillDefinitionSync(FName SkillDefId) const
{
	UTcsSkillDefinition* Asset = LoadFromSource(SkillDefinitionSources, SkillDefinitions, SkillDefId);
	if (Asset)
	{
		StateDefinitions.Add(SkillDefId, Asset);
	}
	return Asset;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::LoadStateSlotDefinitionSync(FName StateSlotDefId) const
{
	UTcsStateSlotDefinition* Asset = LoadFromSource(StateSlotDefinitionSources, StateSlotDefinitions, StateSlotDefId);
	if (Asset && Asset->SlotTag.IsValid())
	{
		StateSlotTagToDefId.Add(Asset->SlotTag, StateSlotDefId);
	}
	return Asset;
}

const UTcsAttributeDefinition* UTcsDefinitionManagerSubsystem::LoadAttributeDefinitionSync(FName AttributeDefId) const
{
	return LoadFromSource(AttributeDefinitionSources, AttributeDefinitions, AttributeDefId);
}

const UTcsAttributeModifierDefinition* UTcsDefinitionManagerSubsystem::LoadAttributeModifierDefinitionSync(FName AttributeModifierDefId) const
{
	return LoadFromSource(AttributeModifierDefinitionSources, AttributeModifierDefinitions, AttributeModifierDefId);
}

const UTcsSkillModifierDefinition* UTcsDefinitionManagerSubsystem::LoadSkillModifierDefinitionSync(FName SkillModifierDefId) const
{
	return LoadFromSource(SkillModifierDefinitionSources, SkillModifierDefinitions, SkillModifierDefId);
}

void UTcsDefinitionManagerSubsystem::LogDefinitionQueryFailure(
	FName QueryKey,
	const TCHAR* EntryName,
	const TCHAR* FailureCategory) const
{
	UE_LOG(LogTcs, Warning,
		TEXT("[UTcsDefinitionManagerSubsystem::%s] Definition query failed. Key=%s Category=%s"),
		EntryName,
		*QueryKey.ToString(),
		FailureCategory);
}
