// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TireflyCombatStateTreeTasks.h"
#include "State/TireflyState.h"
#include "State/TireflyStateComponent.h"
#include "State/TireflyStateManagerSubsystem.h"
#include "Attribute/TireflyAttributeComponent.h"
#include "StateTreeExecutionContext.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

//////////////////////////////////////////////////////////////////////////
// FTireflyStateSlot_Exclusive Implementation
//////////////////////////////////////////////////////////////////////////

FTireflyStateSlot_Exclusive::FTireflyStateSlot_Exclusive()
{
	// 配置任务特性
	bShouldCallTick = true;
	bShouldAffectTransitions = false;
	bConsideredForScheduling = true;
}

EStateTreeRunStatus FTireflyStateSlot_Exclusive::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTireflyStateSlot_ExclusiveInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Exclusive slot entered: %s (Priority: %d)"), 
		*InstanceData.SlotTag.ToString(), InstanceData.Priority);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTireflyStateSlot_Exclusive::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	// 互斥槽通常持续运行，等待状态变化
	return EStateTreeRunStatus::Running;
}

void FTireflyStateSlot_Exclusive::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTireflyStateSlot_ExclusiveInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Exclusive slot exited: %s"), *InstanceData.SlotTag.ToString());
}

#if WITH_EDITOR
FText FTireflyStateSlot_Exclusive::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyStateSlot_ExclusiveInstanceData* Data = InstanceDataView.GetPtr<FTireflyStateSlot_ExclusiveInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Exclusive Slot:</b> %s (Priority: %d)"),
			*Data->SlotTag.ToString(), Data->Priority));
	}

	return FText::FromString(FString::Printf(TEXT("Exclusive Slot: %s (Priority: %d)"),
		*Data->SlotTag.ToString(), Data->Priority));
}
#endif

//////////////////////////////////////////////////////////////////////////
// FTireflyStateSlot_Parallel Implementation
//////////////////////////////////////////////////////////////////////////

FTireflyStateSlot_Parallel::FTireflyStateSlot_Parallel()
{
	// 配置任务特性
	bShouldCallTick = true;
	bShouldAffectTransitions = false;
	bConsideredForScheduling = true;
}

EStateTreeRunStatus FTireflyStateSlot_Parallel::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTireflyStateSlot_ParallelInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Parallel slot entered: %s (MaxStack: %d)"), 
		*InstanceData.SlotTag.ToString(), InstanceData.MaxStackCount);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTireflyStateSlot_Parallel::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	// 并行槽通常持续运行，支持多个状态同时存在
	return EStateTreeRunStatus::Running;
}

void FTireflyStateSlot_Parallel::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTireflyStateSlot_ParallelInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Parallel slot exited: %s"), *InstanceData.SlotTag.ToString());
}

#if WITH_EDITOR
FText FTireflyStateSlot_Parallel::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyStateSlot_ParallelInstanceData* Data = InstanceDataView.GetPtr<FTireflyStateSlot_ParallelInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString StackText = Data->MaxStackCount == -1 ? TEXT("Unlimited") : FString::Printf(TEXT("%d"), Data->MaxStackCount);

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Parallel Slot:</b> %s (MaxStack: %s)"),
			*Data->SlotTag.ToString(), *StackText));
	}

	return FText::FromString(FString::Printf(TEXT("Parallel Slot: %s (MaxStack: %s)"),
		*Data->SlotTag.ToString(), *StackText));
}
#endif

//////////////////////////////////////////////////////////////////////////
// FTireflyStateCondition_SlotAvailable Implementation
//////////////////////////////////////////////////////////////////////////

bool FTireflyStateCondition_SlotAvailable::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FTireflyStateCondition_SlotAvailableInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// 获取StateComponent
	UTireflyStateComponent* StateComponent = Context.GetExternalData<UTireflyStateComponent>();
	if (!StateComponent)
	{
		return false;
	}

	// 检查槽位是否被占用
	bool bSlotOccupied = StateComponent->IsSlotOccupied(InstanceData.TargetSlot);
	
	// 如果槽位未被占用，则可以应用状态
	// 如果槽位已被占用，需要检查优先级等其他条件
	// 这里简化处理，实际实现可能需要更复杂的逻辑
	return !bSlotOccupied;
}

