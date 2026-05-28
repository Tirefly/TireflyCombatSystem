// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeComponent.h"

#include "TcsEntityInterface.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"


namespace
{
	void BuildModifierEventPayloads(
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierEventPayload>& OutPayloads)
	{
		OutPayloads.Reset();
		OutPayloads.Reserve(Modifiers.Num());

		for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
		{
			OutPayloads.Emplace(Modifier);
		}
	}
}



UTcsAttributeComponent::UTcsAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTcsAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
		}
	}

#if !UE_BUILD_SHIPPING
	// 预热自测断言：GameInstanceSubsystem 在 BeginPlay 之前必然完成 Initialize，
	// 若此处仍为空表明 Subsystem 生命周期被破坏，立即暴露。
	checkf(AttrMgr, TEXT("AttrMgr resolve failed in BeginPlay for %s; GameInstanceSubsystem lifecycle broken."), *GetPathName());
#endif
}

UTcsAttributeManagerSubsystem* UTcsAttributeComponent::ResolveAttributeManager()
{
	if (!AttrMgr)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
			}
		}
		ensureMsgf(AttrMgr, TEXT("[%s] Failed to resolve AttributeManagerSubsystem for %s"),
			*FString(__FUNCTION__), *GetPathName());
	}
	return AttrMgr;
}

bool UTcsAttributeComponent::GetAttributeValue(FName AttributeName, float& OutValue) const
{
	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->CurrentValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::HasAttributeByTag(const FGameplayTag& AttributeTag) const
{
	UTcsAttributeManagerSubsystem* Mgr = const_cast<UTcsAttributeComponent*>(this)->ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
	{
		return false;
	}

	return Attributes.Contains(AttributeName);
}

bool UTcsAttributeComponent::GetAttributeValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	UTcsAttributeManagerSubsystem* Mgr = const_cast<UTcsAttributeComponent*>(this)->ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
	{
		return false;
	}

	return GetAttributeValue(AttributeName, OutValue);
}

bool UTcsAttributeComponent::GetAttributeBaseValue(FName AttributeName, float& OutValue) const
{
	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->BaseValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::GetAttributeBaseValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	UTcsAttributeManagerSubsystem* Mgr = const_cast<UTcsAttributeComponent*>(this)->ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
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

void UTcsAttributeComponent::BroadcastAttributeValueChangeEvent(
	const TArray<FTcsAttributeChangeEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnAttributeValueChanged.IsBound())
	{
		OnAttributeValueChanged.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeBaseValueChangeEvent(
	const TArray<FTcsAttributeChangeEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnAttributeBaseValueChanged.IsBound())
	{
		OnAttributeBaseValueChanged.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierAddedBatchEvent(
	const TArray<FTcsAttributeModifierEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributeModifiersAdded.IsBound())
	{
		OnAttributeModifiersAdded.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierRemovedBatchEvent(
	const TArray<FTcsAttributeModifierEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributeModifiersRemoved.IsBound())
	{
		OnAttributeModifiersRemoved.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierUpdatedBatchEvent(
	const TArray<FTcsAttributeModifierEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributeModifiersUpdated.IsBound())
	{
		OnAttributeModifiersUpdated.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeReachedBoundaryBatchEvent(
	const TArray<FTcsAttributeBoundaryEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributesReachedBoundary.IsBound())
	{
		OnAttributesReachedBoundary.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeStateDiffs(
	const TMap<FName, float>& PreviousBaseValues,
	const TMap<FName, float>& PreviousCurrentValues)
{
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BaseChangePayloads.Reserve(Attributes.Num());
	CurrentChangePayloads.Reserve(Attributes.Num());
	BoundaryPayloads.Reserve(Attributes.Num());

	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		const FName AttributeName = Pair.Key;
		const FTcsAttributeInstance& Attribute = Pair.Value;

		const float PreviousBase = PreviousBaseValues.FindRef(AttributeName);
		if (!FMath::IsNearlyEqual(PreviousBase, Attribute.BaseValue))
		{
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = PreviousBase;
			Payload.NewValue = Attribute.BaseValue;
			BaseChangePayloads.Add(Payload);

			float BoundaryCandidate = Attribute.BaseValue;
			float RangeMin = BoundaryCandidate;
			float RangeMax = BoundaryCandidate;
			ClampAttributeValueInRange(AttributeName, BoundaryCandidate, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(Attribute.BaseValue, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Attribute.BaseValue, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				BoundaryPayloads.Emplace(
					AttributeName,
					bReachedMax,
					PreviousBase,
					Attribute.BaseValue,
					bReachedMax ? RangeMax : RangeMin);
			}
		}

		const float PreviousCurrent = PreviousCurrentValues.FindRef(AttributeName);
		if (!FMath::IsNearlyEqual(PreviousCurrent, Attribute.CurrentValue))
		{
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = PreviousCurrent;
			Payload.NewValue = Attribute.CurrentValue;
			CurrentChangePayloads.Add(Payload);

			float BoundaryCandidate = Attribute.CurrentValue;
			float RangeMin = BoundaryCandidate;
			float RangeMax = BoundaryCandidate;
			ClampAttributeValueInRange(AttributeName, BoundaryCandidate, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				BoundaryPayloads.Emplace(
					AttributeName,
					bReachedMax,
					PreviousCurrent,
					Attribute.CurrentValue,
					bReachedMax ? RangeMax : RangeMin);
			}
		}
	}

	if (!BaseChangePayloads.IsEmpty())
	{
		BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}

	if (!CurrentChangePayloads.IsEmpty())
	{
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}

	BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
}


// ============================================================
// #pragma region AttributeInstance
// ============================================================

bool UTcsAttributeComponent::AddAttribute(FName AttributeName, float InitValue)
{
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
		if (ModDef && ModDef->AttributeName == AttributeName)
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
		if (ModDef && ModDef->AttributeName == AttributeName)
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


// ============================================================
// #pragma region AttributeModifier
// ============================================================

bool UTcsAttributeComponent::CreateAttributeModifier(
	FName ModifierId,
	AActor* Instigator,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"), *FString(__FUNCTION__));
		return false;
	}

	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	const UTcsAttributeModifierDefinition* ModifierDef = Mgr->GetModifierDefinition(ModifierId);
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	OutModifierInst = FTcsAttributeModifierInstance();

	// 设置 DataAsset 引用和 ModifierId
	OutModifierInst.ModifierDef = ModifierDef;
	OutModifierInst.ModifierId = ModifierId;

	// 验证优先级
	if (ModifierDef->Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, will use raw priority 0."),
			*FString(__FUNCTION__),
			*ModifierDef->ModifierName.ToString(),
			ModifierDef->Priority);
		return false;
	}

	OutModifierInst.ModifierInstId = Mgr->AllocateModifierInstanceId();
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = GetOwner();
	OutModifierInst.Operands = ModifierDef->Operands;

	return true;
}

bool UTcsAttributeComponent::CreateAttributeModifierWithOperands(
	FName ModifierId,
	AActor* Instigator,
	const TMap<FName, float>& Operands,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"), *FString(__FUNCTION__));
		return false;
	}

	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	const UTcsAttributeModifierDefinition* ModifierDef = Mgr->GetModifierDefinition(ModifierId);
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	// 验证 Operands 是否正确
	if (ModifierDef->Operands.IsEmpty())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] ModifierDef does not contain Operands"), *FString(__FUNCTION__));
		return false;
	}
	for (const TPair<FName, float>& Operand : ModifierDef->Operands)
	{
		if (!Operands.Contains(Operand.Key))
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Operand %s is not found in Operands"),
				*FString(__FUNCTION__),
				*Operand.Key.ToString());
			return false;
		}
	}

	OutModifierInst = FTcsAttributeModifierInstance();

	// 设置 DataAsset 引用和 ModifierId
	OutModifierInst.ModifierDef = ModifierDef;
	OutModifierInst.ModifierId = ModifierId;

	// 验证优先级
	if (ModifierDef->Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, will use raw priority 0."),
			*FString(__FUNCTION__),
			*ModifierDef->ModifierName.ToString(),
			ModifierDef->Priority);
	}

	OutModifierInst.ModifierInstId = Mgr->AllocateModifierInstanceId();
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = GetOwner();
	OutModifierInst.Operands = Operands;

	return true;
}

