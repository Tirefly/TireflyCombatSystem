// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeManagerSubsystem.h"

#include "TcsDeveloperSettings.h"
#include "TcsEntityInterface.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttribute.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Attribute/TcsAttributeDefinitionAsset.h"
#include "Attribute/TcsAttributeModifierDefinitionAsset.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"
#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"

#if !WITH_EDITOR
#include "Engine/AssetManager.h"
#endif


void UTcsAttributeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	LoadFromDeveloperSettings();
#else
	LoadFromAssetManager();
#endif
}

void UTcsAttributeManagerSubsystem::LoadFromDeveloperSettings()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Failed to get TcsDeveloperSettings"),
			*FString(__FUNCTION__));
		return;
	}

	// 从 DeveloperSettings 的缓存中加载属性定义
	AttributeDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedAttributeDefinitions())
	{
		const UTcsAttributeDefinitionAsset* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			AttributeDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (AttributeDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] No AttributeDefinitions loaded from DeveloperSettings"),
			*FString(__FUNCTION__));
		return;
	}

	// 从 DeveloperSettings 的缓存中加载属性修改器定义
	AttributeModifierDefinitions.Empty();
	for (const auto& Pair : Settings->GetCachedAttributeModifierDefinitions())
	{
		const UTcsAttributeModifierDefinitionAsset* Asset = Pair.Value.LoadSynchronous();
		if (Asset)
		{
			AttributeModifierDefinitions.Add(Pair.Key, Asset);
		}
		else
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeModifierDefinition: %s"),
				*FString(__FUNCTION__),
				*Pair.Key.ToString());
		}
	}

	if (AttributeModifierDefinitions.Num() == 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] No AttributeModifierDefinitions loaded from DeveloperSettings"),
			*FString(__FUNCTION__));
	}

	// 构建 AttributeTag -> AttributeName 映射
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	for (const auto& Pair : AttributeDefinitions)
	{
		const FName& AttributeName = Pair.Key;
		const UTcsAttributeDefinitionAsset* AttrDef = Pair.Value;

		if (!AttrDef)
		{
			continue;
		}

		// 检查 AttributeTag 是否有效
		if (!AttrDef->AttributeTag.IsValid())
		{
			// 空或无效 Tag，记录 Warning 但不影响运行
			if (AttrDef->AttributeTag != FGameplayTag::EmptyTag)
			{
				UE_LOG(LogTcsAttribute, Warning,
					TEXT("[%s] Attribute '%s' has invalid AttributeTag, skipping Tag mapping"),
					*FString(__FUNCTION__),
					*AttributeName.ToString());
			}
			continue;
		}

		// 检查是否重复
		if (AttributeTagToName.Contains(AttrDef->AttributeTag))
		{
			const FName ExistingName = AttributeTagToName[AttrDef->AttributeTag];
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Duplicate AttributeTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
				*FString(__FUNCTION__),
				*AttrDef->AttributeTag.ToString(),
				*ExistingName.ToString(),
				*AttributeName.ToString());
			continue;
		}

		// 添加到映射
		AttributeTagToName.Add(AttrDef->AttributeTag, AttributeName);
		AttributeNameToTag.Add(AttributeName, AttrDef->AttributeTag);
	}

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Initialized: %d Attributes, %d AttributeModifiers, %d Tag mappings"),
		*FString(__FUNCTION__),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		AttributeTagToName.Num());
}

void UTcsAttributeManagerSubsystem::AddAttribute(
	AActor* CombatEntity,
	FName AttributeName,
	float InitValue)
{
	const UTcsAttributeDefinitionAsset* const* AttrDefPtr = AttributeDefinitions.Find(AttributeName);
	if (!AttrDefPtr || !*AttrDefPtr)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
		return;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return;
	}

	// 防止覆盖已存在的属性
	if (AttributeComponent->Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute '%s' already exists on CombatEntity '%s', skipping add"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return;
	}

	const UTcsAttributeDefinitionAsset* AttrDef = *AttrDefPtr;
	FTcsAttributeInstance AttrInst = FTcsAttributeInstance(AttrDef, AttributeName, ++GlobalAttributeInstanceIdMgr, CombatEntity, InitValue);
	AttributeComponent->Attributes.Add(AttributeName, AttrInst);

	// Clamp initialization values to the configured range (static or dynamic).
	if (FTcsAttributeInstance* Added = AttributeComponent->Attributes.Find(AttributeName))
	{
		float Clamped = Added->BaseValue;
		ClampAttributeValueInRange(AttributeComponent, AttributeName, Clamped);
		Added->BaseValue = Clamped;
		Added->CurrentValue = Clamped;
	}
}

