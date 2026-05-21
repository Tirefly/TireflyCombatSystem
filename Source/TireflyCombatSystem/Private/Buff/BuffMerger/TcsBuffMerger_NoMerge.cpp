// Copyright Tirefly. All Rights Reserved.

#include "Buff/BuffMerger/TcsBuffMerger_NoMerge.h"



ETcsBuffMergeDependencyFlags UTcsBuffMerger_NoMerge::GetDependencyFlags_Implementation() const
{
	return ETcsBuffMergeDependencyFlags::MemberSet;
}



void UTcsBuffMerger_NoMerge::Merge_Implementation(
	TArray<UTcsBuffInstance*>& BuffsToMerge,
	TArray<UTcsBuffInstance*>& MergedBuffs,
	TArray<UTcsBuffInstance*>& MergedOutBuffs)
{
	MergedBuffs.Reset();
	MergedOutBuffs.Reset();

	// 不合并，直接返回所有状态
	MergedBuffs = BuffsToMerge;
}