void UTcsAttributeComponent::ApplyModifier(TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_ApplyModifier);

	if (Modifiers.IsEmpty())
	{
		return;
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

	TArray<FTcsAttributeModifierInstance> ModifiersToExecute; // 对属性 Base 值执行操作的属性修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToApply;   // 对属性 Current 值应用的属性修改器
	ModifiersToExecute.Reserve(Modifiers.Num());
	ModifiersToApply.Reserve(Modifiers.Num());
	const int64 BatchId = Mgr->AllocateModifierChangeBatchId();
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();

	// 区分修改属性 Base 值和 Current 值的两种修改器
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		if (!Modifier.ModifierDef)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] ModifierDef is null for ModifierId: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString());
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;

		switch (ModDef->ModifierMode)
		{
		case ETcsAttributeModifierMode::AMM_BaseValue:
			{
				// BaseValue modifiers are executed immediately (not persisted), but we still stamp them for
				// deterministic merging policies (e.g., UseNewest/UseOldest) and debugging.
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToExecute.Add(Modifier);
				break;
			}
		case ETcsAttributeModifierMode::AMM_CurrentValue:
			{
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToApply.Add(Modifier);
				break;
			}
		}
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Input=%d BaseOps=%d CurrentOps=%d StoredCurrentModifiers=%d"),
		*FString(__FUNCTION__),
		Modifiers.Num(),
		ModifiersToExecute.Num(),
		ModifiersToApply.Num(),
		AttributeModifiers.Num());

	// 先执行针对属性 Base 值的修改器
	if (!ModifiersToExecute.IsEmpty())
	{
		RecalculateAttributeBaseValues(ModifiersToExecute);
	}

	// 再执行针对属性 Current 值的修改器
	if (!ModifiersToApply.IsEmpty())
	{
		TArray<FTcsAttributeModifierInstance> NewlyAddedModifiers;
		TArray<FTcsAttributeModifierInstance> UpdatedExistingModifiers;
		TArray<FTcsAttributeModifierEventPayload> ModifierEventPayloads;
		NewlyAddedModifiers.Reserve(ModifiersToApply.Num());
		UpdatedExistingModifiers.Reserve(ModifiersToApply.Num());

		// 把已经用过但有改变的属性修改器更新一下，并从待应用列表中移除
		{
			TArray<FTcsAttributeModifierInstance> IncomingToAdd;
			IncomingToAdd.Reserve(ModifiersToApply.Num());

			for (const FTcsAttributeModifierInstance& Incoming : ModifiersToApply)
			{
				bool bUpdated = false;

				if (Incoming.SourceHandle.IsValid())
				{
					const int32 SourceId = Incoming.SourceHandle.Id;

					// 使用稳定 ID 缓存查找现有修改器
					if (const TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceId))
					{
						for (int32 ModifierInstId : *InstIdsPtr)
						{
							const int32* IndexPtr = ModifierInstIdToIndex.Find(ModifierInstId);
							if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
							{
								continue;
							}

							int32 Index = *IndexPtr;
							FTcsAttributeModifierInstance& Stored = AttributeModifiers[Index];

							if (Stored.ModifierInstId != ModifierInstId)
							{
								continue;
							}

							if (Stored.ModifierId != Incoming.ModifierId)
							{
								continue;
							}

							// Keep ModifierInstId and ApplyTimestamp stable; treat this as a refresh/update.
							Stored.Operands = Incoming.Operands;
							Stored.Instigator = Incoming.Instigator;
							Stored.Target = Incoming.Target;
							Stored.SourceHandle = Incoming.SourceHandle;
							Stored.UpdateTimestamp = UtcNowTicks;
							Stored.LastTouchedBatchId = BatchId;

							UpdatedExistingModifiers.Add(Stored);
							bUpdated = true;
							break;
						}
					}
				}

				if (!bUpdated)
				{
					IncomingToAdd.Add(Incoming);
				}
			}

			ModifiersToApply = IncomingToAdd;
		}

		// 剩余的 ModifiersToApply 即为新增的修改器
		NewlyAddedModifiers = ModifiersToApply;

		// 添加新修改器并更新索引
		for (const FTcsAttributeModifierInstance& Modifier : ModifiersToApply)
		{
			FTcsAttributeModifierInstance ModifierToStore = Modifier;
			ModifierToStore.LastTouchedBatchId = BatchId;
			int32 NewIndex = AttributeModifiers.Add(ModifierToStore);

			// 更新两个缓存: ModifierInstId -> Index 和 SourceId -> ModifierInstIds
			ModifierInstIdToIndex.Add(ModifierToStore.ModifierInstId, NewIndex);

			if (ModifierToStore.SourceHandle.IsValid())
			{
				TSet<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(ModifierToStore.SourceHandle.Id);
				InstIds.Add(ModifierToStore.ModifierInstId);
			}
		}

		// 对外统一走批量事件面；旧的逐条事件仅作为兼容出口，由批量广播函数按需回放。
		BuildModifierEventPayloads(UpdatedExistingModifiers, ModifierEventPayloads);
		BroadcastAttributeModifierUpdatedBatchEvent(ModifierEventPayloads);

		BuildModifierEventPayloads(NewlyAddedModifiers, ModifierEventPayloads);
		BroadcastAttributeModifierAddedBatchEvent(ModifierEventPayloads);
	}

	// 无论如何，都要重新计算属性 Current 值
	RecalculateAttributeCurrentValues(BatchId);
}

