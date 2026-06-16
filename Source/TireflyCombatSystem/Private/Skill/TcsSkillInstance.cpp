// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillInstance.h"

#include "Skill/TcsSkillEntry.h"
#include "StateTree/Schema/TcsSTSchema_Skill.h"
#include "TcsLogChannels.h"



UTcsSkillInstance::UTcsSkillInstance()
{
}

UWorld* UTcsSkillInstance::GetWorld() const
{
	return Super::GetWorld();
}


int32 UTcsSkillInstance::GetLevel() const
{
	return SkillEntry ? SkillEntry->GetLevel() : Super::GetLevel();
}


FTcsNumericStateParamInstance* UTcsSkillInstance::GetNumericParamInstance(FGameplayTag Tag)
{
	return SkillEntry
		? SkillEntry->NumericParamInstances.Find(Tag)
		: Super::GetNumericParamInstance(Tag);
}


FTcsBoolStateParamInstance* UTcsSkillInstance::GetBoolParamInstance(FGameplayTag Tag)
{
	return SkillEntry
		? SkillEntry->BoolParamInstances.Find(Tag)
		: Super::GetBoolParamInstance(Tag);
}


FTcsVectorStateParamInstance* UTcsSkillInstance::GetVectorParamInstance(FGameplayTag Tag)
{
	return SkillEntry
		? SkillEntry->VectorParamInstances.Find(Tag)
		: Super::GetVectorParamInstance(Tag);
}


TMap<FGameplayTag, FTcsNumericStateParamInstance>& UTcsSkillInstance::GetNumericParamInstances()
{
	return SkillEntry
		? SkillEntry->NumericParamInstances
		: Super::GetNumericParamInstances();
}


TMap<FGameplayTag, FTcsBoolStateParamInstance>& UTcsSkillInstance::GetBoolParamInstances()
{
	return SkillEntry
		? SkillEntry->BoolParamInstances
		: Super::GetBoolParamInstances();
}


TMap<FGameplayTag, FTcsVectorStateParamInstance>& UTcsSkillInstance::GetVectorParamInstances()
{
	return SkillEntry
		? SkillEntry->VectorParamInstances
		: Super::GetVectorParamInstances();
}


bool UTcsSkillInstance::SetContextRequirements(FStateTreeExecutionContext& Context)
{
	if (!Context.IsValid())
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("Invalid Skill StateTree execution context"));
		return false;
	}

	if (!SkillEntry)
	{
		UE_LOG(LogTcsStateTree, Error,
			TEXT("%s Skill runtime '%s' does not have a valid SkillEntry."),
			*FString(__FUNCTION__),
			*GetNameSafe(this));
		return false;
	}

	Context.SetCollectExternalDataCallback(
		FOnCollectStateTreeExternalData::CreateUObject(
			this,
			&UTcsSkillInstance::CollectExternalData
		)
	);

	return UTcsSTSchema_Skill::SetContextRequirements(*this, *SkillEntry, Context);
}

bool UTcsSkillInstance::CollectExternalData(
	const FStateTreeExecutionContext& Context,
	const UStateTree* StateTree,
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
	TArrayView<FStateTreeDataView> OutDataViews)
{
	return UTcsSTSchema_Skill::CollectExternalData(
		Context,
		StateTree,
		this,
		SkillEntry,
		ExternalDataDescs,
		OutDataViews);
}
