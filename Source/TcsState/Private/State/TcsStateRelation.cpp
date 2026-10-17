// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateRelation.h"



bool FTcsStateRelation::AppliesTo(const FGameplayTagContainer& SubjectTags, const FTcsRelationRow& Row)
{
	// 主体词无效 ⇒ 该行不作用于任何主体（空词 SHOULD 由作者期拦，此处不 ensure——
	// 运行期静默不命中是明确结果，与「脏句柄不 ensure」同款口径）
	if (!Row.AppliesToTag.IsValid())
	{
		return false;
	}

	// **层级匹配**：行上的主体词作「被匹配词」，实例词集里任一命中它即算。
	// 方向判据（引擎源码 `GameplayTagContainer.h`）：父词命中子词、反向不成立 ⇒
	// 作者填**父词**即覆盖一族，填具体词只作用它自己。
	// MUST NOT 反向写（把实例词拿去 MatchesTag 行的词）——那会让「填父词覆盖一族」失效。
	return SubjectTags.HasTag(Row.AppliesToTag);
}



bool FTcsStateRelation::CheckRequired(const FGameplayTagContainer& TargetTags, const FTcsRelationRow& Row)
{
	// 无前置 ⇒ 恒满足（空容器是合法配置，不是缺省故障）
	if (Row.RequiredTags.IsEmpty())
	{
		return true;
	}

	// 逐词判「必须有」：任一词未被覆盖即不满足。
	// 覆盖判据 = `TargetTags.HasTag(要求词)`（层级：实例侧的具体词命中要求侧的父词）。
	for (const FGameplayTag& Required : Row.RequiredTags)
	{
		if (!TargetTags.HasTag(Required))
		{
			return false;
		}
	}

	return true;
}



bool FTcsStateRelation::CheckForbidden(const FGameplayTagContainer& TargetTags, const FTcsRelationRow& Row)
{
	// 无禁区 ⇒ 恒不命中
	if (Row.ForbiddenTags.IsEmpty() || TargetTags.IsEmpty())
	{
		return false;
	}

	// **双向层级匹配 MUST 都查**（这是本函数最容易做错的一处）：
	// - 方向一：实例侧的具体词 ⊆ 禁区侧的父词（作者填父词封一族）；
	// - 方向二：实例侧的父词覆盖禁区侧的具体词（实例是「一族」，禁区点了一个具体词）。
	// 只查一个方向会让另一侧的层级关系静默失效——两个方向都合法，故都 MUST 查。
	if (TargetTags.HasAny(Row.ForbiddenTags))
	{
		return true;
	}

	for (const FGameplayTag& Target : TargetTags)
	{
		if (Row.ForbiddenTags.HasTag(Target))
		{
			return true;
		}
	}

	return false;
}



bool FTcsStateRelation::CollectBlocks(const FGameplayTagContainer& CandidateTags, const FTcsRelationRow& Row)
{
	if (Row.TagsToBlock.IsEmpty() || CandidateTags.IsEmpty())
	{
		return false;
	}

	// 同 `CheckForbidden`：两个方向的层级关系都合法，MUST 都查
	if (Row.TagsToBlock.HasAny(CandidateTags))
	{
		return true;
	}

	for (const FGameplayTag& Blocked : Row.TagsToBlock)
	{
		if (CandidateTags.HasTag(Blocked))
		{
			return true;
		}
	}

	return false;
}



bool FTcsStateRelation::CollectCancels(const FGameplayTagContainer& CandidateTags, const FTcsRelationRow& Row)
{
	if (Row.TagsToCancel.IsEmpty() || CandidateTags.IsEmpty())
	{
		return false;
	}

	if (Row.TagsToCancel.HasAny(CandidateTags))
	{
		return true;
	}

	for (const FGameplayTag& Canceled : Row.TagsToCancel)
	{
		if (CandidateTags.HasTag(Canceled))
		{
			return true;
		}
	}

	return false;
}