bool UTcsAttributeComponent::ApplyModifierWithSourceHandle(
	const FTcsSourceHandle& SourceHandle,
	const TArray<FName>& ModifierIds,
	TArray<FTcsAttributeModifierInstance>& OutModifiers)
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	OutModifiers.Empty();

	// 为每个 ModifierId 创建修改器实例
	for (const FName& ModifierId : ModifierIds)
	{
		FTcsAttributeModifierInstance ModifierInst;
		if (CreateAttributeModifier(ModifierId, SourceHandle.Instigator.Get(), ModifierInst))
		{
			// 设置 SourceHandle
			ModifierInst.SourceHandle = SourceHandle;
			OutModifiers.Add(ModifierInst);
		}
	}

	if (OutModifiers.Num() > 0)
	{
		ApplyModifier(OutModifiers);
		return true;
	}

	return false;
}

void UTcsAttributeComponent::RemoveModifier(TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

	const int64 BatchId = Mgr->AllocateModifierChangeBatchId();
	bool bModified = false;
	TArray<FTcsAttributeModifierInstance> RemovedModifiers;
	RemovedModifiers.Reserve(Modifiers.Num());
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		// 使用 ModifierInstId 定位元素
		const int32* IndexPtr = ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			continue;
		}

		int32 RemovedIndex = *IndexPtr;
		const FTcsAttributeModifierInstance& RemovedModifierRef = AttributeModifiers[RemovedIndex];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (RemovedModifierRef.ModifierInstId != Modifier.ModifierInstId)
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] ModifierInstId mismatch at index %d: expected %d, found %d"),
				*FString(__FUNCTION__),
				RemovedIndex,
				Modifier.ModifierInstId,
				RemovedModifierRef.ModifierInstId);
			continue;
		}

		// 在 RemoveAtSwap 之前拷贝数据，避免引用失效
		const FTcsAttributeModifierInstance RemovedModifier = RemovedModifierRef;

		// 从两个缓存中移除
		ModifierInstIdToIndex.Remove(RemovedModifier.ModifierInstId);

		if (RemovedModifier.SourceHandle.IsValid())
		{
			TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(RemovedModifier.SourceHandle.Id);
			if (InstIdsPtr)
			{
				InstIdsPtr->Remove(RemovedModifier.ModifierInstId);
				if (InstIdsPtr->Num() == 0)
				{
					SourceHandleIdToModifierInstIds.Remove(RemovedModifier.SourceHandle.Id);
				}
			}
		}

		// 使用 RemoveAtSwap 删除元素（O(1) 操作）
		const int32 LastIndex = AttributeModifiers.Num() - 1;
		if (RemovedIndex != LastIndex)
		{
			// 有元素被 swap 过来，更新其索引
			const FTcsAttributeModifierInstance& SwappedModifier = AttributeModifiers[LastIndex];
			ModifierInstIdToIndex[SwappedModifier.ModifierInstId] = RemovedIndex;
		}

		AttributeModifiers.RemoveAtSwap(RemovedIndex);

		RemovedModifiers.Add(RemovedModifier);
		bModified = true;
	}

	if (!RemovedModifiers.IsEmpty())
	{
		TArray<FTcsAttributeModifierEventPayload> RemovedPayloads;
		BuildModifierEventPayloads(RemovedModifiers, RemovedPayloads);
		BroadcastAttributeModifierRemovedBatchEvent(RemovedPayloads);
	}

	// 如果确实有属性修改器被移除，则更新属性的当前值
	if (bModified)
	{
		RecalculateAttributeCurrentValues(BatchId);
	}
}

bool UTcsAttributeComponent::RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RemoveModifiersBySourceHandle);

	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	// 使用稳定 ID 缓存查找匹配的修改器
	const TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] SourceId=%d BucketSize=%d StoredCurrentModifiers=%d"),
		*FString(__FUNCTION__),
		SourceHandle.Id,
		InstIdsPtr->Num(),
		AttributeModifiers.Num());

	return RemoveStoredModifiersByInstIds(*InstIdsPtr, Mgr->AllocateModifierChangeBatchId());
}

bool UTcsAttributeComponent::GetModifiersBySourceHandle(
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsAttributeModifierInstance>& OutModifiers) const
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	const TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	OutModifiers.Empty();
	OutModifiers.Reserve(InstIdsPtr->Num());
	for (int32 ModifierInstId : *InstIdsPtr)
	{
		const int32* IndexPtr = ModifierInstIdToIndex.Find(ModifierInstId);
		if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			continue;
		}

		int32 Index = *IndexPtr;
		const FTcsAttributeModifierInstance& Modifier = AttributeModifiers[Index];

		if (Modifier.ModifierInstId == ModifierInstId)
		{
			OutModifiers.Add(Modifier);
		}
	}

	return OutModifiers.Num() > 0;
}

