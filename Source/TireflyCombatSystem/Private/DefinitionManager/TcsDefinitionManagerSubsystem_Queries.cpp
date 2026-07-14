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
		FName DefinitionId,
		const TCHAR*& OutFailureCategory)
	{
		if (const TObjectPtr<AssetType>* Found = LoadedCache.Find(DefinitionId))
		{
			return Found->Get();
		}

		const FTcsDefinitionSourceEntry* Source = SourceCache.Find(DefinitionId);
		if (!Source)
		{
			OutFailureCategory = TEXT("NotRegistered");
			return nullptr;
		}

		UPrimaryDataAsset* Asset = Source->SoftPtr.LoadSynchronous();
		if (!Asset)
		{
			OutFailureCategory = TEXT("LoadFailed");
			return nullptr;
		}

		AssetType* TypedAsset = Cast<AssetType>(Asset);
		if (!TypedAsset)
		{
			OutFailureCategory = TEXT("TypeMismatch");
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
		TFunctionRef<FGameplayTag(const AssetType&)> GetTag,
		const TCHAR*& OutFailureCategory)
	{
		OutFailureCategory = TEXT("NotRegistered");
		for (const TPair<FName, FTcsDefinitionSourceEntry>& Pair : SourceCache)
		{
			const TCHAR* CandidateFailureCategory = TEXT("NotRegistered");
			AssetType* Asset = LoadFromSource(SourceCache, LoadedCache, Pair.Key, CandidateFailureCategory);
			if (Asset && GetTag(*Asset) == Tag)
			{
				TagToDefId.Add(Tag, Pair.Key);
				return Asset;
			}

			if (!Asset && !FStringView(CandidateFailureCategory).Equals(TEXT("NotRegistered")))
			{
				OutFailureCategory = CandidateFailureCategory;
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

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsBuffDefinition* Definition = LoadBuffDefinitionSync(BuffDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(BuffDefId, TEXT("GetBuffDefinition"), FailureCategory);
	}
	return Definition;
}

const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::GetBuffDefinitionByTag(FGameplayTag BuffTag) const
{
	if (const FName* BuffDefId = BuffTagToDefId.Find(BuffTag))
	{
		return GetBuffDefinition(*BuffDefId);
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsBuffDefinition* Definition = LoadByTagFromSource(
		BuffDefinitionSources, BuffDefinitions, BuffTagToDefId, BuffTag,
		TFunctionRef<FGameplayTag(const UTcsBuffDefinition&)>([](const UTcsBuffDefinition& Def) { return Def.StateTag; }),
		FailureCategory);

	if (!Definition)
	{
		LogDefinitionQueryFailure(BuffTag.GetTagName(), TEXT("GetBuffDefinitionByTag"), FailureCategory);
	}
	return Definition;
}

const UTcsSkillDefinition* UTcsDefinitionManagerSubsystem::GetSkillDefinition(FName SkillDefId) const
{
	if (const TObjectPtr<UTcsSkillDefinition>* Found = SkillDefinitions.Find(SkillDefId))
	{
		return Found->Get();
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsSkillDefinition* Definition = LoadSkillDefinitionSync(SkillDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(SkillDefId, TEXT("GetSkillDefinition"), FailureCategory);
	}
	return Definition;
}

const UTcsStateDefinition* UTcsDefinitionManagerSubsystem::GetStateDefinition(FName StateDefId) const
{
	if (const TObjectPtr<UTcsStateDefinition>* Found = StateDefinitions.Find(StateDefId))
	{
		return Found->Get();
	}

	if (AmbiguousStateDefinitionIds.Contains(StateDefId))
	{
		LogDefinitionQueryFailure(StateDefId, TEXT("GetStateDefinition"), TEXT("TypeMismatch"));
		return nullptr;
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsStateDefinition* Definition = LoadStateDefinitionSync(StateDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(StateDefId, TEXT("GetStateDefinition"), FailureCategory);
	}
	return Definition;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::GetStateSlotDefinition(FName StateSlotDefId) const
{
	if (const TObjectPtr<UTcsStateSlotDefinition>* Found = StateSlotDefinitions.Find(StateSlotDefId))
	{
		return Found->Get();
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsStateSlotDefinition* Definition = LoadStateSlotDefinitionSync(StateSlotDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(StateSlotDefId, TEXT("GetStateSlotDefinition"), FailureCategory);
	}
	return Definition;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::GetStateSlotDefinitionByTag(FGameplayTag StateSlotTag) const
{
	if (const FName* StateSlotDefId = StateSlotTagToDefId.Find(StateSlotTag))
	{
		return GetStateSlotDefinition(*StateSlotDefId);
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsStateSlotDefinition* Definition = LoadByTagFromSource(
		StateSlotDefinitionSources, StateSlotDefinitions, StateSlotTagToDefId, StateSlotTag,
		TFunctionRef<FGameplayTag(const UTcsStateSlotDefinition&)>([](const UTcsStateSlotDefinition& Def) { return Def.SlotTag; }),
		FailureCategory);

	if (!Definition)
	{
		LogDefinitionQueryFailure(StateSlotTag.GetTagName(), TEXT("GetStateSlotDefinitionByTag"), FailureCategory);
	}
	return Definition;
}

const UTcsAttributeDefinition* UTcsDefinitionManagerSubsystem::GetAttributeDefinition(FName AttributeDefId) const
{
	if (const TObjectPtr<UTcsAttributeDefinition>* Found = AttributeDefinitions.Find(AttributeDefId))
	{
		return Found->Get();
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsAttributeDefinition* Definition = LoadAttributeDefinitionSync(AttributeDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeDefId, TEXT("GetAttributeDefinition"), FailureCategory);
	}
	return Definition;
}

const UTcsAttributeDefinition* UTcsDefinitionManagerSubsystem::GetAttributeDefinitionByTag(FGameplayTag AttributeTag) const
{
	if (const FName* AttributeDefId = AttributeTagToDefId.Find(AttributeTag))
	{
		return GetAttributeDefinition(*AttributeDefId);
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsAttributeDefinition* Definition = LoadByTagFromSource(
		AttributeDefinitionSources, AttributeDefinitions, AttributeTagToDefId, AttributeTag,
		TFunctionRef<FGameplayTag(const UTcsAttributeDefinition&)>([](const UTcsAttributeDefinition& Def) { return Def.AttributeTag; }),
		FailureCategory);

	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeTag.GetTagName(), TEXT("GetAttributeDefinitionByTag"), FailureCategory);
	}
	return Definition;
}

FName UTcsDefinitionManagerSubsystem::ResolveAttributeDefIdByTag(FGameplayTag AttributeTag) const
{
	if (const UTcsAttributeDefinition* Def = GetAttributeDefinitionByTag(AttributeTag))
	{
		return Def->AttributeDefId;
	}
	return NAME_None;
}

const UTcsAttributeModifierDefinition* UTcsDefinitionManagerSubsystem::GetAttributeModifierDefinition(FName AttributeModifierDefId) const
{
	if (const TObjectPtr<UTcsAttributeModifierDefinition>* Found = AttributeModifierDefinitions.Find(AttributeModifierDefId))
	{
		return Found->Get();
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsAttributeModifierDefinition* Definition = LoadAttributeModifierDefinitionSync(AttributeModifierDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(AttributeModifierDefId, TEXT("GetAttributeModifierDefinition"), FailureCategory);
	}
	return Definition;
}

const UTcsSkillModifierDefinition* UTcsDefinitionManagerSubsystem::GetSkillModifierDefinition(FName SkillModifierDefId) const
{
	if (const TObjectPtr<UTcsSkillModifierDefinition>* Found = SkillModifierDefinitions.Find(SkillModifierDefId))
	{
		return Found->Get();
	}

	const TCHAR* FailureCategory = TEXT("NotRegistered");
	const UTcsSkillModifierDefinition* Definition = LoadSkillModifierDefinitionSync(SkillModifierDefId, FailureCategory);
	if (!Definition)
	{
		LogDefinitionQueryFailure(SkillModifierDefId, TEXT("GetSkillModifierDefinition"), FailureCategory);
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

const UTcsBuffDefinition* UTcsDefinitionManagerSubsystem::LoadBuffDefinitionSync(FName BuffDefId, const TCHAR*& OutFailureCategory) const
{
	UTcsBuffDefinition* Asset = LoadFromSource(BuffDefinitionSources, BuffDefinitions, BuffDefId, OutFailureCategory);
	if (Asset && !AmbiguousStateDefinitionIds.Contains(BuffDefId))
	{
		StateDefinitions.Add(BuffDefId, Asset);
		if (Asset->StateTag.IsValid())
		{
			BuffTagToDefId.Add(Asset->StateTag, BuffDefId);
		}
	}
	return Asset;
}

const UTcsSkillDefinition* UTcsDefinitionManagerSubsystem::LoadSkillDefinitionSync(FName SkillDefId, const TCHAR*& OutFailureCategory) const
{
	UTcsSkillDefinition* Asset = LoadFromSource(SkillDefinitionSources, SkillDefinitions, SkillDefId, OutFailureCategory);
	if (Asset && !AmbiguousStateDefinitionIds.Contains(SkillDefId))
	{
		StateDefinitions.Add(SkillDefId, Asset);
	}
	return Asset;
}

const UTcsStateDefinition* UTcsDefinitionManagerSubsystem::LoadStateDefinitionSync(FName StateDefId, const TCHAR*& OutFailureCategory) const
{
	UTcsStateDefinition* Asset = LoadFromSource(StateDefinitionSources, StateDefinitions, StateDefId, OutFailureCategory);
	if (UTcsBuffDefinition* BuffDefinition = Cast<UTcsBuffDefinition>(Asset))
	{
		BuffDefinitions.Add(StateDefId, BuffDefinition);
		if (BuffDefinition->StateTag.IsValid())
		{
			BuffTagToDefId.Add(BuffDefinition->StateTag, StateDefId);
		}
	}
	else if (UTcsSkillDefinition* SkillDefinition = Cast<UTcsSkillDefinition>(Asset))
	{
		SkillDefinitions.Add(StateDefId, SkillDefinition);
	}
	else if (Asset)
	{
		StateDefinitions.Remove(StateDefId);
		OutFailureCategory = TEXT("TypeMismatch");
		return nullptr;
	}

	return Asset;
}

const UTcsStateSlotDefinition* UTcsDefinitionManagerSubsystem::LoadStateSlotDefinitionSync(FName StateSlotDefId, const TCHAR*& OutFailureCategory) const
{
	UTcsStateSlotDefinition* Asset = LoadFromSource(StateSlotDefinitionSources, StateSlotDefinitions, StateSlotDefId, OutFailureCategory);
	if (Asset && Asset->SlotTag.IsValid())
	{
		StateSlotTagToDefId.Add(Asset->SlotTag, StateSlotDefId);
	}
	return Asset;
}

const UTcsAttributeDefinition* UTcsDefinitionManagerSubsystem::LoadAttributeDefinitionSync(FName AttributeDefId, const TCHAR*& OutFailureCategory) const
{
	return LoadFromSource(AttributeDefinitionSources, AttributeDefinitions, AttributeDefId, OutFailureCategory);
}

const UTcsAttributeModifierDefinition* UTcsDefinitionManagerSubsystem::LoadAttributeModifierDefinitionSync(FName AttributeModifierDefId, const TCHAR*& OutFailureCategory) const
{
	return LoadFromSource(AttributeModifierDefinitionSources, AttributeModifierDefinitions, AttributeModifierDefId, OutFailureCategory);
}

const UTcsSkillModifierDefinition* UTcsDefinitionManagerSubsystem::LoadSkillModifierDefinitionSync(FName SkillModifierDefId, const TCHAR*& OutFailureCategory) const
{
	return LoadFromSource(SkillModifierDefinitionSources, SkillModifierDefinitions, SkillModifierDefId, OutFailureCategory);
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
