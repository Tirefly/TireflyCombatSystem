// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsChainRedirect.h"

#include "TcsSkillLogChannel.h"



// 挂载与摘除
bool FTcsChainRedirectRegistry::Mount(const FTcsChainRedirectEntry& Entry, FTcsSourceHandle Source)
{
	// **来源无效 ⇒ 拒绝**（无锚点的条目摘不掉 ⇒ 违反成对性判据）；
	// 同款守卫先例 = `FTcsParamChainOps::ApplyParamModifiers` 的无效来源分支。
	if (!Source.IsValid())
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("链重定向挂载被拒：来源句柄无效（单位=%lld）——无来源锚点则条目摘不掉，本次零挂载"),
			Entry.Unit.Id);
		return false;
	}

	// **身份无效 ⇒ 拒绝**（无效 tag 的记录永远不可能命中，挂着只是占位）。
	// 两条身份各有其判据：目标链是"被替换的谁"、替换链是"换成谁"——缺任一条都表达不出让渡。
	if (!Entry.TargetChainId.IsValid() || !Entry.ReplacementChainId.IsValid())
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("链重定向挂载被拒：链身份无效（单位=%lld 目标=%s 替换=%s）"),
			Entry.Unit.Id, *Entry.TargetChainId.ToString(), *Entry.ReplacementChainId.ToString());
		return false;
	}

	FRecord Record;
	Record.Entry = Entry;
	Record.Source = Source;
	Record.Sequence = NextSequence++;

	Records.Add(MoveTemp(Record));

	UE_LOG(LogTcsSkill, Log,
		TEXT("链重定向已挂载：单位=%lld %s → %s 优先级=%d 来源=%llu（在册 %d 条）"),
		Entry.Unit.Id, *Entry.TargetChainId.ToString(), *Entry.ReplacementChainId.ToString(),
		Entry.Priority, Source.Id, Records.Num());

	return true;
}

int32 FTcsChainRedirectRegistry::UnmountBySource(FTcsSourceHandle Source)
{
	if (!Source.IsValid())
	{
		// 无效来源不构成"要摘谁"——0 命中，不 ensure（同 `RemoveParamModifiersBySource` 的口径）
		return 0;
	}

	const int32 Before = Records.Num();
	Records.RemoveAll([&Source](const FRecord& Record) { return Record.Source == Source; });
	const int32 Removed = Before - Records.Num();

	// **0 条命中 = 正常路径，MUST NOT ensure**（来源可能只挂过已摘除的记录）
	if (Removed > 0)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("链重定向按来源摘除：来源=%llu 摘除 %d 条（在册 %d 条）"),
			Source.Id, Removed, Records.Num());
	}

	return Removed;
}



// 解析
FGameplayTag FTcsChainRedirectRegistry::Resolve(FTcsCombatEntityHandle Unit, FGameplayTag GlobalChainId) const
{
	// **未命中任何记录 ⇒ 原样返回**（**正常路径**：既不是错误，也 MUST NOT 报"未实现"）。
	// 本函数因此**没有"未找到"的日志分支**——把正常路径也报出来会让常规命令的输出失去信号意义。
	const FRecord* Best = nullptr;

	for (const FRecord& Record : Records)
	{
		// **命中的第一段键 = `(单位, 目标链)`**：两个都要等。
		// **只比 `TargetChainId` 会跨单位误命中**（甲身上"链 A → B"的让渡不该影响乙），
		// 那是"重定向泄漏到别人身上"的一类缺陷。
		if (Record.Entry.Unit != Unit || Record.Entry.TargetChainId != GlobalChainId)
		{
			continue;
		}

		if (!Best)
		{
			Best = &Record;
			continue;
		}

		// **多条命中 ⇒ `Priority` 大者胜、打平取后挂者**（设计原词"后挂/高优先"）。
		//
		// **两个判据都 MUST 覆盖**（只验其一无法区分两种实现）：`>` 而非 `>=` 使"打平"落到
		// "不替换"这一支 ⇒ 保留**先**命中的那条；而 `Records` 是**挂载序数组** ⇒ 先命中的就是
		// **先挂**的。于是"打平"必须反过来——**打平时应取后挂者** ⇒ 本处用 `>=`：
		// 打平时替换为当前这条（更后挂的那条），实现"后挂者胜"。
		//
		// **⚠ 判据在此**：`Priority > Best->Priority` → 高优先胜；`Priority == Best->Priority` → 取后者。
		// 合起来即 `>=`（因为 `Best` 总是**更早**挂的那条，替换即"取后挂者"）。
		if (Record.Entry.Priority >= Best->Entry.Priority)
		{
			Best = &Record;
		}
	}

	if (!Best)
	{
		return GlobalChainId;
	}

	UE_LOG(LogTcsSkill, Log,
		TEXT("链重定向解析：单位=%lld %s → %s（优先级=%d 挂载序=%llu）"),
		Unit.Id, *GlobalChainId.ToString(), *Best->Entry.ReplacementChainId.ToString(),
		Best->Entry.Priority, Best->Sequence);

	return Best->Entry.ReplacementChainId;
}



// 观测与生命周期
int32 FTcsChainRedirectRegistry::Num() const
{
	return Records.Num();
}

void FTcsChainRedirectRegistry::Reset()
{
	Records.Reset();

	// **发号器不复位**：它只用于栈内"后挂者"的相对比较，复位与否都不影响裁决正确性；
	// 但"只增不回退"使读数（挂载序）在跨世界后仍可区分，且与全仓其余发号器的
	// "进程内唯一、永不复用"口径一致（同 `FTcsSourceHandleRegistry` 的处置）。
}
