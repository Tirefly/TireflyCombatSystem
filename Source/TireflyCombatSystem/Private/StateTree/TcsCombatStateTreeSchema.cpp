// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsCombatStateTreeSchema.h"
#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "Attribute/TcsAttributeComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UTcsCombatStateTreeSchema::UTcsCombatStateTreeSchema()
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
		FName("StateComponent"),
		UTcsStateComponent::StaticClass(),
		FGuid::NewGuid()
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("AttributeComponent"),
		UTcsAttributeComponent::StaticClass(), 
		FGuid::NewGuid()
	));
	
	// 可选的Pawn和Controller上下文
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Pawn"),
		APawn::StaticClass(),
		FGuid::NewGuid()
	));
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		FName("Controller"),
		AController::StaticClass(),
		FGuid::NewGuid()
	));
}

TConstArrayView<FStateTreeExternalDataDesc> UTcsCombatStateTreeSchema::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UTcsCombatStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	// 允许所有StateTree节点基类和TCS专用节点
	return InScriptStruct &&
		(InScriptStruct->IsChildOf(FStateTreeNodeBase::StaticStruct()) ||
		 InScriptStruct->GetName().Contains(TEXT("TcsCombat")) ||
		 Super::IsStructAllowed(InScriptStruct));
}

bool UTcsCombatStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	// 允许TCS相关的类
	return InClass &&
		(InClass->GetName().Contains(TEXT("Tirefly")) ||
		 Super::IsClassAllowed(InClass));
}

bool UTcsCombatStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// 允许Actor、组件和TCS相关类作为外部数据
	return InStruct.IsChildOf(AActor::StaticClass()) ||
		   InStruct.IsChildOf(UActorComponent::StaticClass()) ||
		   InStruct.GetName().Contains(TEXT("Tirefly")) ||
		   Super::IsExternalItemAllowed(InStruct);
}