void UTcsAttributeManagerSubsystem::AddAttributes(AActor* CombatEntity, const TArray<FName>& AttributeNames)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return;
	}

	for (const FName AttributeName : AttributeNames)
	{
		// 防止覆盖已存在的属性
		if (AttributeComponent->Attributes.Contains(AttributeName))
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute '%s' already exists on CombatEntity '%s', skipping add"),
				*FString(__FUNCTION__),
				*AttributeName.ToString(),
				*CombatEntity->GetName());
			continue;
		}

		const UTcsAttributeDefinitionAsset* const* AttrDefPtr = AttributeDefinitions.Find(AttributeName);
		if (!AttrDefPtr || !*AttrDefPtr)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefinition '%s' not found"),
				*FString(__FUNCTION__),
				*AttributeName.ToString());
			continue;
		}

		const UTcsAttributeDefinitionAsset* AttrDef = *AttrDefPtr;
		FTcsAttributeInstance AttrInst = FTcsAttributeInstance(AttrDef, AttributeName, ++GlobalAttributeInstanceIdMgr, CombatEntity);
		AttributeComponent->Attributes.Add(AttributeName, AttrInst);

		// Clamp initialization values to the configured range (static or dynamic).
		if (FTcsAttributeInstance* Added = AttributeComponent->Attributes.Find(AttributeName))
		{
			float Clamped = Added->BaseValue;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, Clamped);
			Added->BaseValue = Clamped;
			Added->CurrentValue = Clamped;
		}
	}
}
bool UTcsAttributeManagerSubsystem::AddAttributeByTag(
	AActor* CombatEntity,
	const FGameplayTag& AttributeTag,
	float InitValue)
{
	FName AttributeName;
	if (!TryResolveAttributeNameByTag(AttributeTag, AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Failed to resolve AttributeTag '%s' to AttributeName"),
			*FString(__FUNCTION__),
			*AttributeTag.ToString());
		return false;
	}

	// 检查属性是否已存在
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (IsValid(AttributeComponent) && AttributeComponent->Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Attribute '%s' already exists on CombatEntity '%s', skipping add"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return false;
	}

	AddAttribute(CombatEntity, AttributeName, InitValue);

	// 验证是否真的添加成功
	return IsValid(AttributeComponent) && AttributeComponent->Attributes.Contains(AttributeName);
}
bool UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag(
	const FGameplayTag& AttributeTag,
	FName& OutAttributeName) const
{
	if (!AttributeTag.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Invalid AttributeTag provided"),
			*FString(__FUNCTION__));
		return false;
	}

	const FName* FoundName = AttributeTagToName.Find(AttributeTag);
	if (FoundName)
	{
		OutAttributeName = *FoundName;
		return true;
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("[%s] AttributeTag '%s' not found in mapping. Make sure the tag is registered in AttributeDefTable."),
		*FString(__FUNCTION__),
		*AttributeTag.ToString());
	return false;
}
bool UTcsAttributeManagerSubsystem::TryGetAttributeTagByName(
	FName AttributeName,
	FGameplayTag& OutAttributeTag) const
{
	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Invalid AttributeName provided"),
			*FString(__FUNCTION__));
		return false;
	}

	const FGameplayTag* FoundTag = AttributeNameToTag.Find(AttributeName);
	if (FoundTag)
	{
		OutAttributeTag = *FoundTag;
		return true;
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("[%s] AttributeName '%s' not found in mapping or has no AttributeTag configured."),
		*FString(__FUNCTION__),
		*AttributeName.ToString());
	return false;
}

