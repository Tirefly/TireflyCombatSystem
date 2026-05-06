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

bool UTcsAttributeComponent::GetAttributeBaseValue(FName AttributeName, float& OutValue) const
{
	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->BaseValue;
		return true;
	}

	return false;
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeValues() const
{
	TMap<FName, float> AttributeValues;
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.CurrentValue);
	}
	
	return AttributeValues;
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeBaseValues() const
{
	TMap<FName, float> AttributeValues;
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

void UTcsAttributeComponent::BroadcastAttributeModifierAddedEvent(
	const FTcsAttributeModifierInstance& ModifierInstance) const
{
	if (OnAttributeModifierAdded.IsBound())
	{
		OnAttributeModifierAdded.Broadcast(ModifierInstance);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierRemovedEvent(
	const FTcsAttributeModifierInstance& ModifierInstance) const
{
	if (OnAttributeModifierRemoved.IsBound())
	{
		OnAttributeModifierRemoved.Broadcast(ModifierInstance);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierUpdatedEvent(
	const FTcsAttributeModifierInstance& ModifierInstance) const
{
	if (OnAttributeModifierUpdated.IsBound())
	{
		OnAttributeModifierUpdated.Broadcast(ModifierInstance);
	}
}

void UTcsAttributeComponent::BroadcastAttributeReachedBoundaryEvent(
	FName AttributeName,
	bool bIsMaxBoundary,
	float OldValue,
	float NewValue,
	float BoundaryValue) const
{
	if (OnAttributeReachedBoundary.IsBound())
	{
		OnAttributeReachedBoundary.Broadcast(AttributeName, bIsMaxBoundary, OldValue, NewValue, BoundaryValue);
	}
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
	EnforceAttributeRangeConstraints();
	return true;
}

void UTcsAttributeComponent::AddAttributes(const TArray<FName>& AttributeNames)
{
	UTcsAttributeManagerSubsystem* Mgr = ResolveAttributeManager();
	if (!Mgr)
	{
		return;
	}

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
		}
	}

	// 批量添加完成后统一传播动态范围约束
	EnforceAttributeRangeConstraints();
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

	// 设置新值并 Clamp
	Attribute->BaseValue = NewValue;
	ClampAttributeValueInRange(AttributeName, Attribute->BaseValue);

	// 重新计算 Current 值（应用修改器）
	RecalculateAttributeCurrentValues();

	// 触发事件
	if (bTriggerEvents && !FMath::IsNearlyEqual(OldValue, Attribute->BaseValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldValue;
		Payload.NewValue = Attribute->BaseValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		BroadcastAttributeBaseValueChangeEvent(Payloads);
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

	// 设置新值并 Clamp
	Attribute->CurrentValue = NewValue;
	ClampAttributeValueInRange(AttributeName, Attribute->CurrentValue);

	// 传播动态范围约束（该属性值变化可能影响其他属性的动态范围边界）
	EnforceAttributeRangeConstraints();

	// 触发事件
	if (bTriggerEvents && !FMath::IsNearlyEqual(OldValue, Attribute->CurrentValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldValue;
		Payload.NewValue = Attribute->CurrentValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		BroadcastAttributeValueChangeEvent(Payloads);
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

	// 重置 Base 和 Current 值
	float OldBase = Attribute->BaseValue;
	float OldCurrent = Attribute->CurrentValue;
	Attribute->BaseValue = InitValue;
	Attribute->CurrentValue = InitValue;

	// Clamp 值
	ClampAttributeValueInRange(AttributeName, Attribute->BaseValue);
	ClampAttributeValueInRange(AttributeName, Attribute->CurrentValue);

	// 触发事件
	if (!FMath::IsNearlyEqual(OldBase, Attribute->BaseValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldBase;
		Payload.NewValue = Attribute->BaseValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	if (!FMath::IsNearlyEqual(OldCurrent, Attribute->CurrentValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldCurrent;
		Payload.NewValue = Attribute->CurrentValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		BroadcastAttributeValueChangeEvent(Payloads);
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
		NewlyAddedModifiers.Reserve(ModifiersToApply.Num());

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
					if (const TArray<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceId))
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
				TArray<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(ModifierToStore.SourceHandle.Id);
				InstIds.AddUnique(ModifierToStore.ModifierInstId);
			}
		}

		// 广播新增事件
		for (const FTcsAttributeModifierInstance& Updated : UpdatedExistingModifiers)
		{
			BroadcastAttributeModifierUpdatedEvent(Updated);
		}

		for (const FTcsAttributeModifierInstance& Added : NewlyAddedModifiers)
		{
			BroadcastAttributeModifierAddedEvent(Added);
		}
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
			TArray<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(RemovedModifier.SourceHandle.Id);
			if (InstIdsPtr)
			{
				// TODO(Perf): 逐个 Remove 导致 O(K^2) 批量退化；见 SourceHandleIdToModifierInstIds 注释。
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

		BroadcastAttributeModifierRemovedEvent(RemovedModifier);
		bModified = true;
	}

	// 如果确实有属性修改器被移除，则更新属性的当前值
	if (bModified)
	{
		RecalculateAttributeCurrentValues(BatchId);
	}
}

bool UTcsAttributeComponent::RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"), *FString(__FUNCTION__));
		return false;
	}

	// 使用稳定 ID 缓存查找匹配的修改器
	const TArray<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	// 先拷贝 ID 列表（避免在迭代中修改）
	// TODO(Perf): 当前实现逐个委托给 RemoveModifier，桶内 Remove 导致 O(K^2)。
	//   优化优先级：1) 将 SourceHandleIdToModifierInstIds 桶类型改为 TSet<int32>；
	//              2) 或提取 RemoveModifierInternal(无桶维护)，在末尾一次性整桶丢弃。
	TArray<int32> InstIdsCopy = *InstIdsPtr;

	// 收集要移除的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (int32 ModifierInstId : InstIdsCopy)
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
			ModifiersToRemove.Add(Modifier);
		}
	}

	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(ModifiersToRemove);
		return true;
	}

	return false;
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

	const TArray<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	OutModifiers.Empty();
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
				if (TArray<int32>* InstIdsPtr = SourceHandleIdToModifierInstIds.Find(OldSourceId))
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
				TArray<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
				InstIds.AddUnique(Modifier.ModifierInstId);
			}
		}
		else if (NewSourceId >= 0)
		{
			TArray<int32>& InstIds = SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
			InstIds.AddUnique(Modifier.ModifierInstId);
		}

		BroadcastAttributeModifierUpdatedEvent(Modifier);

		bModified = true;
	}

	if (bModified)
	{
		RecalculateAttributeCurrentValues(BatchId);
	}
}


