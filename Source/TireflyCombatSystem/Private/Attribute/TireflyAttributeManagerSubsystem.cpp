// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TireflyAttributeManagerSubsystem.h"

#include "TireflyCombatEntityInterface.h"
#include "TireflyCombatSystemLogChannels.h"
#include "TireflyCombatSystemSettings.h"
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
	
	const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>();
	const UDataTable* AttributeDefTable = Settings->AttributeDefTable.LoadSynchronous();
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
	
	const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>();
	const UDataTable* AttributeDefTable = Settings->AttributeDefTable.LoadSynchronous();
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

	// 区分好修改属性Base值和Current值的两种修改器
	for (FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		switch (Modifier.ModifierDef.ModifierMode)
		{
		case ETireflyAttributeModifierMode::BaseValue:
			{
				ModifiersToExecute.Add(Modifier);
				break;
			}
		case ETireflyAttributeModifierMode::CurrentValue:
			{
				Modifier.ApplyTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
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
	for (FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		if (AttributeComponent->AttributeModifiers.Contains(Modifier))
		{
			const int32 ModifierInstId = AttributeComponent->AttributeModifiers.Find(Modifier);
			Modifier.UpdateTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
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

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	for (const FTireflyAttributeModifierInstance& Modifier : Modifiers)
	{
		if (!Modifier.ModifierDef.ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type"),
				*FString(__FUNCTION__),
				*Modifier.ModifierDef.ModifierName.ToString());
			return;
		}

		auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTireflyAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, BaseValues);
	}
	
	// 对修改后的属性基础值进行范围修正，然后更新属性基础值
	for (TPair<FName, float>& Pair : BaseValues)
	{
		if (FTireflyAttributeInstance* Attr = AttributeComponent->Attributes.Find(Pair.Key))
		{
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value);
			Attr->BaseValue = Pair.Value;
		}
	}

	// TODO: 属性基础值更新广播
}

void UTireflyAttributeManagerSubsystem::RecalculateAttributeCurrentValues(const AActor* CombatEntity)
{
	UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 先声明用于更新计算的临时属性值容器
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	TMap<FName, float> CurrentValues = BaseValues;

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TMap<FName, TArray<FTireflyAttributeModifierInstance>> ModifiersToMerge;
	for (const FTireflyAttributeModifierInstance& Modifier : AttributeComponent->AttributeModifiers)
	{
		ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName).Add(Modifier);
	}
	
	// 执行修改器合并
	TArray<FTireflyAttributeModifierInstance> MergedModifiers;
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

	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

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

		auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTireflyAttributeModifierExecution>();
		Execution->Execute(Modifier, BaseValues, CurrentValues);
	}

	// 对修改后的属性当前值进行范围修正，然后更新属性当前值
	for (TPair<FName, float>& Pair : CurrentValues)
	{
		if (FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value);
			Attribute->CurrentValue = Pair.Value;
		}
	}

	// TODO: 属性当前值更新广播
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
	case ETireflyAttributeRangeType::None:
		{
			break;
		}
	case ETireflyAttributeRangeType::Static:
		{
			MinValue = Range.MinValue;
			break;
		}
	case ETireflyAttributeRangeType::Dynamic:
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
	case ETireflyAttributeRangeType::None:
		{
			break;
		}
	case ETireflyAttributeRangeType::Static:
		{
			MaxValue = Range.MaxValue;
			break;
		}
	case ETireflyAttributeRangeType::Dynamic:
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
