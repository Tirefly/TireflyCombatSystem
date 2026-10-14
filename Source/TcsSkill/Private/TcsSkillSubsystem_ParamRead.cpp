// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "Skill/TcsSkillOps.h"



// 账本参数读取面（薄壳：求值逻辑住 FTcsSkillOps_ParamRead——设计文档 §3.1 的既定分工）
int32 UTcsSkillSubsystem::GetLevel(
	FTcsCombatEntityHandle Entity,
	FTcsSkillEntryHandle Handle,
	const TArray<double>& LevelModifiers) const
{
	return FTcsSkillOps::GetLevel(*const_cast<UTcsSkillSubsystem*>(this), Entity, Handle, LevelModifiers);
}

bool UTcsSkillSubsystem::GetNumericParam(
	FTcsCombatEntityHandle Entity,
	FTcsSkillEntryHandle Handle,
	FGameplayTag Key,
	double& OutValue) const
{
	return FTcsSkillOps::GetNumericParam(*const_cast<UTcsSkillSubsystem*>(this), Entity, Handle, Key, OutValue);
}

bool UTcsSkillSubsystem::IsSwitchSet(
	FTcsCombatEntityHandle Entity,
	FTcsSkillEntryHandle Handle,
	FGameplayTag Key,
	bool& OutValue) const
{
	return FTcsSkillOps::IsSwitchSet(*const_cast<UTcsSkillSubsystem*>(this), Entity, Handle, Key, OutValue);
}
