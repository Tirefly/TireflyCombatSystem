// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"


float UTcsAttrClampStrategy_Linear::Clamp_Implementation(
	float Value,
	float MinValue,
	float MaxValue,
	const FTcsAttributeClampContextBase& Context,
	const FInstancedStruct& Config)
{
	// 简单策略不需要使用 Context 和 Config
	return FMath::Clamp(Value, MinValue, MaxValue);
}
