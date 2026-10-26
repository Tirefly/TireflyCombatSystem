// Copyright Tirefly. All Rights Reserved.

#include "State/TcsBuffOps.h"

#include "Def/TcsBuffDef.h"
#include "State/TcsBuffEnums.h"
#include "State/TcsStateRelation.h"
#include "TcsBuffLogChannel.h"
#include "TcsBuffSubsystem.h"



namespace
{
	/**
	 * 把一个 Def 的四个关系数组合成**关系行视图**（共享层认得的纯值形状）。
	 *
	 * **为什么在这里做合成**：`FTcsRelationRow` 住 `TcsState`（共享层），它 MUST NOT 认识
	 * `FTcsBuffDef`（域类型）⇒ 「Def 数组 → 行视图」这一步只能由域侧完成。
	 * 这正是 `e1` 的 D1 分期设计的兑现处：**行类型一处、来源两处**——
	 * 本变更只接 Def 数组这一个来源，`e2` 的中心 `UDataAsset` 会复用同一类型。
	 *
	 * `AppliesToTag` 由调用方给（对 Def 上的平铺数组而言，主体即**本定义自己**：
	 * 用它的 `DefTag` 作主体词——具体词只作用于它自己；`e2` 的中心表才填父词覆盖一族）。
	 */
	FTcsRelationRow MakeSelfRelationRow(const FTcsBuffDef& Def, FGameplayTag SelfTag)
	{
		FTcsRelationRow Row;
		Row.AppliesToTag = SelfTag;
		Row.RequiredTags = FGameplayTagContainer::CreateFromArray(Def.RequiredTags);
		Row.ForbiddenTags = FGameplayTagContainer::CreateFromArray(Def.ForbiddenTags);
		Row.TagsToBlock = FGameplayTagContainer::CreateFromArray(Def.TagsToBlock);
		Row.TagsToCancel = FGameplayTagContainer::CreateFromArray(Def.TagsToCancel);
		return Row;
	}

	/**
	 * 收集某实体在册实例的**可寻址词集**（判定用的客体词面）。
	 *
	 * 判据 = 共享层认得的**最小公共字段集**（同 `FTcsEntityAdmissionExisting` 的手法）：
	 * 每个实例贡献「其定义的全部 **`CategoryTags` ∪ `{DefTag}`**」。
	 *
	 * **★ 为什么并 `DefTag`（2026-10-24 用户裁定）**：判据原话是「这两个合起来，相当于 GAS 里
	 * GA 和 GE 都有的 `AssetTags` 集合」——`AssetTags` 就是**该资产拥有的全部词**，
	 * 不分「身份词」与「类别词」⇒ **凡"拿一个定义的词去匹配"的地方，词集 MUST 是并集**。
	 * `DefTag` 作匹配词时**只命中它自己**（具体词语义）、`CategoryTags` 的父词覆盖一族，
	 * 两者在**同一次 `HasTag` 层级匹配**里各得其果。
	 *
	 * **原文的误推 MUST 记录以免重犯**：本条曾写「`DefTag` 与 `CategoryTags` 是两个独立通道，
	 * MUST NOT 混为并集」——**该口径已于 2026-10-17 被 `DEC-2026-10-17` §4 取代**，
	 * 而本文件当时按旧口径实施（裁定为真、落地为假）。
	 */
	void CollectLiveCategoryTags(
		UTcsBuffSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		FGameplayTagContainer& OutTags)
	{
		Subsystem.ForEachState(Entity, [&Subsystem, &OutTags](const FTcsBuffInstance& Instance)
		{
			if (const FTcsBuffDef* Def = Subsystem.GetRegisteredBuffDef(Instance.DefTag))
			{
				// **并集：类别词 + 定义身份词**（见上方注释的裁定与理由）
				OutTags.AppendTags(Def->CategoryTags);
				OutTags.AddTag(Instance.DefTag);
			}
			return true;
		});
	}
}



