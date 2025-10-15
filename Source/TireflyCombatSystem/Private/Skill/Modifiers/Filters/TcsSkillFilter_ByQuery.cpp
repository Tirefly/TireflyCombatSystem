#include "Skill/Modifiers/Filters/TcsSkillFilter_ByQuery.h"

#include "Skill/TcsSkillComponent.h"
#include "Skill/TcsSkillInstance.h"

void UTcsSkillFilter_ByQuery::Filter_Implementation(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills)
{
	OutSkills.Reset();

	if (!SkillOwner)
	{
		return;
	}

	if (UTcsSkillComponent* SkillComponent = SkillOwner->FindComponentByClass<UTcsSkillComponent>())
	{
		const TArray<UTcsSkillInstance*> Learned = SkillComponent->GetAllSkillInstances();
		for (UTcsSkillInstance* SkillInstance : Learned)
		{
			if (!IsValid(SkillInstance))
			{
				continue;
			}

			bool bAccepted = true;

			if (Query.bFilterByStateType)
			{
				const uint8 Type = static_cast<uint8>(SkillInstance->GetSkillDef().StateType);
				if (Type != Query.StateType)
				{
					bAccepted = false;
				}
			}

			if (bAccepted && Query.bFilterBySlotType)
			{
				if (SkillInstance->GetSkillDef().StateSlotType != Query.StateSlotType)
				{
					bAccepted = false;
				}
			}

			if (bAccepted && Query.bFilterByTags)
			{
				const FGameplayTagContainer& Tags = SkillInstance->GetSkillDef().FunctionTags;
				if (!Query.RequiredTags.IsEmpty() && !Tags.HasAll(Query.RequiredTags))
				{
					bAccepted = false;
				}
				if (bAccepted && !Query.BlockedTags.IsEmpty() && Tags.HasAny(Query.BlockedTags))
				{
					bAccepted = false;
				}
			}

			if (bAccepted)
			{
				OutSkills.Add(SkillInstance);
			}
		}
	}
}
