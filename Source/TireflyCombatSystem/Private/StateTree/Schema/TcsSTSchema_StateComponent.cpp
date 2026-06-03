// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Schema/TcsSTSchema_StateComponent.h"

#include "BrainComponent.h"
#include "State/TcsStateComponent.h"
#include "StateTreeExecutionContext.h"
#include "TcsLogChannels.h"



UTcsSTSchema_StateComponent::UTcsSTSchema_StateComponent(const FObjectInitializer& ObjectInitializer)
{
	RefreshContextDescriptor();
}

void UTcsSTSchema_StateComponent::PostLoad()
{
	Super::PostLoad();
	RefreshContextDescriptor();
}

void UTcsSTSchema_StateComponent::SetContextData(FContextDataSetter& ContextDataSetter, bool bLogErrors) const
{
	const UTcsSTSchema_StateComponent* Schema = Cast<UTcsSTSchema_StateComponent>(ContextDataSetter.GetStateTree()->GetSchema());
	const UBrainComponent* BrainComponent = ContextDataSetter.GetComponent();
	const UTcsStateComponent* StateComponent = Cast<UTcsStateComponent>(BrainComponent);
	if (!Schema || !StateComponent)
	{
		if (bLogErrors)
		{
			UE_LOG(LogTcsStateTree, Error,
				TEXT("%s Expected StateTree asset to contain UTcsSTSchema_StateComponent and UTcsStateComponent. StateTree will not update."),
				*FString(__FUNCTION__));
		}
		return;
	}

	ContextDataSetter.SetContextDataByName(
		TcsStateComponentContextName::StateComponent,
		FStateTreeDataView(const_cast<UTcsStateComponent*>(StateComponent)));
}

#if WITH_EDITOR
void UTcsSTSchema_StateComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	RefreshContextDescriptor();
}
#endif

void UTcsSTSchema_StateComponent::RefreshContextDescriptor()
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