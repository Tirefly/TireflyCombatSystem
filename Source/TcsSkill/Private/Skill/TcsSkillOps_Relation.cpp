// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillOps_Relation.h"

#include "Def/TcsSkillDefData.h"
#include "State/TcsStateLiveQuery.h"
#include "State/TcsStateRelation.h"
#include "TcsSkillLogChannel.h"



bool FTcsSkillOps_Relation::CheckRelationGate(
	ITcsStateLiveQuery* LiveQuery,
	FTcsCombatEntityHandle Entity,
	FGameplayTag DefTag,
	const FTcsSkillDefData& Def)
{
	// 四数组全空 ⇒ 恒过（**既有资产的现状** ⇒ 零行为变化，本变更的阴性基线）
	if (Def.RequiredTags.Num() == 0 && Def.ForbiddenTags.Num() == 0
		&& Def.TagsToBlock.Num() == 0 && Def.TagsToCancel.Num() == 0)
	{
		return true;
	}

	// **契约未注册的降级（MUST 留具名日志，MUST NOT 静默）**：注册方（状态域）未就位时，
	// 走「无在跑状态」的保守路径——MUST NOT 因此拒绝（那会让缺注册变成随机的业务失败）。
	if (!LiveQuery)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("技能关系门：在飞查询契约未注册（状态域未就位）——本次按「无在跑状态」判定（实体=%lld 定义=%s）"),
			Entity.Id, *DefTag.ToString());
		return true;
	}

	// 主体行视图：平铺数组的主体词 = 本定义的 `DefTag`（在并集语义下它**恰好命中自己**）
	// **主体词仍只填 `DefTag`**——因为"本定义的主体是哪个资产"就是它自己；
	// 而"我拿什么词去撞别人的容器"那一侧（候选面）才是并集，见下方 `MyTags`。
	FTcsRelationRow SelfRow;
	SelfRow.AppliesToTag = DefTag;
	SelfRow.RequiredTags = FGameplayTagContainer::CreateFromArray(Def.RequiredTags);
	SelfRow.ForbiddenTags = FGameplayTagContainer::CreateFromArray(Def.ForbiddenTags);
	SelfRow.TagsToBlock = FGameplayTagContainer::CreateFromArray(Def.TagsToBlock);
	SelfRow.TagsToCancel = FGameplayTagContainer::CreateFromArray(Def.TagsToCancel);

	// **一次遍历判定三个方向**（视图两半各有其用）：
	//   ① 受动·前置（正）：在跑状态的**类别词**是否覆盖我的 `RequiredTags`
	//   ② 受动·前置（反）：在跑状态的**类别词**是否落进我的 `ForbiddenTags`
	//   ③ 施动侧反向查找：在跑状态是否用**它的** `TagsToBlock` 覆盖我的**类别词**
	// **① 必须收集全量、②③ 可边遍历边定论**：`RequiredTags` 的语义是「必须**全部**已存在」
	// ⇒ 它要看到全部在跑状态的词面才能回答；②③ 是存在性判定（命中一个即定论）。
	FGameplayTagContainer LiveTags;
	bool bBlocked = false;
	FGameplayTag BlockerTag;

	// **我的可寻址词集 = `CategoryTags` ∪ `{DefTag}`**（2026-10-24 裁定：判据是「该资产拥有的
	// 全部词」，与 GAS 的 `AssetTags` 集合同构）。**在遍历外只算一次**——它在循环里不变。
	FGameplayTagContainer MyTags = Def.CategoryTags;
	MyTags.AddTag(DefTag);

	LiveQuery->ForEachLiveState(Entity, [&SelfRow, &Def, &MyTags, &LiveTags, &bBlocked, &BlockerTag](
		const FTcsLiveStateView& View)
	{
		// 事实半累积（供 ① 的全量判定）——**可寻址词集 = CategoryTags ∪ {DefTag}**
		// （2026-10-24 裁定，判据「相当于 GAS 里 GA 和 GE 都有的 AssetTags 集合」）
		LiveTags.AppendTags(View.CategoryTags);
		LiveTags.AddTag(View.Row.AppliesToTag);

		// ③ 施动侧反向查找：它的 `TagsToBlock` 是否覆盖**我的可寻址词集**
		// 判据 = 用我的并集词作候选面、拿它的行去判（与状态域内那处同款）
		if (!bBlocked && FTcsStateRelation::CollectBlocks(MyTags, View.Row))
		{
			bBlocked = true;
			BlockerTag = View.Row.AppliesToTag;
		}

		return true;
	});

	// ① 受动·前置（正）：必须有
	if (!FTcsStateRelation::CheckRequired(LiveTags, SelfRow))
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("技能关系门拒绝[前置不满足]：实体=%lld 定义=%s 需要=%s 在跑=%s"),
			Entity.Id, *DefTag.ToString(), *SelfRow.RequiredTags.ToString(), *LiveTags.ToString());
		return false;
	}

	// ② 受动·前置（反）：必须缺席
	if (FTcsStateRelation::CheckForbidden(LiveTags, SelfRow))
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("技能关系门拒绝[禁区命中]：实体=%lld 定义=%s 禁区=%s 在跑=%s"),
			Entity.Id, *DefTag.ToString(), *SelfRow.ForbiddenTags.ToString(), *LiveTags.ToString());
		return false;
	}

	// ③ 施动侧反向查找：**单向声明即生效**（作者只写状态那一侧，此处即拦住我）
	// **技能↔技能的阻断判定本批未落**（如实登记）：那需要遍历同实体全部在跑 run 的类别词，
	// 而技能在跑实例的词面今天没有读口 ⇒ MUST NOT 读成本条已覆盖两个方向。
	if (bBlocked)
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("技能关系门拒绝[被在跑状态阻断]：实体=%lld 定义=%s 阻断者=%s 我的类别=%s"),
			Entity.Id, *DefTag.ToString(), *BlockerTag.ToString(), *Def.CategoryTags.ToString());
		return false;
	}

	return true;
}
