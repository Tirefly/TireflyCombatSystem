// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateSlot.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy_UseNewest.h"


FTcsStateSlotDefinition::FTcsStateSlotDefinition()
{
	// 默认使用 UseNewest 策略
	SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
}
