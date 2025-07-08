// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TireflyStateParameter_InstigatorLevelTable.h"

#include "TireflyCombatEntityInterface.h"



void UTireflyStateParamParser_InstigatorLevelTable::ParseStateParameter_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto InstigatorLevelTableParam = InstancedStruct.GetPtr<FTireflyStateParam_InstigatorLevelTable>())
	{
		if (!Instigator || !InstigatorLevelTableParam->CurveTableRowHandle.IsValid(__FUNCTION__))
		{
			OutValue = InstigatorLevelTableParam->DefaultValue;
			return;
		}

		// 获取施法者等级
		int32 InstigatorLevel = -1;
		if (Instigator->GetClass()->ImplementsInterface(UTireflyCombatEntityInterface::StaticClass()))
		{
			InstigatorLevel = ITireflyCombatEntityInterface::Execute_GetCombatEntityLevel(Instigator);
		}
		
		// 从曲线表中获取对应等级的值
		if (const FRealCurve* Curve = InstigatorLevelTableParam->CurveTableRowHandle.GetCurve(__FUNCTION__))
		{
			OutValue = Curve->Eval(InstigatorLevel);
		}
		else
		{
			// 无法找到曲线，使用默认值
			OutValue = InstigatorLevelTableParam->DefaultValue;
		}
	}
} 