bool UTcsAttributeManagerSubsystem::SetAttributeBaseValue(
	AActor* CombatEntity,
	FName AttributeName,
	float NewValue,
	bool bTriggerEvents)
{
	// 参数验证
	if (!IsValid(CombatEntity))
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid CombatEntity"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!AttributeComponent)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Entity '%s' has no AttributeComponent"),
			*FString(__FUNCTION__),
			*CombatEntity->GetName());
		return false;
	}

	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid AttributeName"),
			*FString(__FUNCTION__));
		return false;
	}

	FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
	if (!Attribute)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Attribute '%s' not found on entity '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return false;
	}

	// 保存旧值
	float OldValue = Attribute->BaseValue;

	// 设置新值并clamp
	Attribute->BaseValue = NewValue;
	ClampAttributeValueInRange(AttributeComponent, AttributeName, Attribute->BaseValue);

	// 重新计算Current值（应用修改器）
	RecalculateAttributeCurrentValues(CombatEntity);

	// 触发事件
	if (bTriggerEvents && !FMath::IsNearlyEqual(OldValue, Attribute->BaseValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldValue;
		Payload.NewValue = Attribute->BaseValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	UE_LOG(LogTcsAttribute, Verbose,
		TEXT("[%s] Set attribute '%s' BaseValue from %.2f to %.2f on entity '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		OldValue,
		Attribute->BaseValue,
		*CombatEntity->GetName());

	return true;
}

bool UTcsAttributeManagerSubsystem::SetAttributeCurrentValue(
	AActor* CombatEntity,
	FName AttributeName,
	float NewValue,
	bool bTriggerEvents)
{
	// 参数验证
	if (!IsValid(CombatEntity))
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid CombatEntity"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!AttributeComponent)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Entity '%s' has no AttributeComponent"),
			*FString(__FUNCTION__),
			*CombatEntity->GetName());
		return false;
	}

	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid AttributeName"),
			*FString(__FUNCTION__));
		return false;
	}

	FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
	if (!Attribute)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Attribute '%s' not found on entity '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return false;
	}

	// 保存旧值
	float OldValue = Attribute->CurrentValue;

	// 设置新值并clamp
	Attribute->CurrentValue = NewValue;
	ClampAttributeValueInRange(AttributeComponent, AttributeName, Attribute->CurrentValue);

	// 触发事件
	if (bTriggerEvents && !FMath::IsNearlyEqual(OldValue, Attribute->CurrentValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldValue;
		Payload.NewValue = Attribute->CurrentValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		AttributeComponent->BroadcastAttributeValueChangeEvent(Payloads);
	}

	UE_LOG(LogTcsAttribute, Verbose,
		TEXT("[%s] Set attribute '%s' CurrentValue from %.2f to %.2f on entity '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		OldValue,
		Attribute->CurrentValue,
		*CombatEntity->GetName());

	return true;
}

bool UTcsAttributeManagerSubsystem::ResetAttribute(
	AActor* CombatEntity,
	FName AttributeName)
{
	// 参数验证
	if (!IsValid(CombatEntity))
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid CombatEntity"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!AttributeComponent)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Entity '%s' has no AttributeComponent"),
			*FString(__FUNCTION__),
			*CombatEntity->GetName());
		return false;
	}

	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid AttributeName"),
			*FString(__FUNCTION__));
		return false;
	}

	FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
	if (!Attribute)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Attribute '%s' not found on entity '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return false;
	}

	// 获取初始值（重置为0）
	float InitValue = 0.0f;

	// 移除所有应用到该属性的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (const FTcsAttributeModifierInstance& Modifier : AttributeComponent->AttributeModifiers)
	{
		// 检查修改器定义以获取 AttributeName
		if (!Modifier.ModifierDefAsset)
		{
			continue;
		}
		const UTcsAttributeModifierDefinitionAsset* ModDef = Modifier.ModifierDefAsset;
		if (ModDef && ModDef->AttributeName == AttributeName)
		{
			ModifiersToRemove.Add(Modifier);
		}
	}
	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(CombatEntity, ModifiersToRemove);
	}

	// 重置Base和Current值
	float OldBase = Attribute->BaseValue;
	float OldCurrent = Attribute->CurrentValue;
	Attribute->BaseValue = InitValue;
	Attribute->CurrentValue = InitValue;

	// Clamp值
	ClampAttributeValueInRange(AttributeComponent, AttributeName, Attribute->BaseValue);
	ClampAttributeValueInRange(AttributeComponent, AttributeName, Attribute->CurrentValue);

	// 触发事件
	if (!FMath::IsNearlyEqual(OldBase, Attribute->BaseValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldBase;
		Payload.NewValue = Attribute->BaseValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	if (!FMath::IsNearlyEqual(OldCurrent, Attribute->CurrentValue))
	{
		FTcsAttributeChangeEventPayload Payload;
		Payload.AttributeName = AttributeName;
		Payload.OldValue = OldCurrent;
		Payload.NewValue = Attribute->CurrentValue;

		TArray<FTcsAttributeChangeEventPayload> Payloads;
		Payloads.Add(Payload);
		AttributeComponent->BroadcastAttributeValueChangeEvent(Payloads);
	}

	UE_LOG(LogTcsAttribute, Log,
		TEXT("[%s] Reset attribute '%s' to initial value %.2f on entity '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		InitValue,
		*CombatEntity->GetName());

	return true;
}

bool UTcsAttributeManagerSubsystem::RemoveAttribute(
	AActor* CombatEntity,
	FName AttributeName)
{
	// 参数验证
	if (!IsValid(CombatEntity))
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid CombatEntity"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!AttributeComponent)
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Entity '%s' has no AttributeComponent"),
			*FString(__FUNCTION__),
			*CombatEntity->GetName());
		return false;
	}

	if (AttributeName.IsNone())
	{
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] Invalid AttributeName"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!AttributeComponent->Attributes.Contains(AttributeName))
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Attribute '%s' not found on entity '%s'"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*CombatEntity->GetName());
		return false;
	}

	// 移除所有应用到该属性的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (const FTcsAttributeModifierInstance& Modifier : AttributeComponent->AttributeModifiers)
	{
		// 检查修改器定义以获取 AttributeName
		if (!Modifier.ModifierDefAsset)
		{
			continue;
		}
		const UTcsAttributeModifierDefinitionAsset* ModDef = Modifier.ModifierDefAsset;
		if (ModDef && ModDef->AttributeName == AttributeName)
		{
			ModifiersToRemove.Add(Modifier);
		}
	}
	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(CombatEntity, ModifiersToRemove);
	}

	// 从组件中移除属性
	AttributeComponent->Attributes.Remove(AttributeName);

	UE_LOG(LogTcsAttribute, Log,
		TEXT("[%s] Removed attribute '%s' from entity '%s'"),
		*FString(__FUNCTION__),
		*AttributeName.ToString(),
		*CombatEntity->GetName());

	return true;
}

UTcsAttributeComponent* UTcsAttributeManagerSubsystem::GetAttributeComponent(const AActor* CombatEntity)
{
	if (!IsValid(CombatEntity))
	{
		return nullptr;
	}

	if (CombatEntity->Implements<UTcsEntityInterface>())
	{
		return ITcsEntityInterface::Execute_GetAttributeComponent(CombatEntity);
	}

	return CombatEntity->FindComponentByClass<UTcsAttributeComponent>();
}

bool UTcsAttributeManagerSubsystem::CreateAttributeModifier(
	FName ModifierId,
	FName SourceName,
	AActor* Instigator,
	AActor* Target,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	// 严格输入校验
	if (SourceName == NAME_None)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] SourceName cannot be NAME_None"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	// 验证 Instigator 和 Target 实现 ITcsEntityInterface
	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	if (!Target->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Target->GetName());
		return false;
	}

	const UTcsAttributeModifierDefinitionAsset* const* ModifierDefPtr = AttributeModifierDefinitions.Find(ModifierId);
	if (!ModifierDefPtr || !*ModifierDefPtr)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	const UTcsAttributeModifierDefinitionAsset* ModifierDefAsset = *ModifierDefPtr;
	OutModifierInst = FTcsAttributeModifierInstance();

	// 设置 DataAsset 引用和 ModifierId
	OutModifierInst.ModifierDefAsset = ModifierDefAsset;
	OutModifierInst.ModifierId = ModifierId;

	// 验证优先级
	if (ModifierDefAsset->Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, clamped to 0."),
			*FString(__FUNCTION__),
			*ModifierDefAsset->ModifierName.ToString(),
			ModifierDefAsset->Priority);
	}

	OutModifierInst.ModifierInstId = ++GlobalAttributeModifierInstanceIdMgr;
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = Target;
	OutModifierInst.Operands = ModifierDefAsset->Operands;
	OutModifierInst.SourceName = SourceName;

	return true;
}