void UTcsAttributeComponent::HandleModifierUpdated(TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

	bool bModified = false;
	const int64 BatchId = Mgr->AllocateModifierChangeBatchId();
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();
	TArray<FTcsAttributeModifierInstance> UpdatedModifiers;
	UpdatedModifiers.Reserve(Modifiers.Num());
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		const int32* IndexPtr = ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			continue;
		}

		const int32 ModifierIndex = *IndexPtr;
		const FTcsAttributeModifierInstance OldStored = AttributeModifiers[ModifierIndex];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (OldStored.ModifierInstId != Modifier.ModifierInstId)
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] ModifierInstId mismatch at index %d: expected %d, found %d"),
				*FString(__FUNCTION__),
				ModifierIndex,
				Modifier.ModifierInstId,
				OldStored.ModifierInstId);
			continue;
		}

		Modifier.UpdateTimestamp = UtcNowTicks;
		Modifier.LastTouchedBatchId = BatchId;

		AttributeModifiers[ModifierIndex] = Modifier;

		// 更新 SourceHandle 缓存（如果 SourceHandle 发生变化）
		const int32 OldSourceId = OldStored.SourceHandle.IsValid() ? OldStored.SourceHandle.Id : -1;
		const int32 NewSourceId = Modifier.SourceHandle.IsValid() ? Modifier.SourceHandle.Id : -1;
		if (OldSourceId != NewSourceId)
		{
			if (OldSourceId >= 0)
			{
				if (TSet<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(OldSourceId))
				{
					InstIdsPtr->Remove(Modifier.ModifierInstId);
					if (InstIdsPtr->IsEmpty())
					{
						SourceHandleIdToModifierInstIds.Remove(OldSourceId);
					}
				}
			}

			if (NewSourceId >= 0)
			{
				TSet<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
				InstIds.Add(Modifier.ModifierInstId);
			}
		}
		else if (NewSourceId >= 0)
		{
			TSet<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
			InstIds.Add(Modifier.ModifierInstId);
		}

		UpdatedModifiers.Add(Modifier);

		bModified = true;
	}

	if (!UpdatedModifiers.IsEmpty())
	{
		TArray<FTcsAttributeModifierEventPayload> UpdatedPayloads;
		BuildModifierEventPayloads(UpdatedModifiers, UpdatedPayloads);
		BroadcastAttributeModifierUpdatedBatchEvent(UpdatedPayloads);
	}

	if (bModified)
	{
		RecalculateAttributeCurrentValues(BatchId);
	}
}

bool UTcsAttributeComponent::RemoveStoredModifiersByInstIds(
	const TSet<int32>& ModifierInstIdsToRemove,
	int64 ChangeBatchId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RemoveStoredModifiersByInstIds);

	if (ModifierInstIdsToRemove.IsEmpty() || AttributeModifiers.IsEmpty())
	{
		return false;
	}

	TArray<FTcsAttributeModifierInstance> RemovedModifiers;
	RemovedModifiers.Reserve(FMath::Min(ModifierInstIdsToRemove.Num(), AttributeModifiers.Num()));

	TArray<FTcsAttributeModifierInstance> RemainingModifiers;
	RemainingModifiers.Reserve(AttributeModifiers.Num());

	// 先把旧数组拆成“移除集”和“保留集”。
	// 这样可以避免边遍历边删除导致下标失效，也避免在循环里反复维护缓存索引。
	for (const FTcsAttributeModifierInstance& StoredModifier : AttributeModifiers)
	{
		if (ModifierInstIdsToRemove.Contains(StoredModifier.ModifierInstId))
		{
			RemovedModifiers.Add(StoredModifier);
			continue;
		}

		RemainingModifiers.Add(StoredModifier);
	}

	if (RemovedModifiers.IsEmpty())
	{
		return false;
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] RemoveIds=%d Removed=%d Remaining=%d BatchId=%lld"),
		*FString(__FUNCTION__),
		ModifierInstIdsToRemove.Num(),
		RemovedModifiers.Num(),
		RemainingModifiers.Num(),
		ChangeBatchId);

	AttributeModifiers = MoveTemp(RemainingModifiers);
	// ModifierInstIdToIndex 和 SourceHandleIdToModifierInstIds 都依赖 AttributeModifiers 的当前排布。
	// 批量删除完成后统一重建，能保证索引绝对一致，也比逐个挪动缓存更简单可靠。
	RebuildModifierRuntimeCaches();

	// 先落存储和缓存，再发移除事件，避免监听方在回调里读到已经过期的下标缓存。
	TArray<FTcsAttributeModifierEventPayload> RemovedPayloads;
	BuildModifierEventPayloads(RemovedModifiers, RemovedPayloads);
	BroadcastAttributeModifierRemovedBatchEvent(RemovedPayloads);

	// 删除持久化修改器后，CurrentValue 必须按“剩余修改器集合”重新求值。
	// 这里沿用传入的 ChangeBatchId，方便后续增量传播继续识别这一轮变更。
	RecalculateAttributeCurrentValues(ChangeBatchId);
	return true;
}

void UTcsAttributeComponent::RebuildModifierRuntimeCaches()
{
	// 这两个缓存都只是 AttributeModifiers 的运行时索引视图，
	// 不应保留旧状态并尝试增量修补；全量重建更容易保证一致性。
	ModifierInstIdToIndex.Reset();
	SourceHandleIdToModifierInstIds.Reset();

	ModifierInstIdToIndex.Reserve(AttributeModifiers.Num());
	SourceHandleIdToModifierInstIds.Reserve(AttributeModifiers.Num());

	for (int32 Index = 0; Index < AttributeModifiers.Num(); ++Index)
	{
		const FTcsAttributeModifierInstance& Modifier = AttributeModifiers[Index];
		// ModifierInstId -> Index 主要服务于单实例快速更新和删除定位。
		ModifierInstIdToIndex.Add(Modifier.ModifierInstId, Index);

		if (Modifier.SourceHandle.IsValid())
		{
			// SourceHandle -> ModifierInstId 集合主要服务于按 SourceHandle 批量删除。
			SourceHandleIdToModifierInstIds.FindOrAdd(Modifier.SourceHandle.Id).Add(Modifier.ModifierInstId);
		}
	}
}


