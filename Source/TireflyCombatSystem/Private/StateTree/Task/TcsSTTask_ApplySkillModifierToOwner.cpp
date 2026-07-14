// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Task/TcsSTTask_ApplySkillModifierToOwner.h"

#include "Skill/TcsSkillComponent.h"
#include "State/TcsStateInstance.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"



FTcsSTTask_ApplySkillModifierToOwner::FTcsSTTask_ApplySkillModifierToOwner()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = true;
}


bool FTcsSTTask_ApplySkillModifierToOwner::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(StateInstanceHandle);
	return bResult;
}


EStateTreeRunStatus FTcsSTTask_ApplySkillModifierToOwner::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UTcsStateInstance& StateInstance = Context.GetExternalData(StateInstanceHandle);

	UTcsSkillComponent* SkillComp = StateInstance.GetOwnerSkillComponent();
	if (!SkillComp || InstanceData.SkillModifierDefIds.IsEmpty() || !StateInstance.GetSourceHandle().IsValid())
	{
		InstanceData.AppliedRuntimeEntries.Reset();
		return EStateTreeRunStatus::Failed;
	}

	if (!SkillComp->ApplySkillModifiersWithSourceHandle(
		StateInstance.GetSourceHandle(),
		InstanceData.SkillModifierDefIds,
		InstanceData.AppliedRuntimeEntries))
	{
		InstanceData.AppliedRuntimeEntries.Reset();
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}


#if WITH_EDITOR
FText FTcsSTTask_ApplySkillModifierToOwner::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Apply SkillModifier to Owner SkillComponent"));
}
#endif
