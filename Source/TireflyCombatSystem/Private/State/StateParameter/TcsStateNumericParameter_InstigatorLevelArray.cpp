// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateNumericParameter_InstigatorLevelArray.h"
#include "TcsEntityInterface.h"



bool UTcsStateNumericParamEvaluator_InstigatorLevelArray::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float& OutValue) const
{
	if (auto InstigatorLevelArrayParam = Payload.GetPtr<FTcsStateNumericParam_InstigatorLevelArray>())
	{
		if (!Instigator)
		{
			OutValue = InstigatorLevelArrayParam->DefaultValue;
			return true;
		}

		// 获取施法者等级
		int32 InstigatorLevel = -1;
		if (Instigator->GetClass()->ImplementsInterface(UTcsEntityInterface::StaticClass()))
		{
			InstigatorLevel = ITcsEntityInterface::Execute_GetCombatEntityLevel(Instigator);
		}

		const TArray<float>& LevelValues = InstigatorLevelArrayParam->LevelValues;

		// 数组下标直接对应等级值（LevelValues[0] 对应等级 0）。
		if (LevelValues.IsValidIndex(InstigatorLevel))
		{
			OutValue = LevelValues[InstigatorLevel];
		}
		else if (LevelValues.Num() > 0 && InstigatorLevel >= LevelValues.Num())
		{
			OutValue = LevelValues.Last();
		}
		else
		{
			// 未精确匹配且未超限，查找最近的 LowerKey
			bool bLocated = false;
			for (int32 i = 0; i < LevelValues.Num(); ++i)
			{
				if (InstigatorLevel > i)
				{
					OutValue = LevelValues[i];
					bLocated = true;
					break;
				}
			}
			if (!bLocated)
			{
				OutValue = InstigatorLevelArrayParam->DefaultValue;
			}
		}

		return true;
	}

	return false;
} 