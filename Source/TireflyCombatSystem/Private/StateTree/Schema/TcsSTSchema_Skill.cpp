// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Schema/TcsSTSchema_Skill.h"

#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "Engine/GameInstance.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "State/TcsStateInstance.h"
#include "Subsystems/WorldSubsystem.h"
#include "TcsLogChannels.h"



UTcsSTSchema_Skill::UTcsSTSchema_Skill()
{
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsSkillStateContextName::SkillInstance,
		UTcsSkillInstance::StaticClass(),
		FGuid::NewGuid()));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsSkillStateContextName::SkillEntry,
		UTcsSkillEntry::StaticClass(),
		FGuid::NewGuid()));
}

TConstArrayView<FStateTreeExternalDataDesc> UTcsSTSchema_Skill::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UTcsSTSchema_Skill::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UTcsSTSchema_Skill::IsClassAllowed(const UClass* InClass) const
{
	return InClass && InClass->IsChildOf<UStateTreeNodeBlueprintBase>();
}

bool UTcsSTSchema_Skill::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(UWorldSubsystem::StaticClass())
		|| InStruct.IsChildOf(UGameInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsStateInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsSkillInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsSkillEntry::StaticClass());
}

bool UTcsSTSchema_Skill::SetContextRequirements(
	UTcsSkillInstance& SkillInstance,
	UTcsSkillEntry& SkillEntry,
	FStateTreeExecutionContext& Context,
	bool bLogErrors)
{
	if (!Context.IsValid())
	{
		return false;
	}

	FTcsContextDataSetter ContextDataSetter = FTcsContextDataSetter(&SkillInstance, &SkillEntry, Context);
	ContextDataSetter.GetSchema()->SetContextData(ContextDataSetter, bLogErrors);

	const bool bResult = Context.AreContextDataViewsValid();
	if (!bResult && bLogErrors)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("%s Missing external data requirements. StateTree will not update."), *FString(__FUNCTION__));
	}

	return bResult;
}

bool UTcsSTSchema_Skill::CollectExternalData(
	const FStateTreeExecutionContext& Context,
	const UStateTree* StateTree,
	UTcsSkillInstance* SkillInstance,
	UTcsSkillEntry* SkillEntry,
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
	TArrayView<FStateTreeDataView> OutDataViews)
{
	checkf(ExternalDataDescs.Num() == OutDataViews.Num(), TEXT("The execution context failed to fill OutDataViews with empty values."));

	if (!SkillInstance || !SkillEntry || !StateTree)
	{
		UE_LOG(LogTcsStateTree, Error,
			TEXT("[%s] Failed to get SkillInstance, SkillEntry or StateTree for StateTree external data"),
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

		if (DataDesc.Struct->IsChildOf(UTcsSkillEntry::StaticClass()))
		{
			OutDataViews[Index] = FStateTreeDataView(SkillEntry);
		}
		else if (DataDesc.Struct->IsChildOf(UTcsSkillInstance::StaticClass())
			|| DataDesc.Struct->IsChildOf(UTcsStateInstance::StaticClass()))
		{
			OutDataViews[Index] = FStateTreeDataView(SkillInstance);
		}
		else if (DataDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			UWorld* World = Context.GetWorld();
			UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(DataDesc.Struct.Get())));
			OutDataViews[Index] = FStateTreeDataView(Subsystem);
			UE_CVLOG(Subsystem == nullptr, SkillInstance, LogTcsStateTree, Error,
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
			UE_CVLOG(GameInstance == nullptr, SkillInstance, LogTcsStateTree, Error,
				TEXT("[%s] StateTree %s: Could not find required game instance"),
				*FString(__FUNCTION__),
				*GetNameSafe(Context.GetStateTree()));
			IssuesFoundCounter += GameInstance != nullptr ? 0 : 1;
		}
		else
		{
			UE_CVLOG(true, SkillInstance, LogTcsStateTree, Error,
				TEXT("[%s] StateTree %s: Unsupported external data request %s"),
				*FString(__FUNCTION__),
				*GetNameSafe(Context.GetStateTree()),
				*GetNameSafe(DataDesc.Struct));
			IssuesFoundCounter += 1;
		}
	}

	return IssuesFoundCounter == 0;
}

UTcsSTSchema_Skill::FTcsContextDataSetter::FTcsContextDataSetter(
	TNotNull<const UTcsSkillInstance*> InSkillInstance,
	TNotNull<const UTcsSkillEntry*> InSkillEntry,
	FStateTreeExecutionContext& Context)
	: SkillInstance(InSkillInstance)
	, SkillEntry(InSkillEntry)
	, ExecutionContext(Context)
{
}

TNotNull<const UStateTree*> UTcsSTSchema_Skill::FTcsContextDataSetter::GetStateTree() const
{
	return ExecutionContext.GetStateTree();
}

TNotNull<const UTcsSTSchema_Skill*> UTcsSTSchema_Skill::FTcsContextDataSetter::GetSchema() const
{
	return Cast<UTcsSTSchema_Skill>(ExecutionContext.GetStateTree()->GetSchema());
}

bool UTcsSTSchema_Skill::FTcsContextDataSetter::SetContextDataByName(
	FName Name,
	FStateTreeDataView DataView)
{
	return ExecutionContext.IsValid() ? ExecutionContext.SetContextDataByName(Name, DataView) : false;
}

void UTcsSTSchema_Skill::SetContextData(
	FTcsContextDataSetter& ContextDataSetter,
	bool bLogErrors) const
{
	const UTcsSTSchema_Skill* Schema = ContextDataSetter.GetSchema();
	const UTcsSkillInstance* SkillInstance = ContextDataSetter.GetSkillInstance();
	const UTcsSkillEntry* SkillEntry = ContextDataSetter.GetSkillEntry();

	if (!Schema || !SkillInstance || !SkillEntry)
	{
		if (bLogErrors)
		{
			UE_LOG(LogTcsStateTree, Error,
				TEXT("%s Expected StateTree asset to contain UTcsSTSchema_Skill, UTcsSkillInstance and UTcsSkillEntry. StateTree will not update."),
				*FString(__FUNCTION__));
		}
		return;
	}

	ContextDataSetter.SetContextDataByName(
		TcsSkillStateContextName::SkillInstance,
		FStateTreeDataView(const_cast<UTcsSkillInstance*>(SkillInstance)));
	ContextDataSetter.SetContextDataByName(
		TcsSkillStateContextName::SkillEntry,
		FStateTreeDataView(const_cast<UTcsSkillEntry*>(SkillEntry)));
}