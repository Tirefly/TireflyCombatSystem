// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillOps.h"

#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



// 授予
bool FTcsSkillOps::Grant(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	FTcsSkillEntryHandle* OutHandle)
{
	if (!Unit.IsValid())
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("技能授予被拒：单位句柄无效"));
		return false;
	}

	// **身份无效与"身份合法但未登记"是两条不同的拒绝路径**（2026-10-08 订正）：
	// 前者是调用方传错（空 tag），后者是内容缺口。两者都拒绝，但**读数必须可区分**——
	// 初版合成一条 "定义 %s 未在本世界登记"，空 tag 会打印成 `None`，读起来像"有个叫 None 的定义"
	// （实际是"根本没给身份"），把调用方错误误报成内容缺口。
	if (!DefTag.IsValid())
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("技能授予被拒：DefTag 无效（空身份——调用方未给出定义身份）"));
		return false;
	}

	// **授予前置门禁**：定义未在本世界登记 ⇒ 拒绝。
	// MUST NOT 建出一个取不到定义内容的条目——那样三个读取面全部落空，
	// 而失败面表现为"读数为 0"而非"没学到"（静默错误）。
	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(DefTag);
	if (!Def)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("技能授予被拒：定义 %s 未在本世界登记（内容缺口不是契约违规）"),
			*DefTag.ToString());
		return false;
	}

	// 来源未声明 ⇒ 门面发号（同状态侧 Apply 的缺省口径；发号器进程唯一、永不复用）
	if (!Source.IsValid())
	{
		Source = Subsystem.SourceRegistry.Allocate();
	}

	FSkillBucket& Bucket = Subsystem.Registry.FindOrAddBucket(Unit);

	// 重复授予同一 DefTag ⇒ **刷新**既有条目（同一单位"会同一个技能"只有一条事实），不新建第二条
	FTcsSkillEntryHandle Existing;
	bool bFound = false;
	Bucket.ForEach([&DefTag, &Existing, &bFound](const FTcsLearnedSkillEntry& Entry)
	{
		if (Entry.DefTag == DefTag)
		{
			Existing = Entry.Handle;
			bFound = true;
			return false;
		}

		return true;
	});

	FTcsSkillEntryHandle Handle;
	if (bFound)
	{
		Handle = Existing;
		FTcsLearnedSkillEntry* Entry = Bucket.Find(Handle);
		if (Entry)
		{
			// 刷新：等级取定义下界、来源换成本次的学习来源
			// （**运行中升级不追溯**——已冻结在其他运行态里的等级不受影响）
			Entry->Level = Def->LevelBase;
			Entry->LearnSource = Source;
		}
	}
	else
	{
		Handle = Bucket.AllocateSlot();
		FTcsLearnedSkillEntry* Entry = Bucket.Find(Handle);
		if (!Entry)
		{
			// 分配后必然可解析（同一次调用内无增删）——防御性返回
			UE_LOG(LogTcsSkill, Warning, TEXT("技能授予异常：新分配槽位无法解析（%d/%d）"),
				Handle.Index, Handle.Generation);
			return false;
		}

		Entry->DefTag = DefTag;
		Entry->Handle = Handle;
		Entry->Unit = Unit;
		Entry->Level = Def->LevelBase;
		Entry->LearnSource = Source;
		Entry->RunHandles.Reset();
	}

	if (OutHandle)
	{
		*OutHandle = Handle;
	}

	const FTcsLearnedSkillEntry* Granted = Bucket.Find(Handle);
	UE_LOG(LogTcsSkill, Log, TEXT("技能授予%s：单位=%lld 定义=%s 条目=%d/%d 等级=%d"),
		bFound ? TEXT("（刷新）") : TEXT(""),
		Unit.Id, *DefTag.ToString(), Handle.Index, Handle.Generation,
		Granted ? Granted->Level : 0);

	return true;
}



// 撤销
bool FTcsSkillOps::Revoke(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle)
{
	if (!Handle.IsValid())
	{
		// 无效句柄 = 调用方手上的句柄过期（时序竞态），不是配置错误 ⇒ Warning，不 ensure
		UE_LOG(LogTcsSkill, Warning, TEXT("技能撤销被拒：句柄无效（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("技能撤销被拒：单位 %lld 未注册"), Unit.Id);
		return false;
	}

	const FTcsLearnedSkillEntry* Entry = Bucket->Find(Handle);
	if (!Entry)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("技能撤销被拒：句柄悬空（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	Bucket->ReleaseSlot(static_cast<uint32>(Handle.Index));

	UE_LOG(LogTcsSkill, Log, TEXT("技能撤销：单位=%lld 条目=%d/%d"), Unit.Id, Handle.Index, Handle.Generation);
	return true;
}

int32 FTcsSkillOps::RevokeBySource(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSourceHandle Source)
{
	if (!Source.IsValid())
	{
		// 无效来源不构成"要摘谁"——0 命中，不 ensure（同款口径 = 状态侧的时长操作拒绝面）
		return 0;
	}

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		return 0;
	}

	// **两段式**：桶的 ForEach 期间 MUST NOT 增删（会改动槽位与空闲栈）⇒ 先收集命中句柄，再逐个撤销
	TArray<FTcsSkillEntryHandle> Matched;
	Bucket->ForEach([&Source, &Matched](const FTcsLearnedSkillEntry& Entry)
	{
		if (Entry.LearnSource == Source)
		{
			Matched.Add(Entry.Handle);
		}

		return true;
	});

	for (const FTcsSkillEntryHandle& Handle : Matched)
	{
		Bucket->ReleaseSlot(static_cast<uint32>(Handle.Index));
	}

	if (Matched.Num() > 0)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("技能按来源级联撤销：单位=%lld 来源=%llu 摘除 %d 条"),
			Unit.Id, Source.Id, Matched.Num());
	}

	return Matched.Num();
}

int32 FTcsSkillOps::UnregisterUnit(UTcsSkillSubsystem& Subsystem, FTcsCombatEntityHandle Unit)
{
	const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	const int32 Count = Bucket ? Bucket->Num() : 0;

	// 账本条目零外部资源（不挂修正器、不挂触发行、无时间条目）⇒ 无需逐条撤销流程，
	// 直接删桶即可（**这正是"只持一个 LearnSource"的收益**：没有需要配对摘除的东西）。
	if (Subsystem.Registry.RemoveBucket(Unit) && Count > 0)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("技能注销单位：单位=%lld 摘除 %d 条"), Unit.Id, Count);
	}

	return Count;
}



// 访问
const FTcsLearnedSkillEntry* FTcsSkillOps::Find(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle)
{
	return Subsystem.Registry.Find(Unit, Handle);
}

void FTcsSkillOps::ForEachEntry(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor)
{
	if (const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit))
	{
		Bucket->ForEach(Visitor);
	}
}

int32 FTcsSkillOps::CountEntries(UTcsSkillSubsystem& Subsystem, FTcsCombatEntityHandle Unit)
{
	const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	return Bucket ? Bucket->Num() : 0;
}

int32 FTcsSkillOps::CountAllEntries(UTcsSkillSubsystem& Subsystem)
{
	return Subsystem.Registry.NumEntries();
}
