// Copyright Tirefly. All Rights Reserved.


#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy_UseOldest.h"
#include "State/TcsState.h"


int64 UTcsStateSamePriorityPolicy_UseOldest::GetOrderKey_Implementation(const UTcsStateInstance* State) const
{
	if (!IsValid(State))
	{
		return 0;
	}

	// 使用负的 ApplyTimestamp 作为排序键，越旧越大
	return -State->GetApplyTimestamp();
}
