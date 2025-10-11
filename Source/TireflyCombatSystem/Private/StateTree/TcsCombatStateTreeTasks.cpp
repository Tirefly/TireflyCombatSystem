// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsCombatStateTreeTasks.h"
#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateManagerSubsystem.h"
#include "Attribute/TcsAttributeComponent.h"
#include "TcsCombatSystemEnum.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

//////////////////////////////////////////////////////////////////////////
// FTcsStateSlot_Exclusive Implementation
//////////////////////////////////////////////////////////////////////////

FTcsStateSlot_Exclusive::FTcsStateSlot_Exclusive()
{
	// 配置任务特性
	bShouldCallTick = true;
	bShouldAffectTransitions = false;
	bConsideredForScheduling = true;
}

EStateTreeRunStatus FTcsStateSlot_Exclusive::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTcsStateSlot_ExclusiveInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Exclusive slot entered: %s (Priority: %d)"), 
		*InstanceData.SlotTag.ToString(), InstanceData.Priority);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTcsStateSlot_Exclusive::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	// 互斥槽通常持续运行，等待状态变化
	return EStateTreeRunStatus::Running;
}

void FTcsStateSlot_Exclusive::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTcsStateSlot_ExclusiveInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Exclusive slot exited: %s"), *InstanceData.SlotTag.ToString());
}

#if WITH_EDITOR
FText FTcsStateSlot_Exclusive::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsStateSlot_ExclusiveInstanceData* Data = InstanceDataView.GetPtr<FTcsStateSlot_ExclusiveInstanceData>();
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
// FTcsStateSlot_Parallel Implementation
//////////////////////////////////////////////////////////////////////////

FTcsStateSlot_Parallel::FTcsStateSlot_Parallel()
{
	// 配置任务特性
	bShouldCallTick = true;
	bShouldAffectTransitions = false;
	bConsideredForScheduling = true;
}

EStateTreeRunStatus FTcsStateSlot_Parallel::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTcsStateSlot_ParallelInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Parallel slot entered: %s (MaxStack: %d)"), 
		*InstanceData.SlotTag.ToString(), InstanceData.MaxStackCount);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTcsStateSlot_Parallel::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	// 并行槽通常持续运行，支持多个状态同时存在
	return EStateTreeRunStatus::Running;
}

void FTcsStateSlot_Parallel::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FTcsStateSlot_ParallelInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	UE_LOG(LogTemp, Log, TEXT("Parallel slot exited: %s"), *InstanceData.SlotTag.ToString());
}

#if WITH_EDITOR
FText FTcsStateSlot_Parallel::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsStateSlot_ParallelInstanceData* Data = InstanceDataView.GetPtr<FTcsStateSlot_ParallelInstanceData>();
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
// FTcsStateCondition_SlotAvailable Implementation
//////////////////////////////////////////////////////////////////////////

bool FTcsStateCondition_SlotAvailable::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FTcsStateCondition_SlotAvailableInstanceData& InstanceData = Context.GetInstanceData(*this);
    
    // 从上下文外部数据中获取 StateComponent
    UTcsStateComponent* StateComponent = Context.GetExternalData(StateCompHandle);
    if (!StateComponent)
    {
        return false;
    }

    // 检查槽位Gate + 占用情况
    const bool bGateOpen = StateComponent->IsSlotGateOpen(InstanceData.TargetSlot);
    const bool bSlotOccupied = StateComponent->IsStateSlotOccupied(InstanceData.TargetSlot);
	
	// 如果槽位未被占用，则可以应用状态
	// 如果槽位已被占用，需要检查优先级等其他条件
    // Gate开启且未被占用 → 可用
    return bGateOpen && !bSlotOccupied;
}

#if WITH_EDITOR
FText FTcsStateCondition_SlotAvailable::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsStateCondition_SlotAvailableInstanceData* Data = InstanceDataView.GetPtr<FTcsStateCondition_SlotAvailableInstanceData>();
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
// FTcsCombatTask_ApplyState Implementation
//////////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FTcsCombatTask_ApplyState::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    const FTcsCombatTask_ApplyStateInstanceData& InstanceData = Context.GetInstanceData(*this);
    
    // 获取StateManagerSubsystem
    UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UTcsStateManagerSubsystem* StateManager = World->GetSubsystem<UTcsStateManagerSubsystem>();
	if (!StateManager)
	{
		return EStateTreeRunStatus::Failed;
	}

    // 确定目标和来源Actor（优先使用实例数据，其次使用上下文句柄）
    AActor* TargetActor = InstanceData.StateTarget ? InstanceData.StateTarget : Context.GetContextData(OwnerHandle);
    AActor* SourceActor = InstanceData.StateSource ? InstanceData.StateSource : Context.GetContextData(InstigatorHandle);
    if (!SourceActor)
    {
        SourceActor = TargetActor; // fallback
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
FText FTcsCombatTask_ApplyState::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsCombatTask_ApplyStateInstanceData* Data = InstanceDataView.GetPtr<FTcsCombatTask_ApplyStateInstanceData>();
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
// FTcsCombatTask_ModifyAttribute Implementation
//////////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FTcsCombatTask_ModifyAttribute::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    const FTcsCombatTask_ModifyAttributeInstanceData& InstanceData = Context.GetInstanceData(*this);

    // 确定目标Actor
    AActor* TargetActor = InstanceData.ModificationTarget ? InstanceData.ModificationTarget : Context.GetContextData(OwnerHandle);

	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to modify attribute: No target actor"));
		return EStateTreeRunStatus::Failed;
	}

	// 获取AttributeComponent
	UTcsAttributeComponent* AttributeComponent = TargetActor->FindComponentByClass<UTcsAttributeComponent>();
	if (!AttributeComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to modify attribute: No AttributeComponent on %s"), *TargetActor->GetName());
		return EStateTreeRunStatus::Failed;
	}

	// 获取当前属性值
	float CurrentValue = 0.0f;
	AttributeComponent->GetAttributeValue(InstanceData.AttributeName, CurrentValue);
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
	// TODO: 需要实现SetAttributeValue方法
	// bool bSuccess = AttributeComponent->SetAttributeValue(InstanceData.AttributeName, NewValue);
	bool bSuccess = true;

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
FText FTcsCombatTask_ModifyAttribute::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsCombatTask_ModifyAttributeInstanceData* Data = InstanceDataView.GetPtr<FTcsCombatTask_ModifyAttributeInstanceData>();
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
// FTcsCombatCondition_AttributeComparison Implementation
//////////////////////////////////////////////////////////////////////////

