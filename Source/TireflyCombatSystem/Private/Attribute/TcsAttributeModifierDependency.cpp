// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierDependency.h"

#include "Buff/TcsBuffInstance.h"



FTcsAttributeModifierDependencyKey FTcsAttributeModifierDependencyKey::MakeAttributeCurrentValue(
	FName InAttributeId)
{
	FTcsAttributeModifierDependencyKey Key;
	Key.Type = ETcsAttributeModifierDependencyType::AMDT_AttributeCurrentValue;
	Key.AttributeId = InAttributeId;
	return Key;
}

FTcsAttributeModifierDependencyKey FTcsAttributeModifierDependencyKey::MakeBuffNumericStateParam(
	const UTcsBuffInstance& BuffInstance,
	FGameplayTag InStateParamTag)
{
	FTcsAttributeModifierDependencyKey Key;
	Key.Type = ETcsAttributeModifierDependencyType::AMDT_BuffNumericStateParamEffectiveValue;
	Key.SourceBuffObjectKey = FObjectKey(&BuffInstance);
	Key.StateParamTag = InStateParamTag;
	return Key;
}
