// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "Engine/AssetManagerTypes.h"
#include "GameFramework/Actor.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"



ETcsSkillActivateResult UTcsSkillComponent::ActivateSkill(FName SkillDefId, AActor* Instigator)
{
	if (!IsRuntimeReady())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[SkillComp::ActivateSkill] Skill runtime is not ready for %s"), *GetPathName());
		return ETcsSkillActivateResult::NotReady;
	}

	if (SkillDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillComp::ActivateSkill] SkillDefId is none"));
		return ETcsSkillActivateResult::InvalidDefinition;
	}

	UTcsSkillEntry* Entry = GetSkillEntry(SkillDefId);
	if (!Entry)
	{
		return ETcsSkillActivateResult::NotLearned;
	}

	UTcsSkillDefinition* Definition = Entry->GetSkillDefinition();
	if (!Definition || Entry->GetSkillDefId() != SkillDefId || Definition->StateDefId != SkillDefId)
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::ActivateSkill] Invalid cached SkillDefinition for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return ETcsSkillActivateResult::InvalidDefinition;
	}

	if (Entry->IsOnCooldown())
	{
		return ETcsSkillActivateResult::OnCooldown;
	}

	// 单实例：取消上一个。
	if (Entry->ActiveInstance.IsValid())
	{
		if (UTcsStateComponent* StateCmp = GetOwnerStateComponent())
		{
			StateCmp->RequestStateRemoval(Entry->ActiveInstance.Get(), TcsStateRemovalReasons::Cancelled);
		}
		Entry->ActiveInstance.Reset();
	}

	UClass* SkillInstanceClass = Definition->ResolveStateInstanceClass();
	if (!SkillInstanceClass ||
		!SkillInstanceClass->IsChildOf(UTcsSkillInstance::StaticClass()) ||
		SkillInstanceClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillComp::ActivateSkill] Invalid SkillInstanceClass for SkillDefId '%s'"),
			*SkillDefId.ToString());
		return ETcsSkillActivateResult::InvalidDefinition;
	}

	AActor* OwnerActor = GetOwner();
	UObject* SkillInstanceOuter = OwnerActor ? static_cast<UObject*>(OwnerActor) : static_cast<UObject*>(this);
	UTcsSkillInstance* SkillInst = NewObject<UTcsSkillInstance>(SkillInstanceOuter, SkillInstanceClass);
	if (!SkillInst)
	{
		return ETcsSkillActivateResult::ApplyFailed;
	}
	SkillInst->SetSkillEntry(Entry);
	SkillInst->Initialize(
		Definition,
		SkillDefId,
		OwnerActor,
		Instigator,
		UTcsStateComponent::AllocateStateInstanceId(),
		Entry->GetLevel());

	if (!SkillInst->IsInitialized())
	{
		SkillInst->MarkPendingGC();
		return ETcsSkillActivateResult::ApplyFailed;
	}

	SkillInst->SetApplyTimestamp(FDateTime::UtcNow().GetTicks());
	SkillInst->SetSourceHandle(UTcsStateComponent::CreateSourceHandle(TArray<FPrimaryAssetId>(), Instigator));

	UTcsStateComponent* StateCmp = GetOwnerStateComponent();
	if (!StateCmp)
	{
		SkillInst->MarkPendingGC();
		return ETcsSkillActivateResult::ApplyFailed;
	}

	const bool bApplied = StateCmp->TryApplyStateInstance(SkillInst);
	if (!bApplied)
	{
		SkillInst->MarkPendingGC();
		return ETcsSkillActivateResult::ApplyFailed;
	}

	if (Entry->StartCooldown(SkillInst))
	{
		CooldownTracker.Add(Entry);
	}

	Entry->ActiveInstance = SkillInst;

	return ETcsSkillActivateResult::Success;
}
