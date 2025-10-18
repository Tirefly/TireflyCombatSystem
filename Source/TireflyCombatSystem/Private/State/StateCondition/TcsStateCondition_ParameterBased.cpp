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
	float ParameterValue;
	StateInstance->GetNumericParam(Config->ParameterName, ParameterValue);

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