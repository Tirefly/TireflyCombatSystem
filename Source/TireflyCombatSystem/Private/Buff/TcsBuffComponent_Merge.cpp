// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "Buff/TcsBuffDefinition.h"
#include "Buff/TcsBuffInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "TcsLogChannels.h"



void UTcsBuffComponent::RebuildBuffMergeGroups(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	TMap<FName, ETcsBuffMergeDirtyReason> ExistingDirtyReasons;
	for (const TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : StateSlot->BuffMergeGroups)
	{
		if (Pair.Value.HasDirty())
		{
			ExistingDirtyReasons.Add(Pair.Key, Pair.Value.DirtyReasons);
		}
	}

	StateSlot->BuffMergeGroups.Empty();

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(State))
		{
			FTcsBuffMergeGroupRuntime& GroupRuntime = StateSlot->BuffMergeGroups.FindOrAdd(BuffInstance->GetStateDefId());
			GroupRuntime.StateDefId = BuffInstance->GetStateDefId();
			GroupRuntime.AddMember(BuffInstance);
		}
	}

	for (TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : StateSlot->BuffMergeGroups)
	{
		if (const ETcsBuffMergeDirtyReason* ExistingReasons = ExistingDirtyReasons.Find(Pair.Key))
		{
			Pair.Value.MarkDirty(*ExistingReasons);
			StateSlot->DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}

		if (StateSlot->bBuffMergeRequiresFullRebuild)
		{
			Pair.Value.MarkDirty(ETcsBuffMergeDirtyReason::ForceRebuild);
			StateSlot->DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}
	}

	StateSlot->bBuffMergeRequiresFullRebuild = false;
}

bool UTcsBuffComponent::ShouldProcessBuffMergeGroup(
	ETcsBuffMergeDirtyReason DirtyReasons,
	ETcsBuffMergeDependencyFlags DependencyFlags) const
{
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ForceRebuild))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::MembershipChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::MemberSet))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::RuntimeValueChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::RuntimeStack))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ExecutionStageChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::ExecutionStage))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::SlotGateChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::SlotGateState))
	{
		return true;
	}

	return false;
}

void UTcsBuffComponent::ProcessBuffMerging(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	if (StateSlot->bBuffMergeRequiresFullRebuild)
	{
		RebuildBuffMergeGroups(StateSlot);
	}

	if (StateSlot->DirtyBuffMergeStateDefIds.IsEmpty())
	{
		return;
	}

	const TSet<FName> DirtyStateDefIds = StateSlot->DirtyBuffMergeStateDefIds;
	StateSlot->DirtyBuffMergeStateDefIds.Empty();

	TArray<UTcsBuffInstance*> AllMergedOutBuffs;
	for (const FName& StateDefId : DirtyStateDefIds)
	{
		FTcsBuffMergeGroupRuntime* GroupRuntime = StateSlot->BuffMergeGroups.Find(StateDefId);
		if (!GroupRuntime)
		{
			continue;
		}

		TArray<UTcsBuffInstance*> GroupMembers;
		GroupRuntime->GatherValidMembers(GroupMembers);
		if (GroupMembers.IsEmpty())
		{
			StateSlot->BuffMergeGroups.Remove(StateDefId);
			continue;
		}

		UTcsBuffInstance* MergeSource = GroupMembers[0];
		TSubclassOf<UTcsBuffMerger> MergerClass = MergeSource ? MergeSource->GetMergerType() : nullptr;
		UTcsBuffMerger* Merger = MergerClass ? MergerClass->GetDefaultObject<UTcsBuffMerger>() : nullptr;
		const ETcsBuffMergeDependencyFlags DependencyFlags = Merger
			? Merger->GetDependencyFlags()
			: ETcsBuffMergeDependencyFlags::None;
		GroupRuntime->DependencyFlags = DependencyFlags;
		const ETcsBuffMergeDirtyReason PendingDirtyReasons = GroupRuntime->DirtyReasons;

		if (!ShouldProcessBuffMergeGroup(PendingDirtyReasons, DependencyFlags))
		{
			GroupRuntime->LastProcessedDirtyReasons = PendingDirtyReasons;
			GroupRuntime->ClearDirty(PendingDirtyReasons);
			continue;
		}

		TArray<UTcsBuffInstance*> MergedGroup;
		TArray<UTcsBuffInstance*> MergedOutGroup;
		MergeBuffStateGroup(GroupMembers, MergedGroup, MergedOutGroup);
		GroupRuntime->SetMembers(MergedGroup);
		GroupRuntime->LastProcessedDirtyReasons = PendingDirtyReasons;
		GroupRuntime->ClearDirty(PendingDirtyReasons);
		AllMergedOutBuffs.Append(MergedOutGroup);
	}

	RemoveMergedOutBuffs(StateSlot, AllMergedOutBuffs);
}

