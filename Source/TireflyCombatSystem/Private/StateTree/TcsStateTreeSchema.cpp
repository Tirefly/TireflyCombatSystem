// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsStateTreeSchema.h"

#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Skill/TcsSkillComponent.h"


UTcsStateTreeSchema::UTcsStateTreeSchema()
{
	// 定义Schema保证的上下文数据
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Owner"),
		AActor::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Instigator"), 
		AActor::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("StateInstance"),
		UTcsStateInstance::StaticClass(),
		FGuid::NewGuid()
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("OwnerStateCmp"),
		UTcsStateComponent::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("OwnerAttributeCmp"),
		UTcsAttributeComponent::StaticClass(), 
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("OwnerSkillCmp"),
		UTcsSkillComponent::StaticClass(), 
		FGuid::NewGuid()
	));

	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("InstigatorStateCmp"),
		UTcsStateComponent::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("InstigatorAttributeCmp"),
		UTcsAttributeComponent::StaticClass(), 
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("InstigatorSkillCmp"),
		UTcsSkillComponent::StaticClass(), 
		FGuid::NewGuid()
	));
}

TConstArrayView<FStateTreeExternalDataDesc> UTcsStateTreeSchema::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UTcsStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UTcsStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return InClass && InClass->IsChildOf<UStateTreeNodeBlueprintBase>();
}

bool UTcsStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// 允许Actor、组件和TCS相关类作为外部数据
	return InStruct.IsChildOf(AActor::StaticClass())
		|| InStruct.IsChildOf(UActorComponent::StaticClass())
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass())
		|| InStruct.IsChildOf(UGameInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsStateInstance::StaticClass());
}