bool FTcsBuffOps::CheckApplyGate(
	UTcsBuffSubsystem& Subsystem,
	FTcsCombatEntityHandle Target,
	FGameplayTag DefTag,
	const FTcsBuffDef& Def)
{
	// **快速路径的判据 MUST 含「我自己」与「在跑者」两面**（★ 本条曾写错，由装置 R5 时机① 抓出）。
	//
	// **错误形态（MUST NOT 回到）**：只看「**我自己**四数组全空 ⇒ return true」。
	// 它把 ③ 反向查找一起跳过了——而反向查找问的是「**别人**是否阻断我」，
	// 与我自己的字段**无关**：B 自己四数组全空，却仍应被在跑的 Blocker 拦住。
	// 装置实测：R5 时机①（目标后到）以 FAIL 报出，正是这条快速路径让 B 恒过。
	//
	// **正确判据**：只有当「我自己无受动约束」**且**「该实体上没有别的实例带 `TagsToBlock`」
	// 时才可跳过；后者要扫一遍桶才知道 ⇒ 该扫描**无法省**（这正是「反向查找」这条裁定的代价，
	// 已于 D4 记入设计：作者负担 vs 每次 apply 一次 O(该实体在册实例数) 扫描）。
	//
	// **既有资产仍是零行为变化**：它们四数组全空、且没有实例带 `TagsToBlock` ⇒ 扫描后必然
	// 落到「无阻断者」⇒ 与快速路径同结果（装置 R2/R3 的阴性对照仍绿）。
	FGameplayTagContainer LiveTags;
	CollectLiveCategoryTags(Subsystem, Target, LiveTags);

	const FTcsRelationRow SelfRow = MakeSelfRelationRow(Def, DefTag);

	// ① 受动·前置（正）：必须有
	if (!FTcsStateRelation::CheckRequired(LiveTags, SelfRow))
	{
		UE_LOG(LogTcsBuff, Log,
			TEXT("关系门拒绝（前置不满足）：实体=%lld 定义=%s 需要=%s 当前=%s"),
			Target.Id, *DefTag.ToString(), *SelfRow.RequiredTags.ToString(), *LiveTags.ToString());
		return false;
	}

	// ② 受动·前置（反）：必须缺席
	if (FTcsStateRelation::CheckForbidden(LiveTags, SelfRow))
	{
		UE_LOG(LogTcsBuff, Log,
			TEXT("关系门拒绝（禁区命中）：实体=%lld 定义=%s 禁区=%s 当前=%s"),
			Target.Id, *DefTag.ToString(), *SelfRow.ForbiddenTags.ToString(), *LiveTags.ToString());
		return false;
	}

	// ③ **反向查找（施动侧的单向声明）**：是否存在**存活实例**的 `TagsToBlock` 覆盖我。
	// 这是 2026-10-16 用户裁定的那一支（「apply 反向查找（单向声明即生效）」）——
	// 若改取「要求作者双向声明」，此处就没有这段扫描，代价转移到作者负担与「漏写即静默失效」。
	// **扫描域 = 该实体桶**（`ForEachState` 按实体找桶），MUST NOT 全局扫描。
	bool bBlocked = false;
	FGameplayTag BlockerTag;
	Subsystem.ForEachState(Target, [&Subsystem, &DefTag, &Def, &bBlocked, &BlockerTag](const FTcsBuffInstance& Instance)
	{
		// 自己不算阻挡者（同一 DefTag 的重申请走共存决策，不归关系门）
		if (Instance.DefTag == DefTag)
		{
			return true;
		}

		const FTcsBuffDef* LiveDef = Subsystem.GetRegisteredBuffDef(Instance.DefTag);
		if (!LiveDef || LiveDef->TagsToBlock.Num() == 0)
		{
			return true;
		}

		const FTcsRelationRow BlockerRow = MakeSelfRelationRow(*LiveDef, Instance.DefTag);
		// 候选面 = 我的可寻址词集（类别词 ∪ 我的 DefTag）——与判定客体面同口径
		FGameplayTagContainer CandidateTags = Def.CategoryTags;
		CandidateTags.AddTag(DefTag);
		if (FTcsStateRelation::CollectBlocks(CandidateTags, BlockerRow))
		{
			bBlocked = true;
			BlockerTag = Instance.DefTag;
			return false;
		}

		return true;
	});

	if (bBlocked)
	{
		UE_LOG(LogTcsBuff, Log,
			TEXT("关系门拒绝（被在跑实例阻断）：实体=%lld 定义=%s 阻断者=%s 我的类别=%s"),
			Target.Id, *DefTag.ToString(), *BlockerTag.ToString(), *Def.CategoryTags.ToString());
		return false;
	}

	return true;
}