void UTcsBuffComponent::MergeBuffStateGroup(
	TArray<UTcsBuffInstance*>& BuffsToMerge,
	TArray<UTcsBuffInstance*>& OutMergedBuffs,
	TArray<UTcsBuffInstance*>& OutMergedOutBuffs)
{
	if (BuffsToMerge.Num() == 0)
	{
		return;
	}

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsDefinitionManagerSubsystem* DefinitionManager = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UTcsDefinitionManagerSubsystem>()
		: nullptr;
	if (!DefinitionManager)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	const UTcsBuffDefinition* BuffDef = DefinitionManager->GetBuffDefinition(BuffsToMerge[0]->GetStateDefId());
	if (!BuffDef)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get buff definition for %s"),
			*FString(__FUNCTION__),
			*BuffsToMerge[0]->GetStateDefId().ToString());
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsBuffInstance* MergeSource = BuffsToMerge[0];
	if (!MergeSource)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	TSubclassOf<UTcsBuffMerger> MergerClass = MergeSource->GetMergerType();
	if (!MergerClass)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsBuffMerger* Merger = MergerClass->GetDefaultObject<UTcsBuffMerger>();
	if (!IsValid(Merger))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get merger instance for %s"),
			*FString(__FUNCTION__),
			*MergerClass->GetName());
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	Merger->Merge(BuffsToMerge, OutMergedBuffs, OutMergedOutBuffs);
}

void UTcsBuffComponent::RemoveMergedOutBuffs(
	FTcsStateSlot* StateSlot,
	const TArray<UTcsBuffInstance*>& MergedOutBuffs)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !StateSlot)
	{
		return;
	}

	// 一次合并可能会把同槽位里的多个旧 Buff 一起淘汰。
	// 这些 Buff 的移除最终都会汇聚到同一个槽位刷新链路里，因此先包一层批处理，
	// 避免每淘汰一个 Buff 就对同一个槽位重跑一次完整结算。
	const bool bBatchMergedOutRemovals = MergedOutBuffs.Num() > 1;
	const bool bBatchMergedOutPublicEvents = MergedOutBuffs.Num() > 1;
	if (bBatchMergedOutPublicEvents)
	{
		BeginPublicEventBatch();
	}

	if (bBatchMergedOutRemovals)
	{
		StateComponent->BeginStateSlotActivationBatch();
	}

	for (UTcsBuffInstance* BuffInstance : MergedOutBuffs)
	{
		// 这里只处理当前仍然挂在这个槽位上的实例。
		// 如果某个 Buff 已经在前面的链路里被回收，再次进入移除主链只会制造重复工作。
		if (!IsValid(BuffInstance) || !StateSlot->States.Contains(BuffInstance))
		{
			continue;
		}

		StateComponent->RequestStateRemoval(BuffInstance, TcsBuffRemovalReasons::MergedOut);
	}

	// 统一在批次尾部结束，让状态组件按槽位去重后再做最终刷新。
	if (bBatchMergedOutRemovals)
	{
		StateComponent->EndStateSlotActivationBatch();
	}

	if (bBatchMergedOutPublicEvents)
	{
		EndPublicEventBatch();
	}
}

void UTcsBuffComponent::MarkBuffMergeGroupDirty(UTcsBuffInstance* BuffInstance, ETcsBuffMergeDirtyReason DirtyReason)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	const UTcsStateDefinition* StateDef = IsValid(BuffInstance) ? BuffInstance->GetStateDef() : nullptr;
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || !StateDef || !StateDef->StateSlotType.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(StateDef->StateSlotType);
	if (!StateSlot)
	{
		return;
	}

	StateSlot->MarkBuffMergeGroupDirty(BuffInstance->GetStateDefId(), DirtyReason);
	StateComponent->RequestStateSlotRefresh(StateDef->StateSlotType);
}
