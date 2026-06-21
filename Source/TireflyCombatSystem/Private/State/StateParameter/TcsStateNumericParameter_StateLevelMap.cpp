// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateNumericParameter_StateLevelMap.h"

#include "State/TcsStateInstance.h"



bool UTcsStateNumericParamEvaluator_StateLevelMap::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float& OutValue) const
{
	if (auto LevelMapParam = Payload.GetPtr<FTcsStateNumericParam_StateLevelMap>())
	{
		if (!StateInstance)
		{
			OutValue = LevelMapParam->DefaultValue;
			return true;
		}

		const int32 StateLevel = StateInstance->GetLevel();
		const TMap<int32, float>& LevelValues = LevelMapParam->LevelValues;

		// 检查等级是否在映射表中精确匹配
		if (const float* Value = LevelValues.Find(StateLevel))
		{
			OutValue = *Value;
		}
		else
		{
			// 未精确匹配，查找最近的小于等于当前等级的 Key，避免跳变到 DefaultValue
			int32 BestKey = INDEX_NONE;
			for (const TPair<int32, float>& Pair : LevelValues)
			{
				if (Pair.Key <= StateLevel && (BestKey == INDEX_NONE || Pair.Key > BestKey))
				{
					BestKey = Pair.Key;
				}
			}

			if (BestKey != INDEX_NONE)
			{
				OutValue = LevelValues[BestKey];
			}
			else
			{
				OutValue = LevelMapParam->DefaultValue;
			}
		}

		return true;
	}

	return false;
}
