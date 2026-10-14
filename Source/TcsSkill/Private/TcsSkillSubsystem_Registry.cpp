// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "Skill/TcsCastOps.h"
#include "Skill/TcsSkillOps.h"
#include "TcsSkillLogChannel.h"



// 激活（薄壳：门禁裁决与运行态全部住 FTcsCastOps——设计文档 §3.1 的既定分工）
ESkillActivateResult UTcsSkillSubsystem::TryActivate(
	FTcsCombatEntityHandle Entity,
	FGameplayTag DefTag,
	FTcsSkillInstanceHandle* OutRun)
{
	return FTcsCastOps::Activate(*this, Entity, DefTag, OutRun);
}

// **`TerminateSkillInstance` / `CancelCast` / 查询契约住在 `TcsSkillSubsystem_CastPhase.cpp`**
// （Task 5 把施法运行态面的薄壳归到那一片，与本片的"账本面薄壳"分开——
// 本文件保留 `TryActivate` / `GetSkillInstance*` / 授予撤销 / 访问，改动面最小）。

const FTcsSkillInstance* UTcsSkillSubsystem::GetSkillInstance(FTcsSkillInstanceHandle RunHandle) const
{
	return FTcsCastOps::Find(*const_cast<UTcsSkillSubsystem*>(this), RunHandle);
}

int32 UTcsSkillSubsystem::GetSkillInstanceCount() const
{
	return FTcsCastOps::CountInstances(*const_cast<UTcsSkillSubsystem*>(this));
}



// 授予与撤销（薄壳：流程逻辑住 FTcsSkillOps——设计文档 §3.1 的既定分工）
bool UTcsSkillSubsystem::GrantSkill(
	FTcsCombatEntityHandle Entity,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	FTcsSkillEntryHandle* OutHandle)
{
	return FTcsSkillOps::Grant(*this, Entity, DefTag, Source, OutHandle);
}

bool UTcsSkillSubsystem::RevokeSkill(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Handle)
{
	return FTcsSkillOps::Revoke(*this, Entity, Handle);
}

int32 UTcsSkillSubsystem::RevokeSkillBySource(FTcsCombatEntityHandle Entity, FTcsSourceHandle Source)
{
	return FTcsSkillOps::RevokeBySource(*this, Entity, Source);
}

int32 UTcsSkillSubsystem::UnregisterEntity(FTcsCombatEntityHandle Entity)
{
	return FTcsSkillOps::UnregisterEntity(*this, Entity);
}



// 访问（同样薄壳）
const FTcsLearnedSkillEntry* UTcsSkillSubsystem::GetEntry(
	FTcsCombatEntityHandle Entity,
	FTcsSkillEntryHandle Handle) const
{
	return FTcsSkillOps::Find(*const_cast<UTcsSkillSubsystem*>(this), Entity, Handle);
}

void UTcsSkillSubsystem::ForEachEntry(
	FTcsCombatEntityHandle Entity,
	TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor) const
{
	FTcsSkillOps::ForEachEntry(*const_cast<UTcsSkillSubsystem*>(this), Entity, Visitor);
}

int32 UTcsSkillSubsystem::GetEntryCount(FTcsCombatEntityHandle Entity) const
{
	return FTcsSkillOps::CountEntries(*const_cast<UTcsSkillSubsystem*>(this), Entity);
}

int32 UTcsSkillSubsystem::GetTotalEntryCount() const
{
	return FTcsSkillOps::CountAllEntries(*const_cast<UTcsSkillSubsystem*>(this));
}
