// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TireflyAttributeManagerSubsystem.h"

#include "TireflyCombatEntityInterface.h"
#include "TireflyCombatSystemLibrary.h"
#include "TireflyCombatSystemLogChannels.h"
#include "Attribute/TireflyAttribute.h"
#include "Attribute/TireflyAttributeComponent.h"
#include "Attribute/TireflyAttributeModifierExecution.h"
#include "Attribute/TireflyAttributeModifierMerger.h"


void UTireflyAttributeManagerSubsystem::AddAttribute(
	AActor* CombatEntity,
	FName AttributeName,
	float InitValue)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"), *FString(__FUNCTION__));
		return;
	}
	
	const UDataTable* AttributeDefTable = UTireflyCombatSystemLibrary::GetAttributeDefTable();
	if (!IsValid(AttributeDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"), *FString(__FUNCTION__));
		return;
	}

	const auto AttrDef = AttributeDefTable->FindRow<FTireflyAttributeDefinition>(AttributeName, FString(__FUNCTION__));
	if (!AttrDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable does not contain AttributeName %s"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
		return;
	}

	FTireflyAttributeInstance AttrInst = FTireflyAttributeInstance(*AttrDef, ++GlobalAttributeInstanceIdMgr, CombatEntity, InitValue);
	AttributeComponent->Attributes.Add(AttributeName, AttrInst);
}

void UTireflyAttributeManagerSubsystem::AddAttributes(AActor* CombatEntity, const TArray<FName>& AttributeNames)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"), *FString(__FUNCTION__));
		return;
	}
	
	const UDataTable* AttributeDefTable = UTireflyCombatSystemLibrary::GetAttributeDefTable();
	if (!IsValid(AttributeDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"), *FString(__FUNCTION__));
		return;
	}

	for (const FName AttributeName : AttributeNames)
	{
		const auto AttrDef = AttributeDefTable->FindRow<FTireflyAttributeDefinition>(AttributeName, FString(__FUNCTION__));
		if (!AttrDef)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable does not contain AttributeName %s"),
				*FString(__FUNCTION__),
				*AttributeName.ToString());
			continue;
		}

		FTireflyAttributeInstance AttrInst = FTireflyAttributeInstance(*AttrDef, ++GlobalAttributeInstanceIdMgr, CombatEntity);
		AttributeComponent->Attributes.Add(AttributeName, AttrInst);
	}
}

UTireflyAttributeComponent* UTireflyAttributeManagerSubsystem::GetAttributeComponent(const AActor* CombatEntity)
{
	if (!IsValid(CombatEntity))
	{
		return nullptr;
	}

	if (CombatEntity->Implements<UTireflyCombatEntityInterface>())
	{
		return ITireflyCombatEntityInterface::Execute_GetAttributeComponent(CombatEntity);
	}

	return CombatEntity->FindComponentByClass<UTireflyAttributeComponent>();
}

bool UTireflyAttributeManagerSubsystem::CreateAttributeModifier(
	FName ModifierId,
	FName SourceName,
	AActor* Instigator,
	AActor* Target,
	const TMap<FName, float>& Operands,
	FTireflyAttributeModifierInstance& OutModifierInst)
{
	if (!IsValid(Instigator) || !IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator or Target is not valid"), *FString(__FUNCTION__));
		return false;
	}
	
	UDataTable* AttributeModifierDefTable = UTireflyCombatSystemLibrary::GetAttributeModifierDefTable();
	if (!IsValid(AttributeModifierDefTable))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable in TcsDevSettings is not valid"), *FString(__FUNCTION__));
		return false;
	}

	FTireflyAttributeModifierDefinition* ModifierDef = AttributeModifierDefTable->FindRow<FTireflyAttributeModifierDefinition>(ModifierId, FString(__FUNCTION__));
	if (!ModifierDef)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefTable does not contain ModifierId %s"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	// TODO: 验证Source是否有效
	// TODO: 验证Operands是否正确

	OutModifierInst = FTireflyAttributeModifierInstance();
	OutModifierInst.ModifierDef = *ModifierDef;
	OutModifierInst.ModifierInstId = ++GlobalAttributeModifierInstanceIdMgr;
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = Target;
	OutModifierInst.Operands = Operands;
	OutModifierInst.SourceName = SourceName;

	return true;
}