bool UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands(
	FName ModifierId,
	FName SourceName,
	AActor* Instigator,
	AActor* Target,
	const TMap<FName, float>& Operands,
	FTcsAttributeModifierInstance& OutModifierInst)
{
	// 严格输入校验
	if (SourceName == NAME_None)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] SourceName cannot be NAME_None"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Instigator))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target is not valid"),
			*FString(__FUNCTION__));
		return false;
	}

	// 验证 Instigator 和 Target 实现 ITcsEntityInterface
	if (!Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Instigator '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Instigator->GetName());
		return false;
	}

	if (!Target->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Target '%s' does not implement ITcsEntityInterface"),
			*FString(__FUNCTION__),
			*Target->GetName());
		return false;
	}

	const UTcsAttributeModifierDefinitionAsset* const* ModifierDefPtr = AttributeModifierDefinitions.Find(ModifierId);
	if (!ModifierDefPtr || !*ModifierDefPtr)
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeModifierDefinition '%s' not found"),
			*FString(__FUNCTION__),
			*ModifierId.ToString());
		return false;
	}

	const UTcsAttributeModifierDefinitionAsset* ModifierDefAsset = *ModifierDefPtr;

	// 验证Operands是否正确
	if (ModifierDefAsset->Operands.IsEmpty())
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] ModifierDef does not contain Operands"),
			*FString(__FUNCTION__));
		return false;
	}
	for (const TPair<FName, float>& Operand : ModifierDefAsset->Operands)
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
	OutModifierInst.ModifierDefAsset = ModifierDefAsset;
	OutModifierInst.ModifierId = ModifierId;

	// 验证优先级
	if (ModifierDefAsset->Priority < 0)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttrModDef %s has invalid Priority %d, clamped to 0."),
			*FString(__FUNCTION__),
			*ModifierDefAsset->ModifierName.ToString(),
			ModifierDefAsset->Priority);
	}

	OutModifierInst.ModifierInstId = ++GlobalAttributeModifierInstanceIdMgr;
	OutModifierInst.Instigator = Instigator;
	OutModifierInst.Target = Target;
	OutModifierInst.Operands = Operands;
	OutModifierInst.SourceName = SourceName;

	return true;
}

void UTcsAttributeManagerSubsystem::ApplyModifier(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent) || Modifiers.IsEmpty())
	{
		return;
	}

	TArray<FTcsAttributeModifierInstance> ModifiersToExecute;// 对属性Base值执行操作的属性修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToApply;// 对属性Current值应用的属性修改器
	const int64 BatchId = ++GlobalAttributeModifierChangeBatchIdMgr;
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();

	// 区分好修改属性Base值和Current值的两种修改器
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		// 检查修改器定义以获取 ModifierMode
		if (!Modifier.ModifierDefAsset)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] ModifierDefAsset is null for ModifierId: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString());
			continue;
		}
		const UTcsAttributeModifierDefinitionAsset* ModDef = Modifier.ModifierDefAsset;

		switch (ModDef->ModifierMode)
		{
		case ETcsAttributeModifierMode::AMM_BaseValue:
			{
				// BaseValue modifiers are executed immediately (not persisted), but we still stamp them for
				// deterministic merging policies (e.g., UseNewest/UseOldest) and debugging.
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;

				// 添加到执行列表
				ModifiersToExecute.Add(Modifier);
				break;
			}
		case ETcsAttributeModifierMode::AMM_CurrentValue:
			{
				// 添加到应用列表并设置应用时间戳
				Modifier.ApplyTimestamp = UtcNowTicks;
				Modifier.UpdateTimestamp = UtcNowTicks;
				Modifier.LastTouchedBatchId = BatchId;
				ModifiersToApply.Add(Modifier);
				break;
			}
		}
	}

	// 先执行针对属性Base值的修改器
	if (!ModifiersToExecute.IsEmpty())
	{
		RecalculateAttributeBaseValues(CombatEntity, ModifiersToExecute);
	}
	
	// 再执行针对属性Current值的修改器
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
					if (const TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(SourceId))
					{
						for (int32 ModifierInstId : *InstIdsPtr)
						{
							// 通过 ModifierInstId 查找当前数组下标
							const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(ModifierInstId);
							if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
							{
								// 索引失效，跳过（稍后自愈）
								continue;
							}

							int32 Index = *IndexPtr;
							FTcsAttributeModifierInstance& Stored = AttributeComponent->AttributeModifiers[Index];

							// 验证 ModifierInstId 匹配（防御性检查）
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
							Stored.SourceName = Incoming.SourceName;
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
			int32 NewIndex = AttributeComponent->AttributeModifiers.Add(ModifierToStore);

			// 更新两个缓存: ModifierInstId -> Index 和 SourceId -> ModifierInstIds
			AttributeComponent->ModifierInstIdToIndex.Add(ModifierToStore.ModifierInstId, NewIndex);

			if (ModifierToStore.SourceHandle.IsValid())
			{
				TArray<int32>& InstIds = AttributeComponent->SourceHandleIdToModifierInstIds.FindOrAdd(ModifierToStore.SourceHandle.Id);
				InstIds.AddUnique(ModifierToStore.ModifierInstId);
			}
		}

		// 广播新增事件
		for (const FTcsAttributeModifierInstance& Updated : UpdatedExistingModifiers)
		{
			AttributeComponent->BroadcastAttributeModifierUpdatedEvent(Updated);
		}

		for (const FTcsAttributeModifierInstance& Added : NewlyAddedModifiers)
		{
			AttributeComponent->BroadcastAttributeModifierAddedEvent(Added);
		}
	}
	
	// 无论如何，都要重新计算属性Current值
	RecalculateAttributeCurrentValues(CombatEntity, BatchId);
}

