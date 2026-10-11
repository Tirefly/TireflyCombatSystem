// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "Clock/TcsClockSubsystem.h"

#include "Skill/TcsCastOps.h"
#include "TcsSkillLogChannel.h"



// 时钟读口
UTcsClockSubsystem* UTcsSkillSubsystem::GetClockSubsystem() const
{
	// 与状态门面逐字同款（"取另一个子系统的指针"是任何调用方都要做的事，
	// 藏起来只会让下一处再往 friend 名单加一个名字——返回裸指针即"可能为空"的显式声明）
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsClockSubsystem>() : nullptr;
}



// 施法运行态：终结 / 打断 / 查询（薄壳一行转发——门面签名稳定，流程逻辑住引擎函数）
bool UTcsSkillSubsystem::TerminateCastRun(FTcsCastRunHandle RunHandle)
{
	return FTcsCastOps::TerminateRun(*this, RunHandle);
}

bool UTcsSkillSubsystem::CancelCast(FTcsCastRunHandle RunHandle)
{
	return FTcsCastOps::CancelCast(*this, RunHandle);
}

bool UTcsSkillSubsystem::IsInterruptibleNow(FTcsCastRunHandle RunHandle)
{
	return FTcsCastOps::IsInterruptibleNow(*this, RunHandle);
}

bool UTcsSkillSubsystem::CanMoveNow(FTcsCastRunHandle RunHandle)
{
	return FTcsCastOps::CanMoveNow(*this, RunHandle);
}

int32 UTcsSkillSubsystem::GetCastPhaseIndex(FTcsCastRunHandle RunHandle) const
{
	const FTcsCastRun* Run = FTcsCastOps::Find(const_cast<UTcsSkillSubsystem&>(*this), RunHandle);

	// **悬空句柄返回 `-1`**（不是 0）：0 是**合法下标**（第 1 段）⇒ 用 0 当"没有"会让
	// "第 1 段"与"查不到"读数相同——那正是"标称验 X 的检查没证明 X 的前提"这一类缺陷的温床。
	return Run ? Run->PhaseIndex : -1;
}

FGameplayTag UTcsSkillSubsystem::GetCastActiveChainId(FTcsCastRunHandle RunHandle) const
{
	const FTcsCastRun* Run = FTcsCastOps::Find(const_cast<UTcsSkillSubsystem&>(*this), RunHandle);
	return Run ? Run->ActiveChainId : FGameplayTag();
}



// 链重定向栈
bool UTcsSkillSubsystem::MountChainRedirect(const FTcsChainRedirectEntry& Entry, FTcsSourceHandle Source)
{
	return ChainRedirects.Mount(Entry, Source);
}

int32 UTcsSkillSubsystem::UnmountChainRedirectsBySource(FTcsSourceHandle Source)
{
	return ChainRedirects.UnmountBySource(Source);
}

int32 UTcsSkillSubsystem::GetChainRedirectCount() const
{
	return ChainRedirects.Num();
}

void UTcsSkillSubsystem::SetChainRedirectResolver(TScriptInterface<ITcsChainRedirectResolver> InResolver)
{
	ChainRedirectResolver = InResolver;

	// **`nullptr` 是配置状态不是错误**（同 `SetEntityQuery` 的口径）：撤销注入后，
	// 解析路径不再报"未实现"——没有东西被拒绝求值
	UE_LOG(LogTcsSkill, Log, TEXT("技能门面注入链重定向契约：%s"),
		InResolver.GetObject()
			? *InResolver.GetObject()->GetName()
			: TEXT("（已清空，解析路径不再报该档未实现）"));
}
