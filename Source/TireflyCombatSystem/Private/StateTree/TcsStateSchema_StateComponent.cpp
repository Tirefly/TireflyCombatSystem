// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsStateSchema_StateComponent.h"

#include "BrainComponent.h"
#include "State/TcsStateComponent.h"
#include "StateTreeExecutionContext.h"
#include "TcsLogChannels.h"



UTcsStateSchema_StateComponent::UTcsStateSchema_StateComponent(const FObjectInitializer& ObjectInitializer)
{
	RefreshContextDescriptor();
}

void UTcsStateSchema_StateComponent::PostLoad()
{
	Super::PostLoad();
	RefreshContextDescriptor();
}

void UTcsStateSchema_StateComponent::SetContextData(FContextDataSetter& ContextDataSetter, bool bLogErrors) const
{
	const UTcsStateSchema_StateComponent* Schema = Cast<UTcsStateSchema_StateComponent>(ContextDataSetter.GetStateTree()->GetSchema());
	const UBrainComponent* BrainComponent = ContextDataSetter.GetComponent();
	const UTcsStateComponent* StateComponent = Cast<UTcsStateComponent>(BrainComponent);
	if (!Schema || !StateComponent)
	{
		if (bLogErrors)
		{
			UE_LOG(LogTcsStateTree, Error,
				TEXT("%s Expected StateTree asset to contain UTcsStateSchema_StateComponent and UTcsStateComponent. StateTree will not update."),
				*FString(__FUNCTION__));
		}
		return;
	}

	ContextDataSetter.SetContextDataByName(
		TcsStateComponentContextName::StateComponent,
		FStateTreeDataView(const_cast<UTcsStateComponent*>(StateComponent)));
}

#if WITH_EDITOR
void UTcsStateSchema_StateComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	RefreshContextDescriptor();
}
#endif

void UTcsStateSchema_StateComponent::RefreshContextDescriptor()
{
	if (ContextDataDescs.IsEmpty())
	{
		ContextDataDescs.Add(FStateTreeExternalDataDesc(
			TcsStateComponentContextName::StateComponent,
			UTcsStateComponent::StaticClass(),
			FGuid::NewGuid()
		));
		return;
	}

	ContextDataDescs[0].Name = TcsStateComponentContextName::StateComponent;
	ContextDataDescs[0].Struct = UTcsStateComponent::StaticClass();
	if (!ContextDataDescs[0].ID.IsValid())
	{
		ContextDataDescs[0].ID = FGuid::NewGuid();
	}
	if (ContextDataDescs.Num() > 1)
	{
		ContextDataDescs.SetNum(1, EAllowShrinking::No);
	}
}