#if WITH_EDITOR
FText FTireflyStateCondition_SlotAvailable::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyStateCondition_SlotAvailableInstanceData* Data = InstanceDataView.GetPtr<FTireflyStateCondition_SlotAvailableInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Slot Available:</b> %s for State [%s]"),
			*Data->TargetSlot.ToString(), *Data->StateDefId.ToString()));
	}

	return FText::FromString(FString::Printf(TEXT("Slot Available: %s for [%s]"),
		*Data->TargetSlot.ToString(), *Data->StateDefId.ToString()));
}
#endif

//////////////////////////////////////////////////////////////////////////
// FTireflyCombatTask_ApplyState Implementation
//////////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FTireflyCombatTask_ApplyState::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTireflyCombatTask_ApplyStateInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// 获取StateManagerSubsystem
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UTireflyStateManagerSubsystem* StateManager = World->GetSubsystem<UTireflyStateManagerSubsystem>();
	if (!StateManager)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 确定目标和来源Actor
	AActor* TargetActor = InstanceData.StateTarget;
	AActor* SourceActor = InstanceData.StateSource;

	// 如果没有明确指定，使用上下文中的Actor
	if (!TargetActor)
	{
		TargetActor = Context.GetExternalData<AActor>("Owner");
	}

	if (!SourceActor)
	{
		SourceActor = Context.GetExternalData<AActor>("Instigator");
		if (!SourceActor)
		{
			SourceActor = TargetActor; // 如果没有Instigator，使用Owner作为来源
		}
	}

	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to apply state: No target actor"));
		return EStateTreeRunStatus::Failed;
	}

	// 应用状态
	bool bSuccess = StateManager->ApplyState(TargetActor, InstanceData.StateDefId, SourceActor, InstanceData.StateParameters);
	
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully applied state: %s to %s"), 
			*InstanceData.StateDefId.ToString(), *TargetActor->GetName());
		return EStateTreeRunStatus::Succeeded;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to apply state: %s to %s"), 
			*InstanceData.StateDefId.ToString(), *TargetActor->GetName());
		return EStateTreeRunStatus::Failed;
	}
}

#if WITH_EDITOR
FText FTireflyCombatTask_ApplyState::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyCombatTask_ApplyStateInstanceData* Data = InstanceDataView.GetPtr<FTireflyCombatTask_ApplyStateInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString TargetName = Data->StateTarget ? Data->StateTarget->GetName() : TEXT("Context Owner");

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Apply State:</b> [%s] to %s"),
			*Data->StateDefId.ToString(), *TargetName));
	}

	return FText::FromString(FString::Printf(TEXT("Apply State: [%s] to %s"),
		*Data->StateDefId.ToString(), *TargetName));
}
#endif

//////////////////////////////////////////////////////////////////////////
// FTireflyCombatTask_ModifyAttribute Implementation
//////////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FTireflyCombatTask_ModifyAttribute::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTireflyCombatTask_ModifyAttributeInstanceData& InstanceData = Context.GetInstanceData(*this);

	// 确定目标Actor
	AActor* TargetActor = InstanceData.ModificationTarget;
	if (!TargetActor)
	{
		TargetActor = Context.GetExternalData<AActor>("Owner");
	}

	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to modify attribute: No target actor"));
		return EStateTreeRunStatus::Failed;
	}

	// 获取AttributeComponent
	UTireflyAttributeComponent* AttributeComponent = TargetActor->FindComponentByClass<UTireflyAttributeComponent>();
	if (!AttributeComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to modify attribute: No AttributeComponent on %s"), *TargetActor->GetName());
		return EStateTreeRunStatus::Failed;
	}

	// 获取当前属性值
	float CurrentValue = AttributeComponent->GetAttributeValue(InstanceData.AttributeName, 0.0f);
	float NewValue;

	// 计算新值
	if (InstanceData.bIsPercentageModification)
	{
		NewValue = CurrentValue * (1.0f + InstanceData.ModificationValue / 100.0f);
	}
	else
	{
		NewValue = CurrentValue + InstanceData.ModificationValue;
	}

	// 设置新值
	bool bSuccess = AttributeComponent->SetAttributeValue(InstanceData.AttributeName, NewValue);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully modified attribute %s on %s: %.2f -> %.2f"), 
			*InstanceData.AttributeName.ToString(), *TargetActor->GetName(), CurrentValue, NewValue);
		return EStateTreeRunStatus::Succeeded;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to modify attribute %s on %s"), 
			*InstanceData.AttributeName.ToString(), *TargetActor->GetName());
		return EStateTreeRunStatus::Failed;
	}
}

