// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TireflyStateParameter_StateLevelArray.h"

#include "State/TireflyState.h"



void UTireflyStateParamParser_StateLevelArray::ParseStateParameter_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto LevelArrayParam = InstancedStruct.GetPtr<FTireflyStateParam_StateLevelArray>())
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