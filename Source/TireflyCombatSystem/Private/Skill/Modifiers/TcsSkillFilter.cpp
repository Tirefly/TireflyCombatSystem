#include "Skill/Modifiers/TcsSkillFilter.h"

#include "Skill/TcsSkillInstance.h"

void UTcsSkillFilter::Filter_Implementation(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills)
{
	// 默认实现：不做筛选
}
