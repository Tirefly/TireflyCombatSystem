// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TireflyStateParameter_InstigatorLevelArray.h"
#include "TireflyCombatEntityInterface.h"



void UTireflyStateParamParser_InstigatorLevelArray::ParseStateParameter_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto InstigatorLevelArrayParam = InstancedStruct.GetPtr<FTireflyStateParam_InstigatorLevelArray>())
	{
		if (!Instigator)
		{
			OutValue = InstigatorLevelArrayParam->DefaultValue;
			return;
		}

		// 获取施法者等级
		int32 InstigatorLevel = -1;
		if (Instigator->GetClass()->ImplementsInterface(UTireflyCombatEntityInterface::StaticClass()))
		{
			InstigatorLevel = ITireflyCombatEntityInterface::Execute_GetCombatEntityLevel(Instigator);
		}

		const TArray<float>& LevelValues = InstigatorLevelArrayParam->LevelValues;

		// 检查等级是否在数组范围内
		if (LevelValues.IsValidIndex(InstigatorLevel))
		{
			OutValue = LevelValues[InstigatorLevel];
		}
		else
		{
			// 等级超出范围，使用默认值
			OutValue = InstigatorLevelArrayParam->DefaultValue;
		}
	}
} 