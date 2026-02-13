// Copyright Tirefly. All Rights Reserved.


#include "State/StateParameter/TcsStateNumericParameter_StateLevelMap.h"

#include "State/TcsState.h"



bool UTcsStateNumericParamEvaluator_StateLevelMap::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto LevelMapParam = InstancedStruct.GetPtr<FTcsStateNumericParam_StateLevelMap>())
	{
		if (!StateInstance)
		{
			OutValue = LevelMapParam->DefaultValue;
			return true;
		}

		const int32 StateLevel = StateInstance->GetLevel();
		const TMap<int32, float>& LevelValues = LevelMapParam->LevelValues;

		// 检查等级是否在映射表中
		if (const float* Value = LevelValues.Find(StateLevel))
		{
			OutValue = *Value;
		}
		else
		{
			// 等级不在映射表中，使用默认值
			OutValue = LevelMapParam->DefaultValue;
		}

		return true;
	}

	return false;
}