// ============================================================
// #pragma region AttributeCalculation
// ============================================================

void UTcsAttributeComponent::RecalculateAttributeBaseValues(
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RecalculateAttributeBaseValues);

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(Modifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d InputModifiers=%d MergedModifiers=%d"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		Modifiers.Num(),
		MergedModifiers.Num());

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;
	ChangeEventPayloads.Reserve(Attributes.Num());

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	// 这些临时容器在整个循环里复用，避免每个 Modifier 都重新分配。
	TArray<FName> TouchedAttributes;
	TouchedAttributes.Reserve(4);
	TArray<float> TouchedOldValues;
	TouchedOldValues.Reserve(4);
	TMap<FName, float> FallbackLastModifiedResults;
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has null ModifierDef. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;
		if (!ModDef->ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}

		// 执行修改器
		auto Execution = ModDef->ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		TouchedAttributes.Reset();
		TouchedOldValues.Reset();
		// touched-report 只收窄“记录差异”的采样范围，不改变 Execute 的真实写入行为。
		const bool bHasTouchedReport = Execution->CollectTouchedAttributes(Modifier, TouchedAttributes);
		if (bHasTouchedReport)
		{
			// 先过滤掉无效名字，再只为声明 touched 的属性截取旧值。
			// 这样后面比较差异时就不需要复制整张 BaseValues。
			TouchedAttributes.RemoveAll([](const FName AttributeName) { return AttributeName.IsNone(); });
			TouchedOldValues.Reserve(TouchedAttributes.Num());
			for (const FName AttributeName : TouchedAttributes)
			{
				TouchedOldValues.Add(BaseValues.FindRef(AttributeName));
			}
		}
		else
		{
			// 未声明 touched 集时，保留原来的全表差异检测路径，确保行为正确。
			FallbackLastModifiedResults = BaseValues;
		}

		// BaseValue 重算阶段把同一张 BaseValues 同时作为 Base/Current 视图传入 Execute。
		// 这样复用现有执行器接口，但不会引入独立的 CurrentValue 计算状态。
		Execution->Execute(Modifier, BaseValues, BaseValues);

		// Execute 之后只做差异归因：把本次执行真正改动的值累计到 SourceHandle 对应的记录里。
		if (bHasTouchedReport)
		{
			for (int32 Index = 0; Index < TouchedAttributes.Num(); ++Index)
			{
				const FName AttributeName = TouchedAttributes[Index];
				const float OldValue = TouchedOldValues.IsValidIndex(Index) ? TouchedOldValues[Index] : BaseValues.FindRef(AttributeName);
				const float NewValue = BaseValues.FindRef(AttributeName);
				if (!FMath::IsNearlyEqual(NewValue, OldValue))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(AttributeName);
					Payload.AttributeName = AttributeName;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - OldValue;
				}
			}
		}
		else
		{
			for (const TPair<FName, float>& LastPair : FallbackLastModifiedResults)
			{
				const float& NewValue = BaseValues.FindRef(LastPair.Key);
				if (!FMath::IsNearlyEqual(NewValue, LastPair.Value))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
					Payload.AttributeName = LastPair.Key;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - LastPair.Value;
				}
			}
		}
	}

	// 这一轮先处理“属性自身定义的直接 Clamp”，并同步收集真实发生变化的属性。
	// 多跳依赖传播（例如 MaxHP 变化后 HP 再次受限）留给后面的 Enforce 阶段统一处理。
	TSet<FName> ChangedAttributes;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BoundaryPayloads.Reserve(BaseValues.Num());
	for (TPair<FName, float>& Pair : BaseValues)
	{
		if (FTcsAttributeInstance* Attribute = Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->BaseValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 前面如果已经按 SourceHandle 记录过贡献，这里只负责把最终 old/new 补齐到事件载荷里。
			if (FTcsAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
			{
				Payload->NewValue = Pair.Value;
				Payload->OldValue = Attribute->BaseValue;
			}

			// 把属性基础值的最终修改赋值
			const float OldValue = Attribute->BaseValue;
			Attribute->BaseValue = Pair.Value;
			ChangedAttributes.Add(Pair.Key);

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// BaseValue 广播发生在直接写回之后、范围传播之前。
	// 这里表达的是“基础值第一阶段结算结果”，而不是多跳 Clamp 完成后的全局稳定态。
	if (bBroadcastEvents && !ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}

	// 只有真实改过的属性才作为传播种子，避免无意义地从整张属性表重新出发。
	if (!ChangedAttributes.IsEmpty())
	{
		EnforceAttributeRangeConstraints(ChangedAttributes, bBroadcastEvents);
	}
}

void UTcsAttributeComponent::RecalculateAttributeCurrentValues(int64 ChangeBatchId, bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_RecalculateAttributeCurrentValues);

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(AttributeModifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d StoredModifiers=%d MergedModifiers=%d BatchId=%lld"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		AttributeModifiers.Num(),
		MergedModifiers.Num(),
		ChangeBatchId);

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;
	ChangeEventPayloads.Reserve(Attributes.Num());

	// 获取属性基础值，用于计算
	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	// CurrentValue 的执行既可能读取 BaseValue，也可能在 CurrentValue 结果上继续叠加，
	// 因此这里显式保留 BaseValues 和 CurrentValuesToCalc 两张表。
	TMap<FName, float> CurrentValuesToCalc = BaseValues;
	// 这些容器专门服务于 touched-report 的局部差异记录。
	TArray<FName> TouchedAttributes;
	TouchedAttributes.Reserve(4);
	TArray<float> TouchedOldValues;
	TouchedOldValues.Reserve(4);
	TMap<FName, float> FallbackLastModifiedResults;

	// 执行属性修改器的修改计算
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has null ModifierDef. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Modifier.ModifierDef;
		if (!ModDef->ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
			continue;
		}

		// 执行修改器
		auto Execution = ModDef->ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		TouchedAttributes.Reset();
		TouchedOldValues.Reset();
		// CurrentValue 重算沿用同一套 touched-report 机制，未知执行器继续走全表 fallback。
		const bool bHasTouchedReport = Execution->CollectTouchedAttributes(Modifier, TouchedAttributes);
		if (bHasTouchedReport)
		{
			// 这里记录的是 CurrentValuesToCalc 的旧值，因为真正要统计的是当前值阶段的增量贡献。
			TouchedAttributes.RemoveAll([](const FName AttributeName) { return AttributeName.IsNone(); });
			TouchedOldValues.Reserve(TouchedAttributes.Num());
			for (const FName AttributeName : TouchedAttributes)
			{
				TouchedOldValues.Add(CurrentValuesToCalc.FindRef(AttributeName));
			}
		}
		else
		{
			// 未声明 touched 集时，保留原来的全表差异检测路径，确保行为正确。
			FallbackLastModifiedResults = CurrentValuesToCalc;
		}

		// BaseValues 作为只读输入参与计算，CurrentValuesToCalc 承接每个执行器的叠加结果。
		Execution->Execute(Modifier, BaseValues, CurrentValuesToCalc);

		// 只有命中本轮 BatchId 的修改器，才会把差异记进本次事件归因。
		// 这样可以避免旧修改器在全量重算时被误报成“本轮新增变化来源”。
		if (bHasTouchedReport)
		{
			for (int32 Index = 0; Index < TouchedAttributes.Num(); ++Index)
			{
				const FName AttributeName = TouchedAttributes[Index];
				const float OldValue = TouchedOldValues.IsValidIndex(Index) ? TouchedOldValues[Index] : CurrentValuesToCalc.FindRef(AttributeName);
				const float NewValue = CurrentValuesToCalc.FindRef(AttributeName);
				if (!FMath::IsNearlyEqual(NewValue, OldValue) && (ChangeBatchId < 0 || Modifier.LastTouchedBatchId == ChangeBatchId))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(AttributeName);
					Payload.AttributeName = AttributeName;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - OldValue;
				}
			}
		}
		else
		{
			for (const TPair<FName, float>& LastPair : FallbackLastModifiedResults)
			{
				const float& NewValue = CurrentValuesToCalc.FindRef(LastPair.Key);
				if (!FMath::IsNearlyEqual(NewValue, LastPair.Value) && (ChangeBatchId < 0 || Modifier.LastTouchedBatchId == ChangeBatchId))
				{
					FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
					Payload.AttributeName = LastPair.Key;
					float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceHandle);
					PayloadValue += NewValue - LastPair.Value;
				}
			}
		}
	}

	// 这里先完成“每个属性自身”的当前值 Clamp 和写回。
	// 由于动态范围可能存在多跳依赖，真正的全局稳定化仍然交给后面的传播阶段。
	TSet<FName> ChangedAttributes;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BoundaryPayloads.Reserve(CurrentValuesToCalc.Num());
	for (TPair<FName, float>& Pair : CurrentValuesToCalc)
	{
		if (FTcsAttributeInstance* Attribute = Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->CurrentValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 修改器移除、合并等情况可能不会留下逐条 SourceHandle 差异记录，
			// 因此这里统一用最终 old/new 补齐当前值变化事件，避免漏报。
			FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(Pair.Key);
			Payload.AttributeName = Pair.Key;
			Payload.NewValue = Pair.Value;
			Payload.OldValue = Attribute->CurrentValue;

			// 把属性当前值的最终修改赋值
			const float OldValue = Attribute->CurrentValue;
			Attribute->CurrentValue = Pair.Value;
			ChangedAttributes.Add(Pair.Key);

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// 当前值广播反映的是本轮执行器求值后的直接结果；
	// 如果后续范围传播又把值压回去了，会再由传播阶段补发对应事件。
	if (bBroadcastEvents && !ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		BroadcastAttributeValueChangeEvent(Payloads);
	}

	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}

	// 增量批次更新时，只有真实变更过的属性才作为传播种子；全量重算仍走保守路径。
	if (ChangeBatchId >= 0 && !ChangedAttributes.IsEmpty())
	{
		EnforceAttributeRangeConstraints(ChangedAttributes, bBroadcastEvents);
	}
	else
	{
		EnforceAttributeRangeConstraints(bBroadcastEvents);
	}
}