void UTireflyAttributeManagerSubsystem::ApplyModifier(
	AActor* CombatEntity,
	TArray<FTireflyAttributeModifierInstance>& Modifiers)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent) || Modifiers.IsEmpty())
	{
		return;
	}

	TArray<FTireflyAttributeModifierInstance> ModifiersToExecute;// 对属性Base值执行操作的属性修改器
	TArray<FTireflyAttributeModifierInstance> ModifiersToApply;// 对属性Current值应用的属性修改器
	const int64 UtcNow = FDateTime::UtcNow().ToUnixTimestamp();

	// 区分好修改属性Base值和Current值的两种修改器
	for (FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		switch (Modifier.ModifierDef.ModifierMode)
		{
		case ETireflyAttributeModifierMode::BaseValue:
			{
				// 添加到执行列表
				ModifiersToExecute.Add(Modifier);
				break;
			}
		case ETireflyAttributeModifierMode::CurrentValue:
			{
				// 添加到应用列表并设置应用时间戳
				Modifier.ApplyTimestamp = UtcNow;
				Modifier.UpdateTimestamp = UtcNow;
				ModifiersToApply.Add(Modifier);
				break;
			}
		}
	}

	// 先执行针对属性Current值的修改器
	if (!ModifiersToExecute.IsEmpty())
	{
		RecalculateAttributeBaseValues(CombatEntity, ModifiersToExecute);
	}
	
	// 再执行针对属性Base值的修改器
	if (!ModifiersToApply.IsEmpty())
	{
		// 把以经营用过但有改变的属性修改器更新一下，并从待应用列表中移除
		for (FTireflyAttributeModifierInstance& Modifier : AttributeComponent->AttributeModifiers)
		{
			if (ModifiersToApply.Contains(Modifier))
			{
				Modifier.UpdateTimestamp = UtcNow;
				ModifiersToApply.Remove(Modifier);
			}
		}
		AttributeComponent->AttributeModifiers.Append(ModifiersToApply);
	}
	
	// 无论如何，都要重新计算属性Current值
	RecalculateAttributeCurrentValues(CombatEntity);
}

void UTireflyAttributeManagerSubsystem::RemoveModifier(
	AActor* CombatEntity,
	TArray<FTireflyAttributeModifierInstance>& Modifiers)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	bool bModified = false;
	for (const FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		if (AttributeComponent->AttributeModifiers.Remove(Modifier) > 0)
		{
			bModified = true;
		}
	}

	// 如果确实有属性修改器被移除，则更新属性的当前值
	if (bModified)
	{
		RecalculateAttributeCurrentValues(CombatEntity);
	}
}

void UTireflyAttributeManagerSubsystem::HandleModifierUpdated(
	AActor* CombatEntity,
	TArray<FTireflyAttributeModifierInstance>& Modifiers)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	bool bModified = false;
	const int64 UtcNow = FDateTime::UtcNow().ToUnixTimestamp();
	for (FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		if (AttributeComponent->AttributeModifiers.Contains(Modifier))
		{
			const int32 ModifierInstId = AttributeComponent->AttributeModifiers.Find(Modifier);
			Modifier.UpdateTimestamp = UtcNow;
			AttributeComponent->AttributeModifiers[ModifierInstId] = Modifier;
			
			bModified = true;
		}
	}

	if (bModified)
	{
		RecalculateAttributeCurrentValues(CombatEntity);
	}
}

void UTireflyAttributeManagerSubsystem::RecalculateAttributeBaseValues(
	const AActor* CombatEntity,
	const TArray<FTireflyAttributeModifierInstance>& Modifiers)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTireflyAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(CombatEntity, Modifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTireflyAttributeChangeEventPayload> ChangeEventPayloads;

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	for (const FTireflyAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef.ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__), 
				*Modifier.ModifierDef.ModifierName.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}

		// 缓存属性基础值的上一次修改最终值
		TMap<FName, float> LastModifiedResults = BaseValues;

		// 执行修改器
		auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTireflyAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, BaseValues);

		// 记录属性修改过程
		for (const TPair<FName, float>& LastPair : LastModifiedResults)
		{
			const float& NewValue = BaseValues.FindRef(LastPair.Key);
			if (NewValue != LastPair.Value)
			{
				FTireflyAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
				Payload.AttributeName = LastPair.Key;
				float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceName);
				PayloadValue += NewValue - LastPair.Value;
			}
		}
	}
	
	// 对修改后的属性基础值进行范围修正，然后更新属性基础值
	for (TPair<FName, float>& Pair : BaseValues)
	{
		if (FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value);
			if (FMath::IsNearlyEqual(Attribute->BaseValue, Pair.Value))
			{
				continue;
			}

			// 记录属性修改事件的最终结果
			if (FTireflyAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
			{
				Payload->NewValue = Pair.Value;
				Payload->OldValue = Attribute->BaseValue;
			}

			// 把属性基础值的最终修改赋值
			Attribute->BaseValue = Pair.Value;
		}
	}

	// 属性基础值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTireflyAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(Payloads);
	}
}

