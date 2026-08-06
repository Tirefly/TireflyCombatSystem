// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsLogChannels.h"



namespace
{
	bool LogAttributeRuntimeNotReady_Query(const UTcsAttributeComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return false;
	}
}



bool UTcsAttributeComponent::GetAttributeValue(FName AttributeName, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->CurrentValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::HasAttributeByTag(const FGameplayTag& AttributeTag) const
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = const_cast<UTcsAttributeComponent*>(this)->ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	return Attributes.Contains(AttributeName);
}

bool UTcsAttributeComponent::GetAttributeValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = const_cast<UTcsAttributeComponent*>(this)->ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	return GetAttributeValue(AttributeName, OutValue);
}

bool UTcsAttributeComponent::GetAttributeBaseValue(FName AttributeName, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->BaseValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::GetAttributeBaseValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = const_cast<UTcsAttributeComponent*>(this)->ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	return GetAttributeBaseValue(AttributeName, OutValue);
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeValues() const
{
	TMap<FName, float> AttributeValues;
	AttributeValues.Reserve(Attributes.Num());
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.CurrentValue);
	}

	return AttributeValues;
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeBaseValues() const
{
	TMap<FName, float> AttributeValues;
	AttributeValues.Reserve(Attributes.Num());
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.BaseValue);
	}

	return AttributeValues;
}
