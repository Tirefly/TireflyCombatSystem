// Copyright Tirefly. All Rights Reserved.

#include "TcsSourceHandle.h"



namespace
{
	int32 NextSourceHandleId = -1;
}



FTcsSourceHandle FTcsSourceHandleFactory::CreateRootSourceHandle(
	AActor* Instigator,
	const FGameplayTagContainer& SourceTags)
{
	return FTcsSourceHandle(++NextSourceHandleId, TArray<FPrimaryAssetId>(), Instigator, SourceTags);
}

FTcsSourceHandle FTcsSourceHandleFactory::CreateChildSourceHandle(
	const FTcsSourceHandle& ParentSourceHandle,
	const FPrimaryAssetId& DirectParentSourceDefId,
	AActor* Instigator,
	const FGameplayTagContainer& SourceTags)
{
	if (!ParentSourceHandle.IsValid() || !DirectParentSourceDefId.IsValid())
	{
		return FTcsSourceHandle();
	}

	TArray<FPrimaryAssetId> ChildCausalityChain = ParentSourceHandle.CausalityChain;
	ChildCausalityChain.Add(DirectParentSourceDefId);

	return FTcsSourceHandle(++NextSourceHandleId, ChildCausalityChain, Instigator, SourceTags);
}

#if WITH_AUTOMATION_TESTS
void FTcsSourceHandleFactory::ResetForTests()
{
	NextSourceHandleId = -1;
}
#endif

bool FTcsSourceHandle::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// 序列化 ID
	Ar << Id;

	// 序列化 SourceTags
	SourceTags.NetSerialize(Ar, Map, bOutSuccess);

	// 条件序列化 Instigator (只在有效时才序列化)
	uint8 bHasInstigator = 0;
	if (Ar.IsSaving())
	{
		bHasInstigator = Instigator.IsValid() ? 1 : 0;
	}
	Ar.SerializeBits(&bHasInstigator, 1);

	if (bHasInstigator)
	{
		UObject* InstigatorObject = Instigator.Get();
		Map->SerializeObject(Ar, AActor::StaticClass(), InstigatorObject);

		if (Ar.IsLoading())
		{
			Instigator = Cast<AActor>(InstigatorObject);
		}
	}

	// 序列化 CausalityChain
	int32 ChainNum = CausalityChain.Num();
	Ar << ChainNum;

	if (Ar.IsLoading())
	{
		CausalityChain.SetNum(ChainNum);
	}

	for (int32 i = 0; i < ChainNum; ++i)
	{
		Ar << CausalityChain[i];
	}

	bOutSuccess = true;
	return true;
}