// ============================================================
// #pragma region AttributeCalculation
// ============================================================

void UTcsAttributeComponent::RecalculateAttributeBaseValues(const TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(Modifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = GetAttributeBaseValues();
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

		// 缓存属性基础值的上一次修改最终值
		TMap<FName, float> LastModifiedResults = BaseValues;

		// 执行修改器
		auto Execution = ModDef->ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, BaseValues);

		// 记录属性修改过程
		for (const TPair<FName, float>& LastPair : LastModifiedResults)
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

	// 对修改后的属性基础值进行范围修正，然后更新属性基础值
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

			// 记录属性修改事件的最终结果
			if (FTcsAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
			{
				Payload->NewValue = Pair.Value;
				Payload->OldValue = Attribute->BaseValue;
			}

			// 把属性基础值的最终修改赋值
			const float OldValue = Attribute->BaseValue;
			Attribute->BaseValue = Pair.Value;

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BroadcastAttributeReachedBoundaryEvent(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// 属性基础值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	// 执行范围约束传播
	EnforceAttributeRangeConstraints();
}

void UTcsAttributeComponent::RecalculateAttributeCurrentValues(int64 ChangeBatchId)
{
	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(AttributeModifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;

	// 获取属性基础值，用于计算
	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	// 用于更新计算的临时属性值容器，基于属性的基础值
	TMap<FName, float> CurrentValuesToCalc = BaseValues;

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

		// 缓存属性当前值的上一次修改最终值
		TMap<FName, float> LastModifiedResults = CurrentValuesToCalc;

		// 执行修改器
		auto Execution = ModDef->ModifierType->GetDefaultObject<UTcsAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, CurrentValuesToCalc);

		// 记录属性修改过程，需要属性修改器的更新时间为最新
		for (const TPair<FName, float>& LastPair : LastModifiedResults)
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

	// 对修改后的属性当前值进行范围修正，然后更新属性当前值
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

			// 记录属性修改事件的最终结果，因为修改器移除导致的属性当前值变更，不会有修改记录，所以需要在此处查漏补缺
			FTcsAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(Pair.Key);
			Payload.AttributeName = Pair.Key;
			Payload.NewValue = Pair.Value;
			Payload.OldValue = Attribute->CurrentValue;

			// 把属性当前值的最终修改赋值
			const float OldValue = Attribute->CurrentValue;
			Attribute->CurrentValue = Pair.Value;

			// 广播达到边界值事件
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BroadcastAttributeReachedBoundaryEvent(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// 属性当前值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		BroadcastAttributeValueChangeEvent(Payloads);
	}

	// 执行范围约束传播
	EnforceAttributeRangeConstraints();
}

void UTcsAttributeComponent::MergeAttributeModifiers(
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	// 按 ModifierId 整理所有属性修改器，方便后续执行修改器合并
	TMap<FName, TArray<FTcsAttributeModifierInstance>> ModifiersToMerge;
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

	// 计算属性范围的最小值
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

	// 计算属性范围的最大值
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

	// 执行属性值 Clamp（使用策略对象）
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

void UTcsAttributeComponent::EnforceAttributeRangeConstraints()
{
	const int32 MaxIterations = 8; // 防止无限循环
	int32 Iteration = 0;
	bool bAnyChanged = true;

	// 工作集: 当前正在处理的值
	TMap<FName, float> WorkingBaseValues;
	TMap<FName, float> WorkingCurrentValues;

	// 初始化工作集
	for (auto& Pair : Attributes)
	{
		WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
		WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
	}

	// 迭代直到稳定
	while (bAnyChanged && Iteration < MaxIterations)
	{
		bAnyChanged = false;
		Iteration++;

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

	// 检查是否收敛
	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Max iterations reached for entity '%s', possible circular dependency"),
			*FString(__FUNCTION__),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
	}

	// 提交工作集到组件
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;

	for (auto& Pair : Attributes)
	{
		FName AttributeName = Pair.Key;
		FTcsAttributeInstance& Attribute = Pair.Value;

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
				BroadcastAttributeReachedBoundaryEvent(AttributeName, bIsMaxBoundary, OldBase, NewBase, BoundaryValue);
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
				BroadcastAttributeReachedBoundaryEvent(AttributeName, bIsMaxBoundary, OldCurrent, NewCurrent, BoundaryValue);
			}
		}
	}

	// 广播事件
	if (BaseChangePayloads.Num() > 0)
	{
		BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}
	if (CurrentChangePayloads.Num() > 0)
	{
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}
}
