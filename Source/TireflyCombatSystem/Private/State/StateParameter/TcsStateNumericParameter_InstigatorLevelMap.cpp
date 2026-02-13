// Copyright Tirefly. All Rights Reserved.


#include "State/StateParameter/TcsStateNumericParameter_InstigatorLevelMap.h"
#include "TcsEntityInterface.h"



bool UTcsStateNumericParamEvaluator_InstigatorLevelMap::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto InstigatorLevelMapParam = InstancedStruct.GetPtr<FTcsStateNumericParam_InstigatorLevelMap>())
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

		// 检查等级是否在映射表中
		if (const float* Value = LevelValues.Find(InstigatorLevel))
		{
			OutValue = *Value;
		}
		else
		{
			// 等级不在映射表中，使用默认值
			OutValue = InstigatorLevelMapParam->DefaultValue;
		}

		return true;
	}

	return false;
}
