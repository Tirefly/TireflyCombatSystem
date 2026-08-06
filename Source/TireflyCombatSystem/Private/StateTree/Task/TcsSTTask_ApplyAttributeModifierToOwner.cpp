// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Task/TcsSTTask_ApplyAttributeModifierToOwner.h"

#include "Attribute/TcsAttributeComponent.h"
#include "State/TcsStateInstance.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"



FTcsSTTask_ApplyAttributeModifierToOwner::FTcsSTTask_ApplyAttributeModifierToOwner()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

bool FTcsSTTask_ApplyAttributeModifierToOwner::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(StateInstanceHandle);
	return bResult;
}

EStateTreeRunStatus FTcsSTTask_ApplyAttributeModifierToOwner::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UTcsStateInstance& StateInstance = Context.GetExternalData(StateInstanceHandle);

	UTcsAttributeComponent* AttrComp = StateInstance.GetOwnerAttributeComponent();
	if (!AttrComp)
	{
		return EStateTreeRunStatus::Failed;
	}

	FTcsAttributeModifierApplicationRequest Request;
	Request.ModifierDefId = InstanceData.ModifierDefId;
	Request.ApplicationMode = ETcsAttributeModifierApplicationMode::AMAM_Ongoing;
	Request.SourceHandle = StateInstance.GetSourceHandle();
	Request.SourceStateInstance = &StateInstance;

	FTcsAttributeModifierApplicationResult Result;
	return AttrComp->ApplyAttributeModifier(Request, Result)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FTcsSTTask_ApplyAttributeModifierToOwner::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Apply AttributeModifier to Owner"));
}
#endif
