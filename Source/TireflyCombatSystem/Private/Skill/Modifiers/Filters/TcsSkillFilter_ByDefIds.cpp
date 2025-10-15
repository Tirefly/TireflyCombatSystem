#include "Skill/Modifiers/Filters/TcsSkillFilter_ByDefIds.h"

#include "Skill/TcsSkillComponent.h"
#include "Skill/TcsSkillInstance.h"

void UTcsSkillFilter_ByDefIds::Filter_Implementation(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills)
{
	OutSkills.Reset();

	if (!SkillOwner || SkillDefIds.IsEmpty())
	{
		return;
	}

	if (UTcsSkillComponent* SkillComponent = SkillOwner->FindComponentByClass<UTcsSkillComponent>())
	{
		for (const FName& SkillId : SkillDefIds)
		{
			if (UTcsSkillInstance* SkillInstance = SkillComponent->GetSkillInstance(SkillId))
			{
				OutSkills.Add(SkillInstance);
			}
		}
	}
}