void UTireflyAttributeManagerSubsystem::RecalculateAttributeCurrentValues(const AActor* CombatEntity)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTireflyAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(CombatEntity, AttributeComponent->AttributeModifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTireflyAttributeChangeEventPayload> ChangeEventPayloads;
	int64 UtcNow = FDateTime::UtcNow().ToUnixTimestamp();

	// 获取属性基础值，用于计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	// 重新计算属性当前新值前，保存属性旧值
	TMap<FName, float> OldCurrentValues = AttributeComponent->GetAttributeValues();
	// 用于更新计算的临时属性值容器，基于属性的基础值
	TMap<FName, float> CurrentValuesToCalc = BaseValues;

	// 执行属性修改器的修改计算
	for (const FTireflyAttributeModifierInstance& Modifier : MergedModifiers)
	{
		if (!Modifier.ModifierDef.ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__), 
				*Modifier.ModifierDef.ModifierName.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}

		// 缓存属性当前值的上一次修改最终值
		TMap<FName, float> LastModifiedResults = CurrentValuesToCalc;

		// 执行修改器
		auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTireflyAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, CurrentValuesToCalc);

		// 记录属性修改过程，需要属性修改器的更新时间为最新
		for (const TPair<FName, float>& LastPair : LastModifiedResults)
		{
			const float& NewValue = CurrentValuesToCalc.FindRef(LastPair.Key);
			if (NewValue != LastPair.Value && Modifier.UpdateTimestamp == UtcNow)
			{
				FTireflyAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(LastPair.Key);
				Payload.AttributeName = LastPair.Key;
				float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceName);
				PayloadValue += NewValue - LastPair.Value;
			}
		}
	}

	// 对修改后的属性当前值进行范围修正，然后更新属性当前值
	for (TPair<FName, float>& Pair : CurrentValuesToCalc)
	{
		if (FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value);
			if (FMath::IsNearlyEqual(Attribute->CurrentValue, Pair.Value))
			{
				continue;
			}

			// 记录属性修改事件的最终结果，因为修改器移除导致的属性当前值变更，不会有修改记录，所以需要在此处查漏补缺
			FTireflyAttributeChangeEventPayload & Payload = ChangeEventPayloads.FindOrAdd(Pair.Key);
			Payload.AttributeName = Pair.Key;
			Payload.NewValue = Pair.Value;
			Payload.OldValue = Attribute->CurrentValue;

			// 把属性当前值的最终修改赋值
			Attribute->CurrentValue = Pair.Value;
		}
	}
	
	// 属性当前值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTireflyAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		AttributeComponent->BroadcastAttributeValueChangeEvent(Payloads);
	}
}

void UTireflyAttributeManagerSubsystem::MergeAttributeModifiers(
	const AActor* CombatEntity,
	const TArray<FTireflyAttributeModifierInstance>& Modifiers,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TMap<FName, TArray<FTireflyAttributeModifierInstance>> ModifiersToMerge;
	for (const FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName).Add(Modifier);
	}
	
	// 执行修改器合并
	for (TPair<FName, TArray<FTireflyAttributeModifierInstance>>& Pair : ModifiersToMerge)
	{
		if (Pair.Value.IsEmpty() || !Pair.Value[0].ModifierDef.MergerType)
		{
			UE_LOG(LogTcsAttrModMerger, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierMerger type. Entity: %s"),
				*FString(__FUNCTION__), 
				*Pair.Key.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}

		auto Merger = Pair.Value[0].ModifierDef.MergerType->GetDefaultObject<UTireflyAttributeModifierMerger>();
		Merger->Merge(Pair.Value, MergedModifiers);
	}
}

void UTireflyAttributeManagerSubsystem::ClampAttributeValueInRange(
	UTireflyAttributeComponent* AttributeComponent,
	const FName& AttributeName,
	float& NewValue)
{
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
	if (!Attribute)
	{
		return;
	}
	const FTireflyAttributeRange& Range = Attribute->AttributeDef.AttributeRange;

	// 计算属性范围的最小值
	float MinValue = NewValue;
	switch (Range.MinValueType)
	{
	case ETireflyAttributeRangeType::ART_None:
		{
			break;
		}
	case ETireflyAttributeRangeType::ART_Static:
		{
			MinValue = Range.MinValue;
			break;
		}
	case ETireflyAttributeRangeType::ART_Dynamic:
		{
			if (!AttributeComponent->GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
				return;
			}
			break;
		}
	}

	// 计算属性范围的最大值
	float MaxValue = NewValue;
	switch (Range.MaxValueType)
	{
	case ETireflyAttributeRangeType::ART_None:
		{
			break;
		}
	case ETireflyAttributeRangeType::ART_Static:
		{
			MaxValue = Range.MaxValue;
			break;
		}
	case ETireflyAttributeRangeType::ART_Dynamic:
		{
			if (!AttributeComponent->GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
				return;
			}
			break;
		}
	}

	// 属性值范围修正
	NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
}
