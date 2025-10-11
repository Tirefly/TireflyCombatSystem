// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateParameter_StateLevelArray.h"

#include "State/TcsState.h"



void UTcsStateParamParser_StateLevelArray::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto LevelArrayParam = InstancedStruct.GetPtr<FTcsStateParam_StateLevelArray>())
	{
		if (!StateInstance)
		{
			OutValue = LevelArrayParam->DefaultValue;
			return;
		}

		const int32 StateLevel = StateInstance->GetLevel();
		const TArray<float>& LevelValues = LevelArrayParam->LevelValues;

		// 检查等级是否在数组范围内
		if (LevelValues.IsValidIndex(StateLevel))
		{
			OutValue = LevelValues[StateLevel];
		}
		else
		{
			// 等级超出范围，使用默认值
			OutValue = LevelArrayParam->DefaultValue;
		}
	}
} 