// Copyright Tirefly. All Rights Reserved.


#include "State/StateCondition/TireflyStateCondition_AttributeComparison.h"
#include "State/TireflyState.h"
#include "Attribute/TireflyAttributeComponent.h"
#include "TireflyCombatEntityInterface.h"
#include "TireflyCombatSystemLogChannels.h"


bool UTireflyStateCondition_AttributeComparison::CheckCondition_Implementation(
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float CurrentGameTime)
{
	const FTireflyStateConditionPayload_AttributeComparison* Config = Payload.GetPtr<FTireflyStateConditionPayload_AttributeComparison>();
	if (!Config || !StateInstance)
	{
		UE_LOG(LogTcsStateCondition, Warning, TEXT("[%s] Config or StateInstance is null."), *FString(__FUNCTION__));
		return false;
	}

	// 根据检查目标获取对应的战斗实体
	const ITireflyCombatEntityInterface* CombatEntity = nullptr;
	
	switch (Config->CheckTarget)
	{
	case ETireflyAttributeCheckTarget::Owner:
		{
			if (AActor* Owner = StateInstance->GetOwner())
			{
				CombatEntity = Cast<ITireflyCombatEntityInterface>(Owner);
			}
			break;
		}
	case ETireflyAttributeCheckTarget::Instigator:
		{
			if (AActor* Instigator = StateInstance->GetInstigator())
			{
				CombatEntity = Cast<ITireflyCombatEntityInterface>(Instigator);
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
	const UTireflyAttributeComponent* AttributeComponent = CombatEntity->GetAttributeComponent();
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
	case ETireflyNumericComparison::GreaterThan:
		return AttributeValue > CompareValue;
	case ETireflyNumericComparison::GreaterThanOrEqual:
		return AttributeValue >= CompareValue;
	case ETireflyNumericComparison::LessThan:
		return AttributeValue < CompareValue;
	case ETireflyNumericComparison::LessThanOrEqual:
		return AttributeValue <= CompareValue;
	case ETireflyNumericComparison::Equal:
		return FMath::IsNearlyEqual(AttributeValue, CompareValue);
	case ETireflyNumericComparison::NotEqual:
		return !FMath::IsNearlyEqual(AttributeValue, CompareValue);
	default:
		return false;
	}
}
