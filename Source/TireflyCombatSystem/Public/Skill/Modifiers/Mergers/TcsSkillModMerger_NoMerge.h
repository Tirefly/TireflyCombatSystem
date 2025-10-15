#pragma once

#include "Skill/Modifiers/TcsSkillModifierMerger.h"
#include "TcsSkillModMerger_NoMerge.generated.h"

/**
 * 默认：不做额外合并，直接透传
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModMerger_NoMerge : public UTcsSkillModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers) override;
};
