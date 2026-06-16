// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Task/TcsSTTask_ApplyAttributeModifierToTarget.h"

#include "Attribute/TcsAttributeComponent.h"
#include "State/TcsStateInstance.h"
#include "TcsEntityInterface.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"



FTcsSTTask_ApplyAttributeModifierToTarget::FTcsSTTask_ApplyAttributeModifierToTarget()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = true;
}

bool FTcsSTTask_ApplyAttributeModifierToTarget::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(StateInstanceHandle);
	return bResult;
}

EStateTreeRunStatus FTcsSTTask_ApplyAttributeModifierToTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UTcsStateInstance& StateInstance = Context.GetExternalData(StateInstanceHandle);

	AActor* TargetActor = InstanceData.TargetActor;
	if (!IsValid(TargetActor) || !TargetActor->Implements<UTcsEntityInterface>())
	{
		return EStateTreeRunStatus::Failed;
	}

	UTcsAttributeComponent* AttrComp = ITcsEntityInterface::Execute_GetAttributeComponent(TargetActor);
	if (!AttrComp)
	{
		return EStateTreeRunStatus::Failed;
	}

	FTcsAttributeModifierInstance ModifierInst;
	if (!AttrComp->CreateAttributeModifierWithBindings(
		InstanceData.ModifierId,
		StateInstance.GetInstigator(),
		InstanceData.OperandBindings,
		ModifierInst))
	{
		return EStateTreeRunStatus::Failed;
	}

	ModifierInst.SourceHandle = StateInstance.GetSourceHandle();

	TArray<FTcsAttributeModifierInstance> Modifiers = { ModifierInst };
	AttrComp->ApplyModifier(Modifiers);

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FTcsSTTask_ApplyAttributeModifierToTarget::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Apply AttributeModifier to Target"));
}
#endif