#if WITH_EDITOR
FText FTireflyCombatTask_ModifyAttribute::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyCombatTask_ModifyAttributeInstanceData* Data = InstanceDataView.GetPtr<FTireflyCombatTask_ModifyAttributeInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString TargetName = Data->ModificationTarget ? Data->ModificationTarget->GetName() : TEXT("Context Owner");
	FString ModText = Data->bIsPercentageModification ? 
		FString::Printf(TEXT("%.1f%%"), Data->ModificationValue) :
		FString::Printf(TEXT("%.1f"), Data->ModificationValue);

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Modify Attribute:</b> %s by %s on %s"),
			*Data->AttributeName.ToString(), *ModText, *TargetName));
	}

	return FText::FromString(FString::Printf(TEXT("Modify Attribute: %s by %s on %s"),
		*Data->AttributeName.ToString(), *ModText, *TargetName));
}
#endif

//////////////////////////////////////////////////////////////////////////
// FTireflyCombatCondition_AttributeComparison Implementation
//////////////////////////////////////////////////////////////////////////

bool FTireflyCombatCondition_AttributeComparison::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FTireflyCombatCondition_AttributeComparisonInstanceData& InstanceData = Context.GetInstanceData(*this);

	// 确定目标Actor
	AActor* TargetActor = InstanceData.ComparisonTarget;
	if (!TargetActor)
	{
		TargetActor = Context.GetExternalData<AActor>("Owner");
	}

	if (!TargetActor)
	{
		return false;
	}

	// 获取AttributeComponent
	UTireflyAttributeComponent* AttributeComponent = TargetActor->FindComponentByClass<UTireflyAttributeComponent>();
	if (!AttributeComponent)
	{
		return false;
	}

	// 获取属性值
	float AttributeValue = AttributeComponent->GetAttributeValue(InstanceData.AttributeName, 0.0f);

	// 执行比较
	switch (InstanceData.ComparisonOperation)
	{
	case EArithmeticKeyOperation::Equal:
		return FMath::IsNearlyEqual(AttributeValue, InstanceData.ComparisonValue);

	case EArithmeticKeyOperation::NotEqual:
		return !FMath::IsNearlyEqual(AttributeValue, InstanceData.ComparisonValue);

	case EArithmeticKeyOperation::Less:
		return AttributeValue < InstanceData.ComparisonValue;

	case EArithmeticKeyOperation::LessOrEqual:
		return AttributeValue <= InstanceData.ComparisonValue;

	case EArithmeticKeyOperation::Greater:
		return AttributeValue > InstanceData.ComparisonValue;

	case EArithmeticKeyOperation::GreaterOrEqual:
		return AttributeValue >= InstanceData.ComparisonValue;

	default:
		return false;
	}
}