void UTcsAttributeComponent::MergeAttributeModifiers(
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	MergedModifiers.Reset();
	MergedModifiers.Reserve(Modifiers.Num());

	// 按 ModifierId 整理所有属性修改器，方便后续执行修改器合并
	TMap<FName, TArray<FTcsAttributeModifierInstance>> ModifiersToMerge;
	ModifiersToMerge.Reserve(Modifiers.Num());
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		ModifiersToMerge.FindOrAdd(Modifier.ModifierId).Add(Modifier);
	}

	// 执行修改器合并
	for (TPair<FName, TArray<FTcsAttributeModifierInstance>>& Pair : ModifiersToMerge)
	{
		if (Pair.Value.IsEmpty())
		{
			continue;
		}

		if (!Pair.Value[0].ModifierDef)
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ModifierDef is null for ModifierId: %s"),
				*FString(__FUNCTION__),
				*Pair.Value[0].ModifierId.ToString());
			continue;
		}
		const UTcsAttributeModifierDefinition* ModDef = Pair.Value[0].ModifierDef;

		// No merger: do not merge, keep all instances.
		if (!ModDef->MergerType)
		{
			MergedModifiers.Append(Pair.Value);
			continue;
		}

		auto Merger = ModDef->MergerType->GetDefaultObject<UTcsAttributeModifierMerger>();
		Merger->Merge(Pair.Value, MergedModifiers);
	}
}

