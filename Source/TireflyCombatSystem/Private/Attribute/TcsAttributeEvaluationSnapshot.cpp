// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeEvaluationSnapshot.h"



bool FTcsAttributeEvaluationSnapshot::GetBaseValue(FName AttributeId, float& OutValue) const
{
	if (const float* const Value = BaseValues.Find(AttributeId))
	{
		OutValue = *Value;
		return true;
	}

	OutValue = 0.f;
	return false;
}

bool FTcsAttributeEvaluationSnapshot::GetCurrentValue(FName AttributeId, float& OutValue) const
{
	if (const float* const Value = CurrentValues.Find(AttributeId))
	{
		OutValue = *Value;
		return true;
	}

	OutValue = 0.f;
	return false;
}
