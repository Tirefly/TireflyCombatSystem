// Copyright Tirefly. All Rights Reserved.


#include "State/TireflyStateCondition.h"


bool FTireflyStateConditionConfig::IsValid() const
{
	return ConditionClass != nullptr && Payload.IsValid();
} 