void UTcsAttributeManagerSubsystem::RemoveModifier(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const int64 BatchId = ++GlobalAttributeModifierChangeBatchIdMgr;
	bool bModified = false;
	for (const FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		// 使用 ModifierInstId 定位元素
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 修改器不存在或索引失效
			continue;
		}

		int32 RemovedIndex = *IndexPtr;
		const FTcsAttributeModifierInstance& RemovedModifierRef = AttributeComponent->AttributeModifiers[RemovedIndex];

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
		AttributeComponent->ModifierInstIdToIndex.Remove(RemovedModifier.ModifierInstId);

		if (RemovedModifier.SourceHandle.IsValid())
		{
			TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(RemovedModifier.SourceHandle.Id);
			if (InstIdsPtr)
			{
				InstIdsPtr->Remove(RemovedModifier.ModifierInstId);
				if (InstIdsPtr->Num() == 0)
				{
					AttributeComponent->SourceHandleIdToModifierInstIds.Remove(RemovedModifier.SourceHandle.Id);
				}
			}
		}

		// 使用 RemoveAtSwap 删除元素（O(1) 操作）
		const int32 LastIndex = AttributeComponent->AttributeModifiers.Num() - 1;
		if (RemovedIndex != LastIndex)
		{
			// 有元素被 swap 过来，更新其索引
			const FTcsAttributeModifierInstance& SwappedModifier = AttributeComponent->AttributeModifiers[LastIndex];
			AttributeComponent->ModifierInstIdToIndex[SwappedModifier.ModifierInstId] = RemovedIndex;
		}

		AttributeComponent->AttributeModifiers.RemoveAtSwap(RemovedIndex);

		AttributeComponent->BroadcastAttributeModifierRemovedEvent(RemovedModifier);
		bModified = true;
	}

	// 如果确实有属性修改器被移除，则更新属性的当前值
	if (bModified)
	{
		RecalculateAttributeCurrentValues(CombatEntity, BatchId);
	}
}

void UTcsAttributeManagerSubsystem::HandleModifierUpdated(
	AActor* CombatEntity,
	TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	bool bModified = false;
	const int64 BatchId = ++GlobalAttributeModifierChangeBatchIdMgr;
	const int64 UtcNowTicks = FDateTime::UtcNow().GetTicks();
	for (FTcsAttributeModifierInstance& Modifier : Modifiers)
	{
		// 使用 ModifierInstId 定位元素
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(Modifier.ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 修改器不存在或索引失效
			continue;
		}

		const int32 ModifierIndex = *IndexPtr;
		const FTcsAttributeModifierInstance OldStored = AttributeComponent->AttributeModifiers[ModifierIndex];

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

		AttributeComponent->AttributeModifiers[ModifierIndex] = Modifier;

		// 更新 SourceHandle 缓存（如果 SourceHandle 发生变化）
		const int32 OldSourceId = OldStored.SourceHandle.IsValid() ? OldStored.SourceHandle.Id : -1;
		const int32 NewSourceId = Modifier.SourceHandle.IsValid() ? Modifier.SourceHandle.Id : -1;
		if (OldSourceId != NewSourceId)
		{
			// 从旧桶中移除
			if (OldSourceId >= 0)
			{
				if (TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(OldSourceId))
				{
					InstIdsPtr->Remove(Modifier.ModifierInstId);
					if (InstIdsPtr->IsEmpty())
					{
						AttributeComponent->SourceHandleIdToModifierInstIds.Remove(OldSourceId);
					}
				}
			}

			// 添加到新桶
			if (NewSourceId >= 0)
			{
				TArray<int32>& InstIds = AttributeComponent->SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
				InstIds.AddUnique(Modifier.ModifierInstId);
			}
		}
		else if (NewSourceId >= 0)
		{
			// SourceHandle 未变化：确保 InstId 存在（防御性）
			TArray<int32>& InstIds = AttributeComponent->SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId);
			InstIds.AddUnique(Modifier.ModifierInstId);
		}

		AttributeComponent->BroadcastAttributeModifierUpdatedEvent(Modifier);

		bModified = true;
	}

	if (bModified)
	{
		RecalculateAttributeCurrentValues(CombatEntity, BatchId);
	}
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeBaseValues(
	const AActor* CombatEntity,
	const TArray<FTcsAttributeModifierInstance>& Modifiers)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(CombatEntity, Modifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;

	// 执行对属性基础值的修改计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		// 检查修改器定义以获取 ModifierType
		if (!Modifier.ModifierDefAsset)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has null ModifierDefAsset. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}
		const UTcsAttributeModifierDefinitionAsset* ModDef = Modifier.ModifierDefAsset;
		if (!ModDef->ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
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
			if (NewValue != LastPair.Value)
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
		if (FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value, &RangeMin, &RangeMax);
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
				AttributeComponent->BroadcastAttributeReachedBoundaryEvent(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}

	// 属性基础值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(Payloads);
	}

	// 执行范围约束传播
	EnforceAttributeRangeConstraints(AttributeComponent);
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeCurrentValues(const AActor* CombatEntity, int64 ChangeBatchId)
{
	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	// 按类型整理所有属性修改器，方便后续执行修改器合并
	TArray<FTcsAttributeModifierInstance> MergedModifiers;
	MergeAttributeModifiers(CombatEntity, AttributeComponent->AttributeModifiers, MergedModifiers);
	// 按照优先级对属性修改器进行排序
	MergedModifiers.Sort();

	// 属性修改事件记录
	TMap<FName, FTcsAttributeChangeEventPayload> ChangeEventPayloads;

	// 获取属性基础值，用于计算
	TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
	// 重新计算属性当前新值前，保存属性旧值
	TMap<FName, float> OldCurrentValues = AttributeComponent->GetAttributeValues();
	// 用于更新计算的临时属性值容器，基于属性的基础值
	TMap<FName, float> CurrentValuesToCalc = BaseValues;

	// 执行属性修改器的修改计算
	for (const FTcsAttributeModifierInstance& Modifier : MergedModifiers)
	{
		// 检查修改器定义以获取 ModifierType
		if (!Modifier.ModifierDefAsset)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has null ModifierDefAsset. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
			continue;
		}
		const UTcsAttributeModifierDefinitionAsset* ModDef = Modifier.ModifierDefAsset;
		if (!ModDef->ModifierType)
		{
			UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] ModifierId %s has no valid AttributeModifierExecution type. Entity: %s"),
				*FString(__FUNCTION__),
				*Modifier.ModifierId.ToString(),
				CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
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
			if (NewValue != LastPair.Value && (ChangeBatchId < 0 || Modifier.LastTouchedBatchId == ChangeBatchId))
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
		if (FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
		{
			float RangeMin = Pair.Value;
			float RangeMax = Pair.Value;
			ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute->CurrentValue, Pair.Value))
			{
				continue;
			}

			const bool bReachedMin = FMath::IsNearlyEqual(Pair.Value, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(Pair.Value, RangeMax);

			// 记录属性修改事件的最终结果，因为修改器移除导致的属性当前值变更，不会有修改记录，所以需要在此处查漏补缺
			FTcsAttributeChangeEventPayload & Payload = ChangeEventPayloads.FindOrAdd(Pair.Key);
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
				AttributeComponent->BroadcastAttributeReachedBoundaryEvent(Pair.Key, bIsMaxBoundary, OldValue, Pair.Value, BoundaryValue);
			}
		}
	}
	
	// 属性当前值更新广播
	if (!ChangeEventPayloads.IsEmpty())
	{
		TArray<FTcsAttributeChangeEventPayload> Payloads;
		ChangeEventPayloads.GenerateValueArray(Payloads);
		AttributeComponent->BroadcastAttributeValueChangeEvent(Payloads);
	}

	// 执行范围约束传播
	EnforceAttributeRangeConstraints(AttributeComponent);
}

