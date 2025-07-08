// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TireflyStateParameter_StateLevelTable.h"

#include "State/TireflyState.h"



void UTireflyStateParamParser_StateLevelTable::ParseStateParameter_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto StateLevelTableParam = InstancedStruct.GetPtr<FTireflyStateParam_StateLevelTable>())
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