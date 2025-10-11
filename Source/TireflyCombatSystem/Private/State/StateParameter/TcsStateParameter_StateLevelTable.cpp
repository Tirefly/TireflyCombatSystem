// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateParameter_StateLevelTable.h"

#include "State/TcsState.h"



void UTcsStateParamParser_StateLevelTable::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto StateLevelTableParam = InstancedStruct.GetPtr<FTcsStateParam_StateLevelTable>())
	{
		if (!Instigator || !StateLevelTableParam->CurveTableRowHandle.IsValid(__FUNCTION__))
		{
			OutValue = StateLevelTableParam->DefaultValue;
			return;
		}

		const int32 StateLevel = StateInstance->GetLevel();
		
		// 从曲线表中获取对应等级的值
		if (const FRealCurve* Curve = StateLevelTableParam->CurveTableRowHandle.GetCurve(__FUNCTION__))
		{
			OutValue = Curve->Eval(StateLevel);
		}
		else
		{
			// 无法找到曲线，使用默认值
			OutValue = StateLevelTableParam->DefaultValue;
		}
	}
} 