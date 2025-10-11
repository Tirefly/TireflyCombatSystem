// Copyright Tirefly. All Rights Reserved.


#include "State/StateCondition/TcsStateCondition.h"


bool FTcsStateConditionConfig::IsValid() const
{
	return ConditionClass != nullptr && Payload.IsValid();
} 