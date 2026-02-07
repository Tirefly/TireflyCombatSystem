// Copyright Tirefly. All Rights Reserved.


#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy_UseNewest.h"
#include "State/TcsState.h"


int64 UTcsStateSamePriorityPolicy_UseNewest::GetOrderKey_Implementation(const UTcsStateInstance* State) const
{
	if (!IsValid(State))
	{
		return 0;
	}

	// 使用 ApplyTimestamp 作为排序键，越新越大
	return State->GetApplyTimestamp();
}
