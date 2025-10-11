// Copyright Tirefly. All Rights Reserved.


#include "State/StateCondition/TcsStateCondition_AttributeComparison.h"
#include "State/TcsState.h"
#include "Attribute/TcsAttributeComponent.h"
#include "TcsCombatEntityInterface.h"
#include "TcsCombatSystemLogChannels.h"


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

	// 根据检查目标获取对应的战斗实体
	const ITcsCombatEntityInterface* CombatEntity = nullptr;
	
	switch (Config->CheckTarget)
	{
	case ETcsAttributeCheckTarget::Owner:
		{
			if (AActor* Owner = StateInstance->GetOwner())
			{
				CombatEntity = Cast<ITcsCombatEntityInterface>(Owner);
			}
			break;
		}
	case ETcsAttributeCheckTarget::Instigator:
		{
			if (AActor* Instigator = StateInstance->GetInstigator())
			{
				CombatEntity = Cast<ITcsCombatEntityInterface>(Instigator);
			}
			break;
		}
	}

	if (!CombatEntity)
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] CombatEntity to compare AttributeValue is null."), *FString(__FUNCTION__));
		return false;
	}

	// 获取属性组件
	const UTcsAttributeComponent* AttributeComponent = CombatEntity->GetAttributeComponent();
	if (!AttributeComponent)
	{
		return false;
	}

	// 获取目标属性
	float AttributeValue = 0.f;
	if (!AttributeComponent->GetAttributeValue(Config->AttributeName, AttributeValue))
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Can't get CombatEntity 's AttributeValue %s."),
			*FString(__FUNCTION__),
			*Config->AttributeName.ToString());
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
