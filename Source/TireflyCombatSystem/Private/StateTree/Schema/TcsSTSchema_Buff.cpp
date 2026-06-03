// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Schema/TcsSTSchema_Buff.h"

#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "Buff/TcsBuffInstance.h"
#include "Engine/GameInstance.h"
#include "State/TcsStateInstance.h"
#include "Subsystems/WorldSubsystem.h"
#include "TcsLogChannels.h"



UTcsSTSchema_Buff::UTcsSTSchema_Buff()
{
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsBuffStateContextName::BuffInstance,
		UTcsBuffInstance::StaticClass(),
		FGuid::NewGuid()
	));
}

TConstArrayView<FStateTreeExternalDataDesc> UTcsSTSchema_Buff::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UTcsSTSchema_Buff::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UTcsSTSchema_Buff::IsClassAllowed(const UClass* InClass) const
{
	return InClass && InClass->IsChildOf<UStateTreeNodeBlueprintBase>();
}

bool UTcsSTSchema_Buff::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(UWorldSubsystem::StaticClass())
		|| InStruct.IsChildOf(UGameInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsStateInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsBuffInstance::StaticClass());
}

bool UTcsSTSchema_Buff::SetContextRequirements(
	UTcsBuffInstance& BuffInstance,
	FStateTreeExecutionContext& Context,
	bool bLogErrors)
{
	if (!Context.IsValid())
	{
		return false;
	}

	FTcsContextDataSetter ContextDataSetter = FTcsContextDataSetter(&BuffInstance, Context);
	ContextDataSetter.GetSchema()->SetContextData(ContextDataSetter, bLogErrors);

	const bool bResult = Context.AreContextDataViewsValid();
	if (!bResult && bLogErrors)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("%s Missing external data requirements. StateTree will not update."), *FString(__FUNCTION__));
	}

	return bResult;
}

bool UTcsSTSchema_Buff::CollectExternalData(
	const FStateTreeExecutionContext& Context,
	const UStateTree* StateTree,
	UTcsBuffInstance* BuffInstance,
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
	TArrayView<FStateTreeDataView> OutDataViews)
{
	checkf(ExternalDataDescs.Num() == OutDataViews.Num(), TEXT("The execution context failed to fill OutDataViews with empty values."));

	if (!BuffInstance || !StateTree)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get BuffInstance or StateTree for StateTree external data"),
			*FString(__FUNCTION__));
		return false;
	}

	int32 IssuesFoundCounter = 0;
	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
	{
		const FStateTreeExternalDataDesc& DataDesc = ExternalDataDescs[Index];
		if (DataDesc.Struct == nullptr)
		{
			continue;
		}

		if (DataDesc.Struct->IsChildOf(UTcsBuffInstance::StaticClass())
			|| DataDesc.Struct->IsChildOf(UTcsStateInstance::StaticClass()))
		{
			OutDataViews[Index] = FStateTreeDataView(BuffInstance);
		}
		else if (DataDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			UWorld* World = Context.GetWorld();
			UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(DataDesc.Struct.Get())));
			OutDataViews[Index] = FStateTreeDataView(Subsystem);
			UE_CVLOG(Subsystem == nullptr, BuffInstance, LogTcsStateTree, Error,
				TEXT("[%s] StateTree %s: Could not find required subsystem %s"),
				*FString(__FUNCTION__),
				*GetNameSafe(Context.GetStateTree()),
				*GetNameSafe(DataDesc.Struct));
			IssuesFoundCounter += Subsystem != nullptr ? 0 : 1;
		}
		else if (DataDesc.Struct->IsChildOf(UGameInstance::StaticClass()))
		{
			UWorld* World = Context.GetWorld();
			UGameInstance* GameInstance = World->GetGameInstance();
			OutDataViews[Index] = FStateTreeDataView(GameInstance);
			UE_CVLOG(GameInstance == nullptr, BuffInstance, LogTcsStateTree, Error,
				TEXT("[%s] StateTree %s: Could not find required game instance"),
				*FString(__FUNCTION__),
				*GetNameSafe(Context.GetStateTree()));
			IssuesFoundCounter += GameInstance != nullptr ? 0 : 1;
		}
		else
		{
			UE_CVLOG(true, BuffInstance, LogTcsStateTree, Error,
				TEXT("[%s] StateTree %s: Unsupported external data request %s"),
				*FString(__FUNCTION__),
				*GetNameSafe(Context.GetStateTree()),
				*GetNameSafe(DataDesc.Struct));
			IssuesFoundCounter += 1;
		}
	}

	return IssuesFoundCounter == 0;
}

UTcsSTSchema_Buff::FTcsContextDataSetter::FTcsContextDataSetter(
	TNotNull<const UTcsBuffInstance*> InBuffInstance,
	FStateTreeExecutionContext& Context)
	: BuffInstance(InBuffInstance)
	, ExecutionContext(Context)
{
}

TNotNull<const UStateTree*> UTcsSTSchema_Buff::FTcsContextDataSetter::GetStateTree() const
{
	return ExecutionContext.GetStateTree();
}

TNotNull<const UTcsSTSchema_Buff*> UTcsSTSchema_Buff::FTcsContextDataSetter::GetSchema() const
{
	return Cast<UTcsSTSchema_Buff>(ExecutionContext.GetStateTree()->GetSchema());
}

bool UTcsSTSchema_Buff::FTcsContextDataSetter::SetContextDataByName(
	FName Name,
	FStateTreeDataView DataView)
{
	return ExecutionContext.IsValid() ? ExecutionContext.SetContextDataByName(Name, DataView) : false;
}

void UTcsSTSchema_Buff::SetContextData(
	FTcsContextDataSetter& ContextDataSetter,
	bool bLogErrors) const
{
	const UTcsSTSchema_Buff* Schema = ContextDataSetter.GetSchema();
	const UTcsBuffInstance* BuffInstance = ContextDataSetter.GetBuffInstance();

	if (!Schema || !BuffInstance)
	{
		if (bLogErrors)
		{
			UE_LOG(LogTcsStateTree, Error,
				TEXT("%s Expected StateTree asset to contain UTcsSTSchema_Buff and UTcsBuffInstance. StateTree will not update."),
				*FString(__FUNCTION__));
		}
		return;
	}

	ContextDataSetter.SetContextDataByName(
		TcsBuffStateContextName::BuffInstance,
		FStateTreeDataView(const_cast<UTcsBuffInstance*>(BuffInstance)));
}