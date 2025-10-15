#include "Skill/Modifiers/TcsSkillModifierUtils.h"

#include "Skill/Modifiers/TcsSkillModifierDefinition.h"

bool UTcsSkillModifierUtils::ValidateDefinitionRow(const FTcsSkillModifierDefinition& Definition, FString& OutError)
{
	if (Definition.ModifierName.IsNone())
	{
		OutError = TEXT("ModifierName is None");
		return false;
	}

	if (!Definition.ExecutionType)
	{
		OutError = TEXT("ExecutionType is not set");
		return false;
	}

	if (!Definition.MergeType)
	{
		OutError = TEXT("MergeType is not set");
		return false;
	}

	OutError.Reset();
	return true;
}
