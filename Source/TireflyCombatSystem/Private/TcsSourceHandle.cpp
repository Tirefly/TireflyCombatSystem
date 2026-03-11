// Copyright Tirefly. All Rights Reserved.


#include "TcsSourceHandle.h"



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