void UTcsAttributeManagerSubsystem::MergeAttributeModifiers(
	const AActor* CombatEntity,
	const TArray<FTcsAttributeModifierInstance>& Modifiers,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	// 按ModifierId整理所有属性修改器，方便后续执行修改器合并
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

		// 检查第一个修改器的定义以获取 MergerType
		if (!Pair.Value[0].ModifierDefAsset)
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ModifierDefAsset is null for ModifierId: %s"),
				*FString(__FUNCTION__),
				*Pair.Value[0].ModifierId.ToString());
			continue;
		}
		const UTcsAttributeModifierDefinitionAsset* ModDef = Pair.Value[0].ModifierDefAsset;

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

void UTcsAttributeManagerSubsystem::ClampAttributeValueInRange(
	UTcsAttributeComponent* AttributeComponent,
	const FName& AttributeName,
	float& NewValue,
	float* OutMinValue,
	float* OutMaxValue,
	const TMap<FName, float>* WorkingValues)
{
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
	if (!Attribute)
	{
		return;
	}
	const FTcsAttributeRange& Range = Attribute->AttributeDefAsset->AttributeRange;

	// 计算属性范围的最小值
	float MinValue = TNumericLimits<float>::Lowest();
	switch (Range.MinValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		{
			break;
		}
	case ETcsAttributeRangeType::ART_Static:
		{
			MinValue = Range.MinValue;
			break;
		}
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			// 优先从工作集读取（两段式 Clamp），否则从已提交值读取
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MinValueAttribute))
				{
					MinValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !AttributeComponent->GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
				// Keep default "no constraint" min value and continue clamping.
			}
			break;
		}
	}

	// 计算属性范围的最大值
	float MaxValue = TNumericLimits<float>::Max();
	switch (Range.MaxValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		{
			break;
		}
	case ETcsAttributeRangeType::ART_Static:
		{
			MaxValue = Range.MaxValue;
			break;
		}
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			// 优先从工作集读取（两段式 Clamp），否则从已提交值读取
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MaxValueAttribute))
				{
					MaxValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !AttributeComponent->GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					*AttributeComponent->GetOwner()->GetName(),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
				// Keep default "no constraint" max value and continue clamping.
			}
			break;
		}
	}

	// 执行属性值 Clamp（使用策略对象）
	TSubclassOf<UTcsAttributeClampStrategy> StrategyClass = Attribute->AttributeDefAsset->ClampStrategyClass;
	if (StrategyClass)
	{
		// 使用策略对象执行 Clamp
		UTcsAttributeClampStrategy* StrategyCDO = StrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();

		// 构造固定的基础上下文
		FTcsAttributeClampContextBase Context(
			AttributeComponent,
			AttributeName,
			Attribute->AttributeDefAsset,
			Attribute,
			WorkingValues  // 传递工作集，用于读取其他属性的临时值
		);

		// 获取可选的用户配置
		const FInstancedStruct& Config = Attribute->AttributeDefAsset->ClampStrategyConfig;

		// 调用 Clamp 接口
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
		// 防御性代码：理论上不应该走到这里，因为构造函数设置了默认值
		// 但为了安全，仍然提供 fallback
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

void UTcsAttributeManagerSubsystem::EnforceAttributeRangeConstraints(UTcsAttributeComponent* AttributeComponent)
{
	if (!IsValid(AttributeComponent))
	{
		return;
	}

	const int32 MaxIterations = 8;  // 防止无限循环
	int32 Iteration = 0;
	bool bAnyChanged = true;

	// 工作集: 当前正在处理的值
	TMap<FName, float> WorkingBaseValues;
	TMap<FName, float> WorkingCurrentValues;

	// 初始化工作集
	for (auto& Pair : AttributeComponent->Attributes)
	{
		WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
		WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
	}

	// 迭代直到稳定
	while (bAnyChanged && Iteration < MaxIterations)
	{
		bAnyChanged = false;
		Iteration++;

		for (auto& Pair : AttributeComponent->Attributes)
		{
			FName AttributeName = Pair.Key;

			// 阶段1: Clamp BaseValue，使用 WorkingBaseValues 解析动态范围
			float OldBase = WorkingBaseValues[AttributeName];
			float NewBase = OldBase;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
			if (!FMath::IsNearlyEqual(OldBase, NewBase))
			{
				WorkingBaseValues[AttributeName] = NewBase;
				bAnyChanged = true;
			}

			// 阶段2: Clamp CurrentValue，使用 WorkingCurrentValues 解析动态范围
			float OldCurrent = WorkingCurrentValues[AttributeName];
			float NewCurrent = OldCurrent;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
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
			*AttributeComponent->GetOwner()->GetName());
	}

	// 提交工作集到组件
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;

	for (auto& Pair : AttributeComponent->Attributes)
	{
		FName AttributeName = Pair.Key;
		FTcsAttributeInstance& Attribute = Pair.Value;

		// 提交 BaseValue
		float NewBase = WorkingBaseValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.BaseValue, NewBase))
		{
			float OldBase = Attribute.BaseValue;
			Attribute.BaseValue = NewBase;

			// 构造事件 payload
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldBase;
			Payload.NewValue = NewBase;
			// SourceHandle: 空(因为是 enforcement 导致的变化,不是 modifier)
			BaseChangePayloads.Add(Payload);

			// 检测是否达到边界
			float RangeMin = NewBase;
			float RangeMax = NewBase;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, NewBase, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewBase, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewBase, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				AttributeComponent->BroadcastAttributeReachedBoundaryEvent(AttributeName, bIsMaxBoundary, OldBase, NewBase, BoundaryValue);
			}
		}

		// 提交 CurrentValue
		float NewCurrent = WorkingCurrentValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.CurrentValue, NewCurrent))
		{
			float OldCurrent = Attribute.CurrentValue;
			Attribute.CurrentValue = NewCurrent;

			// 构造事件 payload
			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldCurrent;
			Payload.NewValue = NewCurrent;
			CurrentChangePayloads.Add(Payload);

			// 检测是否达到边界
			float RangeMin = NewCurrent;
			float RangeMax = NewCurrent;
			ClampAttributeValueInRange(AttributeComponent, AttributeName, NewCurrent, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewCurrent, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewCurrent, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				AttributeComponent->BroadcastAttributeReachedBoundaryEvent(AttributeName, bIsMaxBoundary, OldCurrent, NewCurrent, BoundaryValue);
			}
		}
	}

	// 广播事件
	if (BaseChangePayloads.Num() > 0)
	{
		AttributeComponent->BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}
	if (CurrentChangePayloads.Num() > 0)
	{
		AttributeComponent->BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}
}

