// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateNumericParameter_StateLevelArray.h"

#include "State/TcsStateInstance.h"



bool UTcsStateNumericParamEvaluator_StateLevelArray::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto LevelArrayParam = InstancedStruct.GetPtr<FTcsStateNumericParam_StateLevelArray>())
	{
		if (!StateInstance)
		{
			OutValue = LevelArrayParam->DefaultValue;
			return true;
		}

		const int32 StateLevel = StateInstance->GetLevel();
		const TArray<float>& LevelValues = LevelArrayParam->LevelValues;

		// 数组下标直接对应等级值（LevelValues[0] 对应等级 0）。
		if (LevelValues.IsValidIndex(StateLevel))
		{
			OutValue = LevelValues[StateLevel];
		}
		else if (LevelValues.Num() > 0 && StateLevel >= LevelValues.Num())
		{
			OutValue = LevelValues.Last();
		}
		else
		{
			// 未精确匹配且未超限，查找最近的 LowerKey
			bool bLocated = false;
			for (int32 i = 0; i < LevelValues.Num(); ++i)
			{
				if (StateLevel > i)
				{
					OutValue = LevelValues[i];
					bLocated = true;
					break;
				}
			}
			if (!bLocated)
			{
				OutValue = LevelArrayParam->DefaultValue;
			}
		}

		return true;
	}

	return false;
} 