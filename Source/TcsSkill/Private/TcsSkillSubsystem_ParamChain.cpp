// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "Skill/TcsParamChain.h"



// 参数链：施加与摘除（薄壳一行转发——门面签名稳定，流程逻辑住引擎函数）
bool UTcsSkillSubsystem::ApplyParamModifiers(
	FTcsCombatEntityHandle Entity,
	const FTcsSkillEntrySelector& Selector,
	const TArray<FTcsNumericParamModifier>& Modifiers,
	FTcsSourceHandle Source)
{
	return FTcsParamChainOps::ApplyParamModifiers(*this, Entity, Selector, Modifiers, Source);
}

int32 UTcsSkillSubsystem::RemoveParamModifiersBySource(
	FTcsCombatEntityHandle Entity,
	FTcsSourceHandle Source)
{
	return FTcsParamChainOps::RemoveParamModifiersBySource(*this, Entity, Source);
}