void UTcsAttributeComponent::ClampAttributeValueInRange(
	const FName& AttributeName,
	float& NewValue,
	float* OutMinValue,
	float* OutMaxValue,
	const TMap<FName, float>* WorkingValues)
{
	const FTcsAttributeInstance* Attribute = Attributes.Find(AttributeName);
	if (!Attribute)
	{
		return;
	}
	const FTcsAttributeRange& Range = Attribute->AttributeDef->AttributeRange;

	// 先解析最小边界。若传播过程传入了 WorkingValues，则优先读取工作集里的候选值，
	// 这样同一轮 fixpoint 可以看到“尚未提交到组件但已经在本轮推导出的新值”。
	float MinValue = TNumericLimits<float>::Lowest();
	switch (Range.MinValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		break;
	case ETcsAttributeRangeType::ART_Static:
		MinValue = Range.MinValue;
		break;
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MinValueAttribute))
				{
					MinValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
			}
			break;
		}
	}

	// 最大边界与最小边界同理，也优先读取工作集，保证传播阶段的依赖解析基于最新候选状态。
	float MaxValue = TNumericLimits<float>::Max();
	switch (Range.MaxValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		break;
	case ETcsAttributeRangeType::ART_Static:
		MaxValue = Range.MaxValue;
		break;
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MaxValueAttribute))
				{
					MaxValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
			}
			break;
		}
	}

	// 到这里 Min/Max 已经解析完毕，真正“如何在范围内修正”的策略交给 ClampStrategy。
	// 依赖声明属于 CollectDependentAttributes 的职责，不在这里推导。
	TSubclassOf<UTcsAttributeClampStrategy> StrategyClass = Attribute->AttributeDef->ClampStrategyClass;
	if (StrategyClass)
	{
		UTcsAttributeClampStrategy* StrategyCDO = StrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();

		FTcsAttributeClampContextBase Context(
			this,
			AttributeName,
			Attribute->AttributeDef,
			Attribute,
			WorkingValues
		);

		const FInstancedStruct& Config = Attribute->AttributeDef->ClampStrategyConfig;

		NewValue = StrategyCDO->Clamp(NewValue, MinValue, MaxValue, Context, Config);

		UE_LOG(LogTcsAttribute, Verbose, TEXT("[%s] Attribute %s clamped using strategy %s: Value=%f, Min=%f, Max=%f"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*StrategyClass->GetName(),
			NewValue,
			MinValue,
			MaxValue);
	}
	else
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ClampStrategyClass is null for attribute %s, using FMath::Clamp as fallback"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
		NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	}

	if (OutMinValue)
	{
		*OutMinValue = MinValue;
	}
	if (OutMaxValue)
	{
		*OutMaxValue = MaxValue;
	}
}

