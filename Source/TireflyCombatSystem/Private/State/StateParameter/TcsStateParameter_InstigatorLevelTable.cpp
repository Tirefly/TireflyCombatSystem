// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateParameter_InstigatorLevelTable.h"

#include "TcsCombatEntityInterface.h"



void UTcsStateParamParser_InstigatorLevelTable::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto InstigatorLevelTableParam = InstancedStruct.GetPtr<FTcsStateParam_InstigatorLevelTable>())
	{
		if (!Instigator || !InstigatorLevelTableParam->CurveTableRowHandle.IsValid(__FUNCTION__))
		{
			OutValue = InstigatorLevelTableParam->DefaultValue;
			return;
		}

		// 获取施法者等级
		int32 InstigatorLevel = -1;
		if (Instigator->GetClass()->ImplementsInterface(UTcsCombatEntityInterface::StaticClass()))
		{
			InstigatorLevel = ITcsCombatEntityInterface::Execute_GetCombatEntityLevel(Instigator);
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