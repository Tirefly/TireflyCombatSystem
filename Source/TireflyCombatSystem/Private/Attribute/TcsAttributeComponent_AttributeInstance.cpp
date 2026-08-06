// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"


namespace
{
	bool LogAttributeRuntimeNotReady_AttrInstance(const UTcsAttributeComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return false;
	}

	void LogAttributeRuntimeNotReadyVoid_AttrInstance(const UTcsAttributeComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
	}
}



bool UTcsAttributeComponent::AddAttribute(FName AttributeName)
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_AttrInstance(this, TEXT(__FUNCTION__));
	}

	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager();
	if (!DefinitionManager)
	{
		return false;
	}

	const UTcsAttributeDefinition* AttrDef = DefinitionManager->GetAttributeDefinition(AttributeName);
	if (!AttrDef)
	{
		return false;
	}

	// 防止覆盖已存在的属性
	if (Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute '%s' already exists on '%s', skipping add"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*GetPathName());
		return false;
	}

	FTcsAttributeInstance AttrInst = FTcsAttributeInstance(AttrDef, AttributeName, AllocateAttributeInstanceId(), GetOwner());
	Attributes.Add(AttributeName, AttrInst);

	// Clamp initialization values to the configured range (static or dynamic).
	if (FTcsAttributeInstance* Added = Attributes.Find(AttributeName))
	{
		float Clamped = Added->BaseValue;
		ClampAttributeValueInRange(AttributeName, Clamped);
		Added->BaseValue = Clamped;
		Added->CurrentValue = Clamped;
	}

	// 传播动态范围约束（新属性可能影响其他属性的动态范围边界）
	TSet<FName> DirtyAttributes;
	DirtyAttributes.Add(AttributeName);
	EnforceAttributeRangeConstraints(DirtyAttributes);
	return true;
}

void UTcsAttributeComponent::AddAttributes(const TArray<FName>& AttributeNames)
{
	if (!IsRuntimePrepared())
	{
		LogAttributeRuntimeNotReadyVoid_AttrInstance(this, TEXT(__FUNCTION__));
		return;
	}

	TSet<FName> DirtyAttributes;

	for (const FName AttributeName : AttributeNames)
	{
		// 防止覆盖已存在的属性
		if (Attributes.Contains(AttributeName))
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute '%s' already exists on '%s', skipping add"),
				*FString(__FUNCTION__),
				*AttributeName.ToString(),
				*GetPathName());
			continue;
		}

		UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager();
		if (!DefinitionManager)
		{
			return;
		}

		const UTcsAttributeDefinition* AttrDef = DefinitionManager->GetAttributeDefinition(AttributeName);
		if (!AttrDef)
		{
			continue;
		}

		FTcsAttributeInstance AttrInst = FTcsAttributeInstance(AttrDef, AttributeName, AllocateAttributeInstanceId(), GetOwner());
		Attributes.Add(AttributeName, AttrInst);

		// Clamp initialization values to the configured range (static or dynamic).
		if (FTcsAttributeInstance* Added = Attributes.Find(AttributeName))
		{
			float Clamped = Added->BaseValue;
			ClampAttributeValueInRange(AttributeName, Clamped);
			Added->BaseValue = Clamped;
			Added->CurrentValue = Clamped;
			DirtyAttributes.Add(AttributeName);
		}
	}

	// 批量添加完成后统一传播动态范围约束
	if (!DirtyAttributes.IsEmpty())
	{
		EnforceAttributeRangeConstraints(DirtyAttributes);
	}
}

bool UTcsAttributeComponent::AddAttributeByTag(const FGameplayTag& AttributeTag)
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_AttrInstance(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	// 检查属性是否已存在
	if (Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Attribute '%s' already exists on '%s', skipping add"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*GetPathName());
		return false;
	}

	return AddAttribute(AttributeName);
}

