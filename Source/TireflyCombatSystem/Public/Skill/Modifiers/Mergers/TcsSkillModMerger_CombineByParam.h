#pragma once

#include "Skill/Modifiers/TcsSkillModifierMerger.h"
#include "TcsSkillModMerger_CombineByParam.generated.h"

/**
 * 按参数键合并：占位实现，后续在聚合阶段具体化
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModMerger_CombineByParam : public UTcsSkillModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers) override;
};
