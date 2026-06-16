// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsSkillEntrySelector.generated.h"



class UTcsSkillComponent;
class UTcsSkillEntry;


/**
 * SkillEntry 选取策略基类。
 *
 * 应用 SkillModifier 时，通过 ResolveTargets 确定修改哪些 SkillEntry。
 */
UCLASS(Abstract, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsSkillEntrySelector : public UObject
{
	GENERATED_BODY()

public:
	virtual TArray<UTcsSkillEntry*> ResolveTargets(
		const FInstancedStruct& Config,
		UTcsSkillComponent* SkillComp) const PURE_VIRTUAL(, return {};);
};