FTcsSourceHandle UTcsAttributeManagerSubsystem::CreateSourceHandle(
	const FDataTableRowHandle& SourceDefinition,
	FName SourceName,
	const FGameplayTagContainer& SourceTags,
	AActor* Instigator)
{
	// 生成全局唯一ID
	++GlobalSourceHandleIdMgr;

	// 创建并返回SourceHandle
	return FTcsSourceHandle(GlobalSourceHandleIdMgr, SourceDefinition, SourceName, SourceTags, Instigator);
}

FTcsSourceHandle UTcsAttributeManagerSubsystem::CreateSourceHandleSimple(
	FName SourceName,
	AActor* Instigator)
{
	// 生成全局唯一ID
	++GlobalSourceHandleIdMgr;

	// 创建并返回简化的SourceHandle
	return FTcsSourceHandle(GlobalSourceHandleIdMgr, SourceName, Instigator);
}

bool UTcsAttributeManagerSubsystem::ApplyModifierWithSourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle,
	const TArray<FName>& ModifierIds,
	TArray<FTcsAttributeModifierInstance>& OutModifiers)
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return false;
	}

	OutModifiers.Empty();

	// 为每个ModifierId创建修改器实例
	for (const FName& ModifierId : ModifierIds)
	{
		FTcsAttributeModifierInstance ModifierInst;
		if (CreateAttributeModifier(ModifierId, SourceHandle.SourceName, SourceHandle.Instigator.Get(), CombatEntity, ModifierInst))
		{
			// 设置SourceHandle
			ModifierInst.SourceHandle = SourceHandle;
			// 保持SourceName同步
			ModifierInst.SourceName = SourceHandle.SourceName;

			OutModifiers.Add(ModifierInst);
		}
	}

	if (OutModifiers.Num() > 0)
	{
		ApplyModifier(CombatEntity, OutModifiers);
		return true;
	}

	return false;
}

bool UTcsAttributeManagerSubsystem::RemoveModifiersBySourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle)
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return false;
	}

	// 使用稳定 ID 缓存查找匹配的修改器
	const TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	// 先拷贝 ID 列表（避免在迭代中修改）
	TArray<int32> InstIdsCopy = *InstIdsPtr;

	// 收集要移除的修改器
	TArray<FTcsAttributeModifierInstance> ModifiersToRemove;
	for (int32 ModifierInstId : InstIdsCopy)
	{
		// 通过 ModifierInstId 查找当前数组下标
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 索引失效，跳过
			continue;
		}

		int32 Index = *IndexPtr;
		const FTcsAttributeModifierInstance& Modifier = AttributeComponent->AttributeModifiers[Index];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (Modifier.ModifierInstId == ModifierInstId)
		{
			ModifiersToRemove.Add(Modifier);
		}
	}

	if (ModifiersToRemove.Num() > 0)
	{
		RemoveModifier(CombatEntity, ModifiersToRemove);
		return true;
	}

	return false;
}