bool UTcsAttributeComponent::TryBuildDeclaredRangeConstraintDependents(
	TMap<FName, TSet<FName>>& OutDependents) const
{
	OutDependents.Reset();
	OutDependents.Reserve(Attributes.Num());

	TArray<FName> DeclaredDependencies;
	DeclaredDependencies.Reserve(4);

	// 对每个属性询问“我的 Clamp 结果依赖哪些属性”，再反向建图成“谁变化后需要重新检查我”。
	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		const FName AttributeName = Pair.Key;
		const FTcsAttributeInstance& Attribute = Pair.Value;
		if (!Attribute.AttributeDef || !Attribute.AttributeDef->ClampStrategyClass)
		{
			return false;
		}

		UTcsAttributeClampStrategy* StrategyCDO = Attribute.AttributeDef->ClampStrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();
		if (!StrategyCDO)
		{
			return false;
		}

		DeclaredDependencies.Reset();
		if (!StrategyCDO->CollectDependentAttributes(AttributeName, Attribute.AttributeDef, Attribute.AttributeDef->ClampStrategyConfig, DeclaredDependencies))
		{
			// 只要有一个策略不给出完整声明，就不能证明局部传播闭包正确，只能回退到全局 fixpoint。
			return false;
		}

		for (const FName DependencyAttribute : DeclaredDependencies)
		{
			// 忽略空依赖、自依赖和组件内不存在的属性，避免构造出无效传播边。
			if (DependencyAttribute.IsNone() || DependencyAttribute == AttributeName || !Attributes.Contains(DependencyAttribute))
			{
				continue;
			}

			OutDependents.FindOrAdd(DependencyAttribute).Add(AttributeName);
		}
	}

	return true;
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraints(
	const TSet<FName>& DirtyAttributes,
	bool bBroadcastEvents)
{
	EnforceAttributeRangeConstraintsInternal(&DirtyAttributes, bBroadcastEvents);
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraints(bool bBroadcastEvents)
{
	EnforceAttributeRangeConstraintsInternal(nullptr, bBroadcastEvents);
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraintsInternal(
	const TSet<FName>* DirtyAttributes,
	bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_EnforceAttributeRangeConstraints);

	const int32 MaxIterations = 8; // 防止无限循环
	int32 Iteration = 0;
	bool bAnyChanged = true;

	// 工作集承接整轮传播的中间态。
	// 在最终收敛前，不直接写回 Attributes，避免半更新状态污染后续依赖解析。
	TMap<FName, float> WorkingBaseValues;
	TMap<FName, float> WorkingCurrentValues;
	WorkingBaseValues.Reserve(Attributes.Num());
	WorkingCurrentValues.Reserve(Attributes.Num());

	// 初始化工作集
	for (auto& Pair : Attributes)
	{
		WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
		WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
	}

	TMap<FName, TSet<FName>> DeclaredDependents;
	const bool bTryLocalPropagation = DirtyAttributes && !DirtyAttributes->IsEmpty();
	const bool bUseDeclaredLocalPropagation = bTryLocalPropagation && TryBuildDeclaredRangeConstraintDependents(DeclaredDependents);

	if (bUseDeclaredLocalPropagation)
	{
		// 从本轮脏属性出发，只沿“已声明依赖它们的属性”继续扩散，避免无意义的全表扫描。
		TSet<FName> PendingAttributes = *DirtyAttributes;

		while (!PendingAttributes.IsEmpty() && Iteration < MaxIterations)
		{
			Iteration++;
			bool bAnyChangedThisPass = false;

			TArray<FName> AttributesToProcess;
			AttributesToProcess.Reserve(PendingAttributes.Num());
			// 先冻结本轮要处理的节点，再把新脏节点留到下一轮，避免边遍历边扩队列导致语义混乱。
			for (const FName PendingAttribute : PendingAttributes)
			{
				AttributesToProcess.Add(PendingAttribute);
			}
			PendingAttributes.Empty();

			for (const FName AttributeName : AttributesToProcess)
			{
				if (!Attributes.Contains(AttributeName))
				{
					continue;
				}

				bool bAttributeChanged = false;

				float OldBase = WorkingBaseValues.FindRef(AttributeName);
				float NewBase = OldBase;
				ClampAttributeValueInRange(AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
				if (!FMath::IsNearlyEqual(OldBase, NewBase))
				{
					WorkingBaseValues.Add(AttributeName, NewBase);
					bAnyChangedThisPass = true;
					bAttributeChanged = true;
				}

				float OldCurrent = WorkingCurrentValues.FindRef(AttributeName);
				float NewCurrent = OldCurrent;
				ClampAttributeValueInRange(AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
				if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
				{
					WorkingCurrentValues.Add(AttributeName, NewCurrent);
					bAnyChangedThisPass = true;
					bAttributeChanged = true;
				}

				// 只有当前属性在这一轮真的被 Clamp 改动了，才有必要继续唤醒它的声明式依赖下游。
				if (bAttributeChanged)
				{
					if (const TSet<FName>* Dependents = DeclaredDependents.Find(AttributeName))
					{
						for (const FName DependentAttribute : *Dependents)
						{
							PendingAttributes.Add(DependentAttribute);
						}
					}
				}
			}

			if (!bAnyChangedThisPass && PendingAttributes.IsEmpty())
			{
				break;
			}
		}

		if (!PendingAttributes.IsEmpty())
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] Declared local propagation did not converge for entity '%s', fallback to full fixpoint."),
				*FString(__FUNCTION__),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

			// 局部传播不收敛时，不继续沿用中间工作集，直接回到组件当前已提交状态重新跑全局 fixpoint。
			// 这样可以避免把“未证明正确的局部中间态”带进保守路径。
			WorkingBaseValues.Reset();
			WorkingCurrentValues.Reset();
			WorkingBaseValues.Reserve(Attributes.Num());
			WorkingCurrentValues.Reserve(Attributes.Num());
			for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
			{
				WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
				WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
			}

			Iteration = 0;
			bAnyChanged = true;
		}
		else
		{
			bAnyChanged = false;
		}
	}

	if (!bUseDeclaredLocalPropagation || bAnyChanged)
	{
		// 未提供脏属性、策略未声明依赖，或局部传播未收敛时，回退到现有全局 fixpoint。
		Iteration = 0;
		bAnyChanged = true;
		while (bAnyChanged && Iteration < MaxIterations)
		{
			bAnyChanged = false;
			Iteration++;

			// 这里故意回到全表扫描。
			// 当依赖闭包未知时，只有重复检查所有属性，才能保守地逼近稳定态。
			for (auto& Pair : Attributes)
			{
				FName AttributeName = Pair.Key;

				// 阶段1: Clamp BaseValue，使用 WorkingBaseValues 解析动态范围
				float OldBase = WorkingBaseValues[AttributeName];
				float NewBase = OldBase;
				ClampAttributeValueInRange(AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
				if (!FMath::IsNearlyEqual(OldBase, NewBase))
				{
					WorkingBaseValues[AttributeName] = NewBase;
					bAnyChanged = true;
				}

				// 阶段2: Clamp CurrentValue，使用 WorkingCurrentValues 解析动态范围
				float OldCurrent = WorkingCurrentValues[AttributeName];
				float NewCurrent = OldCurrent;
				ClampAttributeValueInRange(AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
				if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
				{
					WorkingCurrentValues[AttributeName] = NewCurrent;
					bAnyChanged = true;
				}
			}
		}
	}

	// 检查是否收敛
	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Max iterations reached for entity '%s', possible circular dependency"),
			*FString(__FUNCTION__),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d Iterations=%d ReachedMaxIterations=%s"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		Iteration,
		Iteration >= MaxIterations ? TEXT("true") : TEXT("false"));

	// 所有 Clamp 传播完成后再统一提交，避免半更新状态污染后续依赖解析和广播结果。
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BaseChangePayloads.Reserve(Attributes.Num());
	CurrentChangePayloads.Reserve(Attributes.Num());
	BoundaryPayloads.Reserve(Attributes.Num());

	for (auto& Pair : Attributes)
	{
		FName AttributeName = Pair.Key;
		FTcsAttributeInstance& Attribute = Pair.Value;

		// 提交阶段只负责把收敛后的工作集写回组件，并补发变更/边界事件，不再参与依赖推导。
		// 这样“求值传播”和“对外可见状态提交”两个阶段是解耦的。
		// 提交 BaseValue
		float NewBase = WorkingBaseValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.BaseValue, NewBase))
		{
			float OldBase = Attribute.BaseValue;
			Attribute.BaseValue = NewBase;

			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldBase;
			Payload.NewValue = NewBase;
			BaseChangePayloads.Add(Payload);

			// 检测是否达到边界
			float RangeMin = NewBase;
			float RangeMax = NewBase;
			ClampAttributeValueInRange(AttributeName, NewBase, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewBase, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewBase, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(AttributeName, bIsMaxBoundary, OldBase, NewBase, BoundaryValue);
			}
		}

		// 提交 CurrentValue
		float NewCurrent = WorkingCurrentValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.CurrentValue, NewCurrent))
		{
			float OldCurrent = Attribute.CurrentValue;
			Attribute.CurrentValue = NewCurrent;

			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldCurrent;
			Payload.NewValue = NewCurrent;
			CurrentChangePayloads.Add(Payload);

			// 检测是否达到边界
			float RangeMin = NewCurrent;
			float RangeMax = NewCurrent;
			ClampAttributeValueInRange(AttributeName, NewCurrent, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewCurrent, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewCurrent, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(AttributeName, bIsMaxBoundary, OldCurrent, NewCurrent, BoundaryValue);
			}
		}
	}

	// 广播事件
	if (bBroadcastEvents && BaseChangePayloads.Num() > 0)
	{
		BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}
	if (bBroadcastEvents && CurrentChangePayloads.Num() > 0)
	{
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}
	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}
}
