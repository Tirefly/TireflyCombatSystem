// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "Skill/TcsSkillOps.h"
#include "TcsSkillLogChannel.h"



// 授予与撤销（薄壳：流程逻辑住 FTcsSkillOps——设计文档 §3.1 的既定分工）
bool UTcsSkillSubsystem::GrantSkill(
	FTcsCombatEntityHandle Unit,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	FTcsSkillEntryHandle* OutHandle)
{
	return FTcsSkillOps::Grant(*this, Unit, DefTag, Source, OutHandle);
}

bool UTcsSkillSubsystem::RevokeSkill(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Handle)
{
	return FTcsSkillOps::Revoke(*this, Unit, Handle);
}

int32 UTcsSkillSubsystem::RevokeSkillBySource(FTcsCombatEntityHandle Unit, FTcsSourceHandle Source)
{
	return FTcsSkillOps::RevokeBySource(*this, Unit, Source);
}

int32 UTcsSkillSubsystem::UnregisterUnit(FTcsCombatEntityHandle Unit)
{
	return FTcsSkillOps::UnregisterUnit(*this, Unit);
}



// 访问（同样薄壳）
const FTcsLearnedSkillEntry* UTcsSkillSubsystem::GetEntry(
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle) const
{
	return FTcsSkillOps::Find(*const_cast<UTcsSkillSubsystem*>(this), Unit, Handle);
}

void UTcsSkillSubsystem::ForEachEntry(
	FTcsCombatEntityHandle Unit,
	TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor) const
{
	FTcsSkillOps::ForEachEntry(*const_cast<UTcsSkillSubsystem*>(this), Unit, Visitor);
}

int32 UTcsSkillSubsystem::GetEntryCount(FTcsCombatEntityHandle Unit) const
{
	return FTcsSkillOps::CountEntries(*const_cast<UTcsSkillSubsystem*>(this), Unit);
}

int32 UTcsSkillSubsystem::GetTotalEntryCount() const
{
	return FTcsSkillOps::CountAllEntries(*const_cast<UTcsSkillSubsystem*>(this));
}