bool UTcsAttributeManagerSubsystem::GetModifiersBySourceHandle(
	AActor* CombatEntity,
	const FTcsSourceHandle& SourceHandle,
	TArray<FTcsAttributeModifierInstance>& OutModifiers) const
{
	if (!SourceHandle.IsValid())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] SourceHandle is invalid"),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
	if (!IsValid(AttributeComponent))
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"),
			*FString(__FUNCTION__));
		return false;
	}

	OutModifiers.Empty();

	// 使用稳定 ID 缓存查找匹配的修改器
	TArray<int32>* InstIdsPtr = AttributeComponent->SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!InstIdsPtr || InstIdsPtr->Num() == 0)
	{
		return false;
	}

	// 收集匹配的修改器，同时自愈陈旧的 ID
	TArray<int32> StaleIds;
	for (int32 ModifierInstId : *InstIdsPtr)
	{
		// 通过 ModifierInstId 查找当前数组下标
		const int32* IndexPtr = AttributeComponent->ModifierInstIdToIndex.Find(ModifierInstId);
		if (!IndexPtr || !AttributeComponent->AttributeModifiers.IsValidIndex(*IndexPtr))
		{
			// 索引失效，标记为陈旧
			StaleIds.Add(ModifierInstId);
			continue;
		}

		int32 Index = *IndexPtr;
		const FTcsAttributeModifierInstance& Modifier = AttributeComponent->AttributeModifiers[Index];

		// 验证 ModifierInstId 匹配（防御性检查）
		if (Modifier.ModifierInstId != ModifierInstId)
		{
			// 不匹配，标记为陈旧
			StaleIds.Add(ModifierInstId);
			continue;
		}

		OutModifiers.Add(Modifier);
	}

	// 自愈：从桶中移除陈旧的 ID
	if (StaleIds.Num() > 0)
	{
		for (int32 StaleId : StaleIds)
		{
			InstIdsPtr->Remove(StaleId);
		}

		// 如果桶为空，移除整个桶
		if (InstIdsPtr->Num() == 0)
		{
			AttributeComponent->SourceHandleIdToModifierInstIds.Remove(SourceHandle.Id);
		}

		UE_LOG(LogTcsAttribute, Verbose,
			TEXT("[%s] Self-healed %d stale ModifierInstIds for SourceHandle.Id=%d"),
			*FString(__FUNCTION__),
			StaleIds.Num(),
			SourceHandle.Id);
	}

	return OutModifiers.Num() > 0;
}

void UTcsAttributeManagerSubsystem::LoadFromAssetManager()
{
#if !WITH_EDITOR
	UAssetManager& AssetManager = UAssetManager::Get();

	// 加载属性定义
	AttributeDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> AttributeDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsAttributeDefinitionAsset::PrimaryAssetType, AttributeDefIds);

		for (const FPrimaryAssetId& AssetId : AttributeDefIds)
		{
			const UTcsAttributeDefinitionAsset* Asset = Cast<UTcsAttributeDefinitionAsset>(
				AssetManager.LoadPrimaryAsset(AssetId));

			if (Asset)
			{
				AttributeDefinitions.Add(Asset->AttributeDefId, Asset);
			}
			else
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeDefinition: %s"),
					*FString(__FUNCTION__),
					*AssetId.ToString());
			}
		}

		UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Loaded %d AttributeDefinitions from AssetManager"),
			*FString(__FUNCTION__),
			AttributeDefinitions.Num());
	}

	// 加载属性修改器定义
	AttributeModifierDefinitions.Empty();
	{
		TArray<FPrimaryAssetId> ModifierDefIds;
		AssetManager.GetPrimaryAssetIdList(UTcsAttributeModifierDefinitionAsset::PrimaryAssetType, ModifierDefIds);

		for (const FPrimaryAssetId& AssetId : ModifierDefIds)
		{
			const UTcsAttributeModifierDefinitionAsset* Asset = Cast<UTcsAttributeModifierDefinitionAsset>(
				AssetManager.LoadPrimaryAsset(AssetId));

			if (Asset)
			{
				AttributeModifierDefinitions.Add(Asset->AttributeModifierDefId, Asset);
			}
			else
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Failed to load AttributeModifierDefinition: %s"),
					*FString(__FUNCTION__),
					*AssetId.ToString());
			}
		}

		UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Loaded %d AttributeModifierDefinitions from AssetManager"),
			*FString(__FUNCTION__),
			AttributeModifierDefinitions.Num());
	}

	// 构建 AttributeTag -> AttributeName 映射
	AttributeTagToName.Empty();
	AttributeNameToTag.Empty();

	for (const auto& Pair : AttributeDefinitions)
	{
		const FName& AttributeName = Pair.Key;
		const UTcsAttributeDefinitionAsset* AttrDef = Pair.Value;

		if (!AttrDef || !AttrDef->AttributeTag.IsValid())
		{
			continue;
		}

		// 检查重复 Tag
		if (AttributeTagToName.Contains(AttrDef->AttributeTag))
		{
			const FName ExistingName = AttributeTagToName[AttrDef->AttributeTag];
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Duplicate AttributeTag '%s' found: already mapped to '%s', ignoring mapping for '%s'"),
				*FString(__FUNCTION__),
				*AttrDef->AttributeTag.ToString(),
				*ExistingName.ToString(),
				*AttributeName.ToString());
			continue;
		}

		AttributeTagToName.Add(AttrDef->AttributeTag, AttributeName);
		AttributeNameToTag.Add(AttributeName, AttrDef->AttributeTag);
	}

	UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Built %d AttributeTag mappings"),
		*FString(__FUNCTION__),
		AttributeTagToName.Num());
#endif
}
