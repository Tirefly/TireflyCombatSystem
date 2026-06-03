// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"

#include "Attribute/TcsAttributeDefinition.h"



float UTcsAttrClampStrategy_Linear::Clamp_Implementation(
	float Value,
	float MinValue,
	float MaxValue,
	const FTcsAttributeClampContextBase& Context,
	const FInstancedStruct& Config)
{
	// 简单策略不需要使用 Context 和 Config
	return FMath::Clamp(Value, MinValue, MaxValue);
}

bool UTcsAttrClampStrategy_Linear::CollectDependentAttributes_Implementation(
	FName AttributeName,
	const UTcsAttributeDefinition* AttributeDef,
	const FInstancedStruct& Config,
	TArray<FName>& OutAttributeNames) const
{
	OutAttributeNames.Reset();

	if (!AttributeDef)
	{
		return false;
	}

	const FTcsAttributeRange& Range = AttributeDef->AttributeRange;
	if (Range.MinValueType == ETcsAttributeRangeType::ART_Dynamic && !Range.MinValueAttribute.IsNone())
	{
		OutAttributeNames.AddUnique(Range.MinValueAttribute);
	}

	if (Range.MaxValueType == ETcsAttributeRangeType::ART_Dynamic && !Range.MaxValueAttribute.IsNone())
	{
		OutAttributeNames.AddUnique(Range.MaxValueAttribute);
	}

	return true;
}
