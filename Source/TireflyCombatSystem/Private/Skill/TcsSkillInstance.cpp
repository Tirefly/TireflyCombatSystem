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