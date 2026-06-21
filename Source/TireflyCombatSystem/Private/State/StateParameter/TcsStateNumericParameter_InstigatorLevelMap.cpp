// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateNumericParameter_InstigatorLevelMap.h"
#include "TcsEntityInterface.h"



bool UTcsStateNumericParamEvaluator_InstigatorLevelMap::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float& OutValue) const
{
	if (auto InstigatorLevelMapParam = Payload.GetPtr<FTcsStateNumericParam_InstigatorLevelMap>())
	{
		if (!Instigator)
		{
			OutValue = InstigatorLevelMapParam->DefaultValue;
			return true;
		}

		// 获取施法者等级
		int32 InstigatorLevel = -1;
		if (Instigator->GetClass()->ImplementsInterface(UTcsEntityInterface::StaticClass()))
		{
			InstigatorLevel = ITcsEntityInterface::Execute_GetCombatEntityLevel(Instigator);
		}

		const TMap<int32, float>& LevelValues = InstigatorLevelMapParam->LevelValues;

		// 检查等级是否在映射表中精确匹配
		if (const float* Value = LevelValues.Find(InstigatorLevel))
		{
			OutValue = *Value;
		}
		else
		{
			// 未精确匹配，查找最近的小于等于当前等级的 Key，避免跳变到 DefaultValue
			int32 BestKey = INDEX_NONE;
			for (const TPair<int32, float>& Pair : LevelValues)
			{
				if (Pair.Key <= InstigatorLevel && (BestKey == INDEX_NONE || Pair.Key > BestKey))
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
				OutValue = InstigatorLevelMapParam->DefaultValue;
			}
		}

		return true;
	}

	return false;
}