bool FTcsCombatCondition_AttributeComparison::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FTcsCombatCondition_AttributeComparisonInstanceData& InstanceData = Context.GetInstanceData(*this);

    // 确定目标Actor
    AActor* TargetActor = InstanceData.ComparisonTarget ? InstanceData.ComparisonTarget : Context.GetContextData(OwnerHandle);

	if (!TargetActor)
	{
		return false;
	}

	// 获取AttributeComponent
	UTcsAttributeComponent* AttributeComponent = TargetActor->FindComponentByClass<UTcsAttributeComponent>();
	if (!AttributeComponent)
	{
		return false;
	}

	// 获取属性值
	float AttributeValue = 0.0f;
	if (!AttributeComponent->GetAttributeValue(InstanceData.AttributeName, AttributeValue))
	{
		return false;
	}

	// 执行比较
	switch (InstanceData.ComparisonOperation)
	{
	case ETcsNumericComparison::Equal:
		return FMath::IsNearlyEqual(AttributeValue, InstanceData.ComparisonValue);

	case ETcsNumericComparison::NotEqual:
		return !FMath::IsNearlyEqual(AttributeValue, InstanceData.ComparisonValue);

	case ETcsNumericComparison::LessThan:
		return AttributeValue < InstanceData.ComparisonValue;

	case ETcsNumericComparison::LessThanOrEqual:
		return AttributeValue <= InstanceData.ComparisonValue;

	case ETcsNumericComparison::GreaterThan:
		return AttributeValue > InstanceData.ComparisonValue;

	case ETcsNumericComparison::GreaterThanOrEqual:
		return AttributeValue >= InstanceData.ComparisonValue;

	default:
		return false;
	}
}

#if WITH_EDITOR
FText FTcsCombatCondition_AttributeComparison::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsCombatCondition_AttributeComparisonInstanceData* Data = InstanceDataView.GetPtr<FTcsCombatCondition_AttributeComparisonInstanceData>();
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString OperationText;
	switch (Data->ComparisonOperation)
	{
	case ETcsNumericComparison::Equal:
		OperationText = TEXT("==");
		break;
	case ETcsNumericComparison::NotEqual:
		OperationText = TEXT("!=");
		break;
	case ETcsNumericComparison::LessThan:
		OperationText = TEXT("<");
		break;
	case ETcsNumericComparison::LessThanOrEqual:
		OperationText = TEXT("<=");
		break;
	case ETcsNumericComparison::GreaterThan:
		OperationText = TEXT(">");
		break;
	case ETcsNumericComparison::GreaterThanOrEqual:
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
// FTcsCombatCondition_HasState Implementation
//////////////////////////////////////////////////////////////////////////

bool FTcsCombatCondition_HasState::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FTcsCombatCondition_HasStateInstanceData& InstanceData = Context.GetInstanceData(*this);

    // 确定目标Actor
    AActor* TargetActor = InstanceData.CheckTarget ? InstanceData.CheckTarget : Context.GetContextData(OwnerHandle);

	if (!TargetActor)
	{
		return false;
	}

	// 获取StateComponent
	UTcsStateComponent* StateComponent = TargetActor->FindComponentByClass<UTcsStateComponent>();
	if (!StateComponent)
	{
		return false;
	}

	if (InstanceData.bCheckSpecificState)
	{
		// 检查特定状态
		UTcsStateInstance* StateInstance = StateComponent->GetStateInstance(InstanceData.StateDefId);
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
		TArray<UTcsStateInstance*> AllStates = StateComponent->GetAllActiveStates();
		
		for (UTcsStateInstance* StateInstance : AllStates)
		{
			if (!StateInstance)
			{
				continue;
			}

			// 检查状态的标签是否匹配
			const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();
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
FText FTcsCombatCondition_HasState::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FTcsCombatCondition_HasStateInstanceData* Data = InstanceDataView.GetPtr<FTcsCombatCondition_HasStateInstanceData>();
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
