// Copyright Tirefly. All Rights Reserved.


#include "State/StateCondition/TcsStateCondition_AttributeComparison.h"
#include "State/TcsState.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "TcsEntityInterface.h"
#include "TcsLogChannels.h"


bool UTcsStateCondition_AttributeComparison::CheckCondition_Implementation(
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float CurrentGameTime)
{
	const FTcsStateConditionPayload_AttributeComparison* Config = Payload.GetPtr<FTcsStateConditionPayload_AttributeComparison>();
	if (!Config || !StateInstance)
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Config or StateInstance is null."), *FString(__FUNCTION__));
		return false;
	}

	// 获取属性组件（使用 Execute_ 形式确保蓝图实现也能正确调用）
	const UTcsAttributeComponent* AttributeComponent = nullptr;
	switch (Config->CheckTarget)
	{
	case ETcsAttributeCheckTarget::Owner:
		if (AActor* Owner = StateInstance->GetOwner())
		{
			if (Owner->Implements<UTcsEntityInterface>())
			{
				AttributeComponent = ITcsEntityInterface::Execute_GetAttributeComponent(Owner);
			}
		}
		break;
	case ETcsAttributeCheckTarget::Instigator:
		if (AActor* Instigator = StateInstance->GetInstigator())
		{
			if (Instigator->Implements<UTcsEntityInterface>())
			{
				AttributeComponent = ITcsEntityInterface::Execute_GetAttributeComponent(Instigator);
			}
		}
		break;
	}
	if (!AttributeComponent)
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] CombatEntity to compare AttributeValue is null or does not implement ITcsEntityInterface."), *FString(__FUNCTION__));
		return false;
	}

	// 解析属性名称：优先使用 AttributeTag，如果无效则使用 AttributeName
	FName ResolvedAttributeName = Config->AttributeName;

	if (Config->AttributeTag.IsValid())
	{
		// 尝试通过 Tag 解析属性名称
		if (UWorld* World = StateInstance->GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UTcsAttributeManagerSubsystem* AttrMgr = GameInstance->GetSubsystem<UTcsAttributeManagerSubsystem>())
				{
					FName TagResolvedName;
					if (AttrMgr->TryResolveAttributeNameByTag(Config->AttributeTag, TagResolvedName))
					{
						ResolvedAttributeName = TagResolvedName;
					}
					else
					{
						UE_LOG(LogTcsStateCondition, Warning,
							TEXT("[%s] Failed to resolve AttributeTag '%s', falling back to AttributeName '%s'."),
							*FString(__FUNCTION__),
							*Config->AttributeTag.ToString(),
							*Config->AttributeName.ToString());
					}
				}
			}
		}
	}

	// 获取目标属性
	float AttributeValue = 0.f;
	if (!AttributeComponent->GetAttributeValue(ResolvedAttributeName, AttributeValue))
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Can't get CombatEntity's AttributeValue '%s'."),
			*FString(__FUNCTION__),
			*ResolvedAttributeName.ToString());
		return false;
	}

	// 执行比较
	float CompareValue = Config->CompareValue;
	switch (Config->ComparisonType)
	{
	case ETcsNumericComparison::GreaterThan:
		return AttributeValue > CompareValue;
	case ETcsNumericComparison::GreaterThanOrEqual:
		return AttributeValue >= CompareValue;
	case ETcsNumericComparison::LessThan:
		return AttributeValue < CompareValue;
	case ETcsNumericComparison::LessThanOrEqual:
		return AttributeValue <= CompareValue;
	case ETcsNumericComparison::Equal:
		return FMath::IsNearlyEqual(AttributeValue, CompareValue);
	case ETcsNumericComparison::NotEqual:
		return !FMath::IsNearlyEqual(AttributeValue, CompareValue);
	default:
		return false;
	}
}