void FTcsBuffOps::ApplyCancelAndBlock(
	UTcsBuffSubsystem& Subsystem,
	FTcsCombatEntityHandle Target,
	FGameplayTag DefTag,
	const FTcsBuffDef& Def)
{
	// 两数组全空 ⇒ 无动作（既有资产现状；零行为变化）
	if (Def.TagsToCancel.Num() == 0 && Def.TagsToBlock.Num() == 0)
	{
		return;
	}

	const FTcsRelationRow SelfRow = MakeSelfRelationRow(Def, DefTag);

	// **两段式遍历**（桶的硬纪律：遍历期间 MUST NOT 增删实例——
	// 移除会改槽位与空闲栈）⇒ 先收集句柄，遍历结束后再动。
	TArray<FTcsBuffInstanceHandle> ToCancel;
	Subsystem.ForEachState(Target, [&Subsystem, &DefTag, &SelfRow, &ToCancel](const FTcsBuffInstance& Instance)
	{
		// 自己不动自己
		if (Instance.DefTag == DefTag)
		{
			return true;
		}

		const FTcsBuffDef* LiveDef = Subsystem.GetRegisteredBuffDef(Instance.DefTag);
		if (!LiveDef)
		{
			return true;
		}

		// 客体词面 = 该在册实例所属定义的类别词集
		FGameplayTagContainer LiveTags = LiveDef->CategoryTags;
		LiveTags.AddTag(Instance.DefTag);
		if (FTcsStateRelation::CollectCancels(LiveTags, SelfRow))
		{
			ToCancel.Add(Instance.Handle);
		}

		return true;
	});

	// 逐条走**同一条移除链**（撤时间条目 + 摘修正器 + 广播 + 归还槽位）——MUST NOT 另写一套。
	// 原因传 `EBRC_Cancelled`：**该枚举首次有内建产生者**（此前只有注释自述「由调用方显式传入」）。
	for (const FTcsBuffInstanceHandle& Handle : ToCancel)
	{
		UE_LOG(LogTcsBuff, Log,
			TEXT("关系表取消在跑状态：实体=%lld 施动者=%s 目标句柄=%d/%d"),
			Target.Id, *DefTag.ToString(), Handle.Index, Handle.Generation);

		Subsystem.RemoveState(Handle, EBuffRemoveCause::EBRC_Cancelled);
	}

	// **`TagsToBlock` 的即时阻断面（第二处时机）**：本状态开始时，对其 `TagsToBlock` 已覆盖的
	// **已在跑**目标，同样按 `TagsToCancel` 处置——即「我已开始，你就不能继续存在」。
	// **为什么与取消合并处置而不是另立一份**：两者的落地动作逐字相同（把目标终止掉），
	// 差别只在**语义来源**（事前拦截 vs 事后终止）；而「目标后到时无法开始」那一半
	// 已由 `CheckApplyGate` 的 ③ 反向查找承担 ⇒ 两处时机在此合拢，无重复实现。
	TArray<FTcsBuffInstanceHandle> ToBlock;
	Subsystem.ForEachState(Target, [&Subsystem, &DefTag, &SelfRow, &ToBlock](const FTcsBuffInstance& Instance)
	{
		if (Instance.DefTag == DefTag)
		{
			return true;
		}

		const FTcsBuffDef* LiveDef = Subsystem.GetRegisteredBuffDef(Instance.DefTag);
		if (!LiveDef)
		{
			return true;
		}

		FGameplayTagContainer LiveTags = LiveDef->CategoryTags;
		LiveTags.AddTag(Instance.DefTag);
		if (FTcsStateRelation::CollectBlocks(LiveTags, SelfRow))
		{
			ToBlock.Add(Instance.Handle);
		}

		return true;
	});

	for (const FTcsBuffInstanceHandle& Handle : ToBlock)
	{
		UE_LOG(LogTcsBuff, Log,
			TEXT("关系表阻断在跑状态：实体=%lld 施动者=%s 目标句柄=%d/%d"),
			Target.Id, *DefTag.ToString(), Handle.Index, Handle.Generation);

		Subsystem.RemoveState(Handle, EBuffRemoveCause::EBRC_Cancelled);
	}
}
