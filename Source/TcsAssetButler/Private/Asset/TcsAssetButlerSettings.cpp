// Copyright Tirefly. All Rights Reserved.

#include "Asset/TcsAssetButlerSettings.h"



FName UTcsAssetButlerSettings::GetCategoryName() const
{
	return TEXT("Tcs");
}



ETcsAssetLoadPolicy UTcsAssetButlerSettings::ResolveLoadPolicy(
	FPrimaryAssetType Type, ETcsAssetLoadPolicy Fallback) const
{
	// 配置优先 → 回落宏声明。只有这一处判据（两处判据 = 两套真相）。
	if (const ETcsAssetLoadPolicy* Configured = LoadPolicyByType.Find(Type))
	{
		return *Configured;
	}

	return Fallback;
}
