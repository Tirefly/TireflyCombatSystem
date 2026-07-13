// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"



bool UTcsSkillComponent::LearnSkill(FName SkillDefId)
{
	if (SkillDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillComp::LearnSkill] SkillDefId is none"));
		return false;
	}

	if (LearnedSkills.Contains(SkillDefId))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[SkillComp::LearnSkill] Skill '%s' already learned"), *SkillDefId.ToString());
		return false;
	}

	const UTcsSkillDefinition* ResolvedDefinition = ResolveSkillDefinition(SkillDefId);
	if (!ResolvedDefinition)
	{
		return false;
	}

	UClass* EntryClass = ResolvedDefinition->ResolveSkillEntryClass();
	if (!EntryClass ||
		!EntryClass->IsChildOf(UTcsSkillEntry::StaticClass()) ||
		EntryClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::LearnSkill] Invalid SkillEntryClass for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return false;
	}

	UTcsSkillEntry* Entry = NewObject<UTcsSkillEntry>(this, EntryClass);
	if (!Entry)
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::LearnSkill] Failed to create SkillEntry for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return false;
	}

	if (!Entry->InitializeFromDef(SkillDefId, ResolvedDefinition))
	{
		Entry->MarkAsGarbage();
		return false;
	}

	LearnedSkills.Add(SkillDefId, Entry);

	UE_LOG(LogTcsState, Log, TEXT("[SkillComp::LearnSkill] Learned skill '%s'"), *SkillDefId.ToString());
	return true;
}

void UTcsSkillComponent::ForgetSkill(FName SkillDefId)
{
	TObjectPtr<UTcsSkillEntry>* Found = LearnedSkills.Find(SkillDefId);
	if (!Found || !(*Found))
	{
		return;
	}

	UTcsSkillEntry* Entry = Found->Get();

	// 遗忘前先撤销活跃 SkillInstance，避免 Entry 消失后仍被 StateComponent 驱动。
	if (Entry->ActiveInstance.IsValid())
	{
		if (UTcsStateComponent* StateCmp = GetOwnerStateComponent())
		{
			StateCmp->RequestStateRemoval(Entry->ActiveInstance.Get(), TcsStateRemovalReasons::Cancelled);
		}
		Entry->ActiveInstance.Reset();
	}

	RemoveSkillModifiersForSkillEntry(Entry);

	CooldownTracker.Remove(Entry);
	LearnedSkills.Remove(SkillDefId);
	Entry->MarkAsGarbage();

	UE_LOG(LogTcsState, Log, TEXT("[SkillComp::ForgetSkill] Forgot skill '%s'"), *SkillDefId.ToString());
}

bool UTcsSkillComponent::HasSkill(FName SkillDefId) const
{
	return !SkillDefId.IsNone() && LearnedSkills.Contains(SkillDefId);
}

UTcsSkillEntry* UTcsSkillComponent::GetSkillEntry(FName SkillDefId) const
{
	if (const TObjectPtr<UTcsSkillEntry>* Found = LearnedSkills.Find(SkillDefId))
	{
		return *Found;
	}
	return nullptr;
}

TArray<UTcsSkillEntry*> UTcsSkillComponent::GetAllSkillEntries() const
{
	TArray<UTcsSkillEntry*> Result;
	Result.Reserve(LearnedSkills.Num());
	for (const auto& Pair : LearnedSkills)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}

const UTcsSkillDefinition* UTcsSkillComponent::ResolveSkillDefinition(FName SkillDefId) const
{
	if (SkillDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillComp::ResolveSkillDefinition] SkillDefId is none"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::ResolveSkillDefinition] Missing world for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::ResolveSkillDefinition] Missing game instance for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return nullptr;
	}

	const UTcsDefinitionManagerSubsystem* DefinitionManager = GameInstance->GetSubsystem<UTcsDefinitionManagerSubsystem>();
	if (!DefinitionManager)
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::ResolveSkillDefinition] Missing DefinitionManager for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return nullptr;
	}

	return DefinitionManager->GetSkillDefinition(SkillDefId);
}
