// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TireflyCombatStateTreeSchema.h"
#include "State/TireflyState.h"
#include "State/TireflyStateComponent.h"
#include "Attribute/TireflyAttributeComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/AIController.h"

UTireflyCombatStateTreeSchema::UTireflyCombatStateTreeSchema()
{
	// 设置Schema的基本信息
	DisplayName = NSLOCTEXT("TireflyCombatSystem", "CombatStateTreeSchemaDisplayName", "Tirefly Combat StateTree");
	
	// 定义Schema保证的上下文数据
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Owner"),
		AActor::StaticClass(),
		EStateTreeExternalDataRequirement::Required
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Instigator"), 
		AActor::StaticClass(),
		EStateTreeExternalDataRequirement::Optional
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("StateInstance"),
		UTireflyStateInstance::StaticClass(),
		EStateTreeExternalDataRequirement::Required
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("StateComponent"),
		UTireflyStateComponent::StaticClass(),
		EStateTreeExternalDataRequirement::Required
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("AttributeComponent"),
		UTireflyAttributeComponent::StaticClass(), 
		EStateTreeExternalDataRequirement::Optional
	));
	
	// 可选的Pawn和Controller上下文
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Pawn"),
		APawn::StaticClass(),
		EStateTreeExternalDataRequirement::Optional
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Controller"),
		AController::StaticClass(),
		EStateTreeExternalDataRequirement::Optional
	));
}

TConstArrayView<FStateTreeExternalDataDesc> UTireflyCombatStateTreeSchema::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UTireflyCombatStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	// 允许所有StateTree节点基类和TCS专用节点
	return InScriptStruct &&
		(InScriptStruct->IsChildOf(FStateTreeNodeBase::StaticStruct()) ||
		 InScriptStruct->GetName().Contains(TEXT("TireflyCombat")) ||
		 Super::IsStructAllowed(InScriptStruct));
}

bool UTireflyCombatStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	// 允许TCS相关的类
	return InClass &&
		(InClass->GetName().Contains(TEXT("Tirefly")) ||
		 Super::IsClassAllowed(InClass));
}

bool UTireflyCombatStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// 允许Actor、组件和TCS相关类作为外部数据
	return InStruct.IsChildOf(AActor::StaticClass()) ||
		   InStruct.IsChildOf(UActorComponent::StaticClass()) ||
		   InStruct.GetName().Contains(TEXT("Tirefly")) ||
		   Super::IsExternalItemAllowed(InStruct);
}