#if WITH_EDITOR
FText FTireflyCombatCondition_AttributeComparison::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyCombatCondition_AttributeComparisonInstanceData* Data = InstanceDataView.GetPtr<FTireflyCombatCondition_AttributeComparisonInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString OperationText;
	switch (Data->ComparisonOperation)
	{
	case EArithmeticKeyOperation::Equal:
		OperationText = TEXT("==");
		break;
	case EArithmeticKeyOperation::NotEqual:
		OperationText = TEXT("!=");
		break;
	case EArithmeticKeyOperation::Less:
		OperationText = TEXT("<");
		break;
	case EArithmeticKeyOperation::LessOrEqual:
		OperationText = TEXT("<=");
		break;
	case EArithmeticKeyOperation::Greater:
		OperationText = TEXT(">");
		break;
	case EArithmeticKeyOperation::GreaterOrEqual:
		OperationText = TEXT(">=");
		break;
	default:
		OperationText = TEXT("?");
		break;
	}

	FString TargetName = Data->ComparisonTarget ? Data->ComparisonTarget->GetName() : TEXT("Context Owner");

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Attribute Check:</b> %s.%s %s %.2f"),
			*TargetName, *Data->AttributeName.ToString(), *OperationText, Data->ComparisonValue));
	}

	return FText::FromString(FString::Printf(TEXT("Attribute: %s.%s %s %.2f"),
		*TargetName, *Data->AttributeName.ToString(), *OperationText, Data->ComparisonValue));
}
#endif

//////////////////////////////////////////////////////////////////////////
// FTireflyCombatCondition_HasState Implementation
//////////////////////////////////////////////////////////////////////////

bool FTireflyCombatCondition_HasState::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FTireflyCombatCondition_HasStateInstanceData& InstanceData = Context.GetInstanceData(*this);

	// 确定目标Actor
	AActor* TargetActor = InstanceData.CheckTarget;
	if (!TargetActor)
	{
		TargetActor = Context.GetExternalData<AActor>("Owner");
	}

	if (!TargetActor)
	{
		return false;
	}

	// 获取StateComponent
	UTireflyStateComponent* StateComponent = TargetActor->FindComponentByClass<UTireflyStateComponent>();
	if (!StateComponent)
	{
		return false;
	}

	if (InstanceData.bCheckSpecificState)
	{
		// 检查特定状态
		UTireflyStateInstance* StateInstance = StateComponent->GetStateInstance(InstanceData.StateDefId);
		if (!StateInstance)
		{
			return false;
		}

		// 检查堆叠数量
		return StateInstance->GetStackCount() >= InstanceData.MinimumStacks;
	}
	else
	{
		// 检查标签匹配的状态
		TArray<UTireflyStateInstance*> AllStates = StateComponent->GetAllActiveStates();
		
		for (UTireflyStateInstance* StateInstance : AllStates)
		{
			if (!StateInstance)
			{
				continue;
			}

			// 检查状态的标签是否匹配
			const FTireflyStateDefinition& StateDef = StateInstance->GetStateDef();
			bool bTagsMatch = StateDef.CategoryTags.HasAny(InstanceData.StateTagsToCheck) ||
							  StateDef.FunctionTags.HasAny(InstanceData.StateTagsToCheck);

			if (bTagsMatch && StateInstance->GetStackCount() >= InstanceData.MinimumStacks)
			{
				return true;
			}
		}

		return false;
	}
}

#if WITH_EDITOR
FText FTireflyCombatCondition_HasState::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTireflyCombatCondition_HasStateInstanceData* Data = InstanceDataView.GetPtr<FTireflyCombatCondition_HasStateInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString TargetName = Data->CheckTarget ? Data->CheckTarget->GetName() : TEXT("Context Owner");
	FString CheckText;

	if (Data->bCheckSpecificState)
	{
		CheckText = FString::Printf(TEXT("State [%s]"), *Data->StateDefId.ToString());
	}
	else
	{
		CheckText = FString::Printf(TEXT("States with tags: %s"), *Data->StateTagsToCheck.ToString());
	}

	if (Data->MinimumStacks > 1)
	{
		CheckText += FString::Printf(TEXT(" (Min stacks: %d)"), Data->MinimumStacks);
	}

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Has State:</b> %s has %s"),
			*TargetName, *CheckText));
	}

	return FText::FromString(FString::Printf(TEXT("Has State: %s has %s"),
		*TargetName, *CheckText));
}
#endif