bool UTcsAttributeComponent::SetAttributeBaseValue(FName AttributeName, float NewValue, bool bTriggerEvents)
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_AttrInstance(this, TEXT(__FUNCTION__));
	}

	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Invalid AttributeName"), *FString(__FUNCTION__));
		return false;
	}

	FTcsAttributeInstance* Attribute = Attributes.Find(AttributeName);
	if (!Attribute)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Attribute '%s' not found on '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*GetPathName());
		return false;
	}

	float OldValue = Attribute->BaseValue;
	TMap<FName, float> CandidateBaseValues = GetAttributeBaseValues();
	CandidateBaseValues.Add(AttributeName, NewValue);
	if (!ClampCandidateAttributeValues(CandidateBaseValues))
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Failed to clamp candidate BaseValue for '%s' on '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*GetPathName());
		return false;
	}

	TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
	TMap<FName, float> CandidateCurrentValues;
	if (!BuildOngoingAttributeValues(
		CandidateBaseValues,
		AttributeModifiers,
		UpdatedModifierInstances,
		CandidateCurrentValues))
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Failed to rebuild Ongoing AttributeModifier values for '%s' on '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*GetPathName());
		return false;
	}

	CommitAttributeModifierTransaction(
		CandidateBaseValues,
		CandidateCurrentValues,
		UpdatedModifierInstances,
		!AttributeModifiers.IsEmpty(),
		bTriggerEvents);

	UE_LOG(LogTcsAttribute, Verbose,
		TEXT("[%s] Set attribute '%s' BaseValue from %.2f to %.2f on '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		OldValue,
		Attribute->BaseValue,
		*GetPathName());

	return true;
}

bool UTcsAttributeComponent::RemoveAttribute(FName AttributeName)
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_AttrInstance(this, TEXT(__FUNCTION__));
	}

	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Invalid AttributeName"), *FString(__FUNCTION__));
		return false;
	}

	if (!Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Attribute '%s' not found on '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*GetPathName());
		return false;
	}

	// 检查是否有其他属性的动态范围依赖于该属性，若有则阻止移除
	for (const auto& Pair : Attributes)
	{
		if (Pair.Key == AttributeName)
		{
			continue;
		}

		const UTcsAttributeDefinition* OtherDef = Pair.Value.AttributeDef;
		if (!OtherDef)
		{
			continue;
		}

		const FTcsAttributeRange& Range = OtherDef->AttributeRange;
		const bool bMinRefersToThis = (Range.MinValueType == ETcsAttributeRangeType::ART_Dynamic)
			&& (Range.MinValueAttribute == AttributeName);
		const bool bMaxRefersToThis = (Range.MaxValueType == ETcsAttributeRangeType::ART_Dynamic)
			&& (Range.MaxValueAttribute == AttributeName);

		if (bMinRefersToThis || bMaxRefersToThis)
		{
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Cannot remove attribute '%s' from '%s': attribute '%s' has a dynamic range dependency on it (%s). Remove or update the dependent attribute first."),
				*FString(__FUNCTION__),
				*AttributeName.ToString(),
				*GetPathName(),
				*Pair.Key.ToString(),
				bMinRefersToThis ? TEXT("MinValue") : TEXT("MaxValue"));
			return false;
		}
	}

	// 被任意 Ongoing Operation 引用的 Attribute 不能移除，避免留下无目标的持续贡献。
	for (const FTcsAttributeModifierInstance& Modifier : AttributeModifiers)
	{
		for (const FTcsEvaluatedAttributeOperation& Operation : Modifier.AppliedOperations)
		{
			if (Operation.TargetAttributeId == AttributeName)
			{
				UE_LOG(LogTcsAttribute, Error,
					TEXT("[%s] Cannot remove attribute '%s' from '%s': Ongoing ModifierInstId %d still targets it."),
					*FString(__FUNCTION__),
					*AttributeName.ToString(),
					*GetPathName(),
					Modifier.ModifierInstId);
				return false;
			}
		}
	}

	// 从组件中移除属性
	Attributes.Remove(AttributeName);

	UE_LOG(LogTcsAttribute, Log,
		TEXT("[%s] Removed attribute '%s' from '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		*GetPathName());

	return true;
}
