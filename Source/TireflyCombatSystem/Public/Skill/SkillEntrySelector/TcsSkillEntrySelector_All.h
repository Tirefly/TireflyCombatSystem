// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillEntrySelector/TcsSkillEntrySelector.h"
#include "TcsSkillEntrySelector_All.generated.h"



// SkillEntry 选择器：全部已学技能
UCLASS(Blueprintable, Meta = (DisplayName = "SkillEntry 选择器：全部已学技能"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillEntrySelector_All : public UTcsSkillEntrySelector
{
	GENERATED_BODY()

public:
	virtual TArray<UTcsSkillEntry*> ResolveTargets(
		const FInstancedStruct& Config,
		UTcsSkillComponent* SkillComp) const override;
};
