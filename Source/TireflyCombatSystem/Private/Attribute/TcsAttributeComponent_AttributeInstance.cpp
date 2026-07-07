// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
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



bool UTcsAttributeComponent::AddAttribute(FName AttributeName, float InitValue)
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_AttrInstance(this, TEXT(__FUNCTION__));
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	const UTcsAttributeDefinition* AttrDef = Mgr->GetAttributeDefinition(AttributeName);
	if (!AttrDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
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

	FTcsAttributeInstance AttrInst = FTcsAttributeInstance(AttrDef, AttributeName, Mgr->AllocateAttributeInstanceId(), GetOwner(), InitValue);
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

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
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

		const UTcsAttributeDefinition* AttrDef = Mgr->GetAttributeDefinition(AttributeName);
		if (!AttrDef)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefinition '%s' not found"),
				*FString(__FUNCTION__),
				*AttributeName.ToString());
			continue;
		}

		FTcsAttributeInstance AttrInst = FTcsAttributeInstance(AttrDef, AttributeName, Mgr->AllocateAttributeInstanceId(), GetOwner());
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

bool UTcsAttributeComponent::AddAttributeByTag(const FGameplayTag& AttributeTag, float InitValue)
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_AttrInstance(this, TEXT(__FUNCTION__));
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Failed to resolve AttributeTag '%s' to AttributeName"),
			*FString(__FUNCTION__),
			*AttributeTag.ToString());
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

	AddAttribute(AttributeName, InitValue);

	// 验证是否真的添加成功
	return Attributes.Contains(AttributeName);
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

	// 保存旧值
	float OldValue = Attribute->BaseValue;
	TMap<FName, float> PreviousBaseValues;
	TMap<FName, float> PreviousCurrentValues;
	if (bTriggerEvents)
	{
		PreviousBaseValues = GetAttributeBaseValues();
		PreviousCurrentValues = GetAttributeValues();
	}

	// 设置新值并 Clamp
	Attribute->BaseValue = NewValue;
	ClampAttributeValueInRange(AttributeName, Attribute->BaseValue);

	// 重新计算 Current 值（应用修改器）
	RecalculateAttributeCurrentValues(-1, false);

	// 触发事件
	if (bTriggerEvents && !FMath::IsNearlyEqual(OldValue, Attribute->BaseValue))
	{
		BroadcastAttributeStateDiffs(PreviousBaseValues, PreviousCurrentValues);
	}

	UE_LOG(LogTcsAttribute, Verbose,
		TEXT("[%s] Set attribute '%s' BaseValue from %.2f to %.2f on '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		OldValue,
		Attribute->BaseValue,
		*GetPathName());

	return true;
}

bool UTcsAttributeComponent::SetAttributeCurrentValue(FName AttributeName, float NewValue, bool bTriggerEvents)
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

	// 保存旧值
	float OldValue = Attribute->CurrentValue;
	TMap<FName, float> PreviousBaseValues;
	TMap<FName, float> PreviousCurrentValues;
	if (bTriggerEvents)
	{
		PreviousBaseValues = GetAttributeBaseValues();
		PreviousCurrentValues = GetAttributeValues();
	}

	// 设置新值并 Clamp
	Attribute->CurrentValue = NewValue;
	ClampAttributeValueInRange(AttributeName, Attribute->CurrentValue);

	// 传播动态范围约束（该属性值变化可能影响其他属性的动态范围边界）
	TSet<FName> DirtyAttributes;
	DirtyAttributes.Add(AttributeName);
	EnforceAttributeRangeConstraints(DirtyAttributes, false);

	// 触发事件
	if (bTriggerEvents && !FMath::IsNearlyEqual(OldValue, Attribute->CurrentValue))
	{
		BroadcastAttributeStateDiffs(PreviousBaseValues, PreviousCurrentValues);
	}

	UE_LOG(LogTcsAttribute, Verbose,
		TEXT("[%s] Set attribute '%s' CurrentValue from %.2f to %.2f on '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		OldValue,
		Attribute->CurrentValue,
		*GetPathName());

	return true;
}

bool UTcsAttributeComponent::ResetAttribute(FName AttributeName)
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

	// 获取初始值（恢复到 AddAttribute 时传入的初始值）
	float InitValue = Attribute->InitialValue;

	// 移除所有应用到该属性的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (const FTcsAttributeModifierInstance& Modifier : AttributeModifiers)
	{
		if (!Modifier.ModifierDef)
		{
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;
		if (ModDef && ModDef->AttributeId == AttributeName)
		{
			ModifiersToRemove.Add(Modifier);
		}
	}
	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(ModifiersToRemove);
	}

	TMap<FName, float> PreviousBaseValues = GetAttributeBaseValues();
	TMap<FName, float> PreviousCurrentValues = GetAttributeValues();

	// 重置 Base 和 Current 值
	float OldBase = Attribute->BaseValue;
	float OldCurrent = Attribute->CurrentValue;
	Attribute->BaseValue = InitValue;
	Attribute->CurrentValue = InitValue;

	// Clamp 值
	ClampAttributeValueInRange(AttributeName, Attribute->BaseValue);
	ClampAttributeValueInRange(AttributeName, Attribute->CurrentValue);
	EnforceAttributeRangeConstraints(false);

	// 触发事件
	if (!FMath::IsNearlyEqual(OldBase, Attribute->BaseValue)
		|| !FMath::IsNearlyEqual(OldCurrent, Attribute->CurrentValue))
	{
		BroadcastAttributeStateDiffs(PreviousBaseValues, PreviousCurrentValues);
	}

	UE_LOG(LogTcsAttribute, Log,
		TEXT("[%s] Reset attribute '%s' to initial value %.2f on '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		InitValue,
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

	// 移除所有应用到该属性的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (const FTcsAttributeModifierInstance& Modifier : AttributeModifiers)
	{
		if (!Modifier.ModifierDef)
		{
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;
		if (ModDef && ModDef->AttributeId == AttributeName)
		{
			ModifiersToRemove.Add(Modifier);
		}
	}
	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(ModifiersToRemove);
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
