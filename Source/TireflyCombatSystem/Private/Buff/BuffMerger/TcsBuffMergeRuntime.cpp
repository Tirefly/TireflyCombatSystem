// Copyright Tirefly. All Rights Reserved.


#include "Buff/BuffMerger/TcsBuffMergeRuntime.h"



FString TcsBuffMergeRuntime::FormatDirtyReasons(ETcsBuffMergeDirtyReason DirtyReasons)
{
	if (DirtyReasons == ETcsBuffMergeDirtyReason::None)
	{
		return TEXT("None");
	}

	TArray<FString> Names;
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::MembershipChanged))
	{
		Names.Add(TEXT("MembershipChanged"));
	}
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::RuntimeValueChanged))
	{
		Names.Add(TEXT("RuntimeValueChanged"));
	}
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ExecutionStageChanged))
	{
		Names.Add(TEXT("ExecutionStageChanged"));
	}
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::SlotGateChanged))
	{
		Names.Add(TEXT("SlotGateChanged"));
	}
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ForceRebuild))
	{
		Names.Add(TEXT("ForceRebuild"));
	}

	return FString::Join(Names, TEXT("|"));
}

FString TcsBuffMergeRuntime::FormatDependencyFlags(ETcsBuffMergeDependencyFlags DependencyFlags)
{
	if (DependencyFlags == ETcsBuffMergeDependencyFlags::None)
	{
		return TEXT("None");
	}

	TArray<FString> Names;
	if (EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::MemberSet))
	{
		Names.Add(TEXT("MemberSet"));
	}
	if (EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::ApplyTimestamp))
	{
		Names.Add(TEXT("ApplyTimestamp"));
	}
	if (EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::Instigator))
	{
		Names.Add(TEXT("Instigator"));
	}
	if (EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::RuntimeStack))
	{
		Names.Add(TEXT("RuntimeStack"));
	}
	if (EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::ExecutionStage))
	{
		Names.Add(TEXT("ExecutionStage"));
	}
	if (EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::SlotGateState))
	{
		Names.Add(TEXT("SlotGateState"));
	}

	return FString::Join(Names, TEXT("|"));
}

int32 TcsBuffMergeRuntime::CountValidMembers(const FTcsBuffMergeGroupRuntime& GroupRuntime)
{
	int32 ValidMembers = 0;
	for (const TWeakObjectPtr<UTcsBuffInstance>& Entry : GroupRuntime.Members)
	{
		if (Entry.IsValid())
		{
			ValidMembers++;
		}
	}

	return ValidMembers;
}