// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttribute.h"

#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"


FTcsAttributeDefinition::FTcsAttributeDefinition()
{
	// 设置默认 Clamp 策略为线性 Clamp
	// 这确保了所有属性定义（包括现有的和新创建的）都有一个有效的 Clamp 策略
	ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
}
