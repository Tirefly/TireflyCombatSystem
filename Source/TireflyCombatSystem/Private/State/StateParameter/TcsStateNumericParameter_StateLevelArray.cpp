// Copyright Tirefly. All Rights Reserved.


#include "State/StateParameter/TcsStateNumericParameter_StateLevelArray.h"

#include "State/TcsState.h"



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

		// 检查等级是否在数组范围内（数组下标直接对应等级值，即 LevelValues[0] 对应等级 0）
		if (LevelValues.IsValidIndex(StateLevel))
		{
			OutValue = LevelValues[StateLevel];
		}
		else
		{
			// 等级超出范围，使用默认值
			OutValue = LevelArrayParam->DefaultValue;
		}

		return true;
	}

	return false;
} 