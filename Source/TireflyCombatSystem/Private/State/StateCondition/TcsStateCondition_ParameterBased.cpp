// Copyright Tirefly. All Rights Reserved.


#include "State/StateCondition/TcsStateCondition_ParameterBased.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"


bool UTcsStateCondition_ParameterBased::CheckCondition_Implementation(
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float CurrentGameTime)
{
	const FTcsStateConditionPayload_ParameterBased* Config = Payload.GetPtr<FTcsStateConditionPayload_ParameterBased>();
	if (!Config || !StateInstance)
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Config or StateInstance is null."), *FString(__FUNCTION__));
		return false;
	}

	// 获取参数值
	float ParameterValue = 0.0f;
	if (!StateInstance->GetNumericParam(Config->ParameterName, ParameterValue))
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Parameter '%s' not found in state '%s'."),
			*FString(__FUNCTION__),
			*Config->ParameterName.ToString(),
			*StateInstance->GetStateDefId().ToString());
		return false;
	}

	// 执行比较
	float CompareValue = Config->CompareValue;
	switch (Config->ComparisonType)
	{
	case ETcsNumericComparison::GreaterThan:
		return ParameterValue > CompareValue;
	case ETcsNumericComparison::GreaterThanOrEqual:
		return ParameterValue >= CompareValue;
	case ETcsNumericComparison::LessThan:
		return ParameterValue < CompareValue;
	case ETcsNumericComparison::LessThanOrEqual:
		return ParameterValue <= CompareValue;
	case ETcsNumericComparison::Equal:
		return FMath::IsNearlyEqual(ParameterValue, CompareValue);
	case ETcsNumericComparison::NotEqual:
		return !FMath::IsNearlyEqual(ParameterValue, CompareValue);
	default:
		return false;
	}
} 
