// Copyright Tirefly. All Rights Reserved.


#include "State/StateCondition/TireflyStateCondition_ParameterBased.h"

#include "TireflyCombatSystemLogChannels.h"
#include "State/TireflyState.h"


bool UTireflyStateCondition_ParameterBased::CheckCondition_Implementation(
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float CurrentGameTime)
{
	const FTireflyStateConditionPayload_ParameterBased* Config = Payload.GetPtr<FTireflyStateConditionPayload_ParameterBased>();
	if (!Config || !StateInstance)
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Config or StateInstance is null."), *FString(__FUNCTION__));
		return false;
	}

	// 获取参数值
	float ParameterValue = StateInstance->GetParamValue(Config->ParameterName);

	// 执行比较
	float CompareValue = Config->CompareValue;
	switch (Config->ComparisonType)
	{
	case ETireflyNumericComparison::GreaterThan:
		return ParameterValue > CompareValue;
	case ETireflyNumericComparison::GreaterThanOrEqual:
		return ParameterValue >= CompareValue;
	case ETireflyNumericComparison::LessThan:
		return ParameterValue < CompareValue;
	case ETireflyNumericComparison::LessThanOrEqual:
		return ParameterValue <= CompareValue;
	case ETireflyNumericComparison::Equal:
		return FMath::IsNearlyEqual(ParameterValue, CompareValue);
	case ETireflyNumericComparison::NotEqual:
		return !FMath::IsNearlyEqual(ParameterValue, CompareValue);
	default:
		return false;
	}
} 