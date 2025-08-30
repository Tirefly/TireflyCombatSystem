// Copyright Tirefly. All Rights Reserved.


#include "TireflyCombatSystem/Public/State/TireflyStateComponent.h"

#include "TireflyCombatSystemLogChannels.h"
#include "TireflyCombatSystem/Public/State/TireflyState.h"
#include "TireflyCombatSystem/Public/State/TireflyStateManagerSubsystem.h"
#include "Engine/World.h"


UTireflyStateComponent::UTireflyStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTireflyStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// 获取状态管理器子系统
	if (UWorld* World = GetWorld())
	{
		StateManagerSubsystem = World->GetSubsystem<UTireflyStateManagerSubsystem>();
	}
}

void UTireflyStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 更新所有状态实例的持续时间
	TArray<UTireflyStateInstance*> ExpiredStates;
	
	for (auto& DurationPair : StateDurationMap)
	{
		UTireflyStateInstance* StateInstance = DurationPair.Key;
		FTireflyStateDurationData& DurationData = DurationPair.Value;

		if (!IsValid(StateInstance))
		{
			// 状态实例已无效，标记为待移除
			ExpiredStates.Add(StateInstance);
			continue;
		}

		// 只更新激活状态且有时限的状态
		if (StateInstance->GetCurrentStage() == ETireflyStateStage::SS_Active && 
			DurationData.DurationType == static_cast<uint8>(ETireflyStateDurationType::SDT_Duration))
		{
			DurationData.RemainingDuration -= DeltaTime;
			
			// 检查是否到期
			if (DurationData.RemainingDuration <= 0.0f)
			{
				ExpiredStates.Add(StateInstance);
			}
		}
	}

	// 处理到期的状态实例
	for (UTireflyStateInstance* ExpiredState : ExpiredStates)
	{
		if (IsValid(ExpiredState))
		{
			// 通知状态管理器子系统处理状态到期
			if (StateManagerSubsystem)
			{
				StateManagerSubsystem->OnStateInstanceDurationExpired(ExpiredState);
			}
		}
		
		// 从管理映射中移除
		StateDurationMap.Remove(ExpiredState);
	}
}

void UTireflyStateComponent::AddStateInstance(UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	// 获取状态定义
	const FTireflyStateDefinition& StateDef = StateInstance->GetStateDef();
	
	// 计算初始剩余时间
	float InitialRemainingDuration = 0.0f;
	if (StateDef.DurationType == ETireflyStateDurationType::SDT_Duration)
	{
		InitialRemainingDuration = StateDef.Duration;
	}
	else if (StateDef.DurationType == ETireflyStateDurationType::SDT_Infinite)
	{
		InitialRemainingDuration = -1.0f;
	}

	// 添加到管理映射
	FTireflyStateDurationData DurationData(
		StateInstance,
		InitialRemainingDuration,
		StateDef.DurationType
	);
	
	StateDurationMap.Add(StateInstance, DurationData);
}

void UTireflyStateComponent::RemoveStateInstance(const UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	StateDurationMap.Remove(StateInstance);
}

float UTireflyStateComponent::GetStateInstanceDurationRemaining(const UTireflyStateInstance* StateInstance) const
{
	if (const FTireflyStateDurationData* DurationData = StateDurationMap.Find(StateInstance))
	{
		return DurationData->RemainingDuration;
	}
	
	return 0.0f;
}

void UTireflyStateComponent::RefreshStateInstanceDurationRemaining(UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!StateManagerSubsystem)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateManagerSubsystem is not initialized."), *FString(__FUNCTION__));
		return;
	}

	FTireflyStateDurationData* DurationData = StateDurationMap.Find(StateInstance);
	if (!DurationData)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is not found in %s."),
			*FString(__FUNCTION__),
			*GetOwner()->GetName());
		return;
	}

	// 刷新为总持续时间
	DurationData->RemainingDuration = StateInstance->GetTotalDuration();
	StateManagerSubsystem->OnStateInstanceDurationUpdated(StateInstance);
}

void UTireflyStateComponent::SetStateInstanceDurationRemaining(UTireflyStateInstance* StateInstance, float InDurationRemaining)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!StateManagerSubsystem)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateManagerSubsystem is not initialized."), *FString(__FUNCTION__));
		return;
	}

	FTireflyStateDurationData* DurationData = StateDurationMap.Find(StateInstance);
	if (!DurationData)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is not found in %s."),
			*FString(__FUNCTION__),
			*GetOwner()->GetName());
		return;
	}

	// 设置新的剩余时间
	DurationData->RemainingDuration = InDurationRemaining;
	StateManagerSubsystem->OnStateInstanceDurationUpdated(StateInstance);
}

#pragma region StateInstance Extended Implementation

UTireflyStateInstance* UTireflyStateComponent::GetStateInstance(FName StateDefId) const
{
	if (UTireflyStateInstance* const* FoundInstance = StateInstancesByDefId.Find(StateDefId))
	{
		return *FoundInstance;
	}
	return nullptr;
}

TArray<UTireflyStateInstance*> UTireflyStateComponent::GetAllActiveStates() const
{
	return ActiveStateInstances;
}

TArray<UTireflyStateInstance*> UTireflyStateComponent::GetStatesByType(TEnumAsByte<ETireflyStateType> StateType) const
{
	TArray<UTireflyStateInstance*> Result;
	Result.Reserve(ActiveStateInstances.Num()); // 预分配内存以提高性能
	
	for (UTireflyStateInstance* StateInstance : ActiveStateInstances)
	{
		if (IsValid(StateInstance) && StateInstance->GetStateDef().StateType == StateType)
		{
			Result.Add(StateInstance);
		}
	}
	
	return Result;
}

TArray<UTireflyStateInstance*> UTireflyStateComponent::GetStatesByTags(const FGameplayTagContainer& Tags) const
{
	TArray<UTireflyStateInstance*> MatchingStates;
	
	for (UTireflyStateInstance* StateInstance : ActiveStateInstances)
	{
		if (!IsValid(StateInstance))
		{
			continue;
		}

		const FTireflyStateDefinition& StateDef = StateInstance->GetStateDef();
		
		// 检查类别标签或功能标签是否匹配
		if (StateDef.CategoryTags.HasAny(Tags) || StateDef.FunctionTags.HasAny(Tags))
		{
			MatchingStates.Add(StateInstance);
		}
	}
	
	return MatchingStates;
}

#pragma endregion

#pragma region StateTreeSlots Implementation

bool UTireflyStateComponent::IsSlotOccupied(FGameplayTag SlotTag) const
{
	if (const TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag))
	{
		// 检查槽位中是否有有效的状态实例
		for (UTireflyStateInstance* StateInstance : *SlotStates)
		{
			if (IsValid(StateInstance) && StateInstance->GetCurrentStage() == ETireflyStateStage::SS_Active)
			{
				return true;
			}
		}
	}
	return false;
}

UTireflyStateInstance* UTireflyStateComponent::GetSlotState(FGameplayTag SlotTag) const
{
	if (const TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag))
	{
		// 返回第一个有效的激活状态实例
		for (UTireflyStateInstance* StateInstance : *SlotStates)
		{
			if (IsValid(StateInstance) && StateInstance->GetCurrentStage() == ETireflyStateStage::SS_Active)
			{
				return StateInstance;
			}
		}
	}
	return nullptr;
}

TArray<UTireflyStateInstance*> UTireflyStateComponent::GetActiveStatesInSlot(FGameplayTag SlotTag) const
{
	TArray<UTireflyStateInstance*> ActiveSlotStates;
	
	if (const TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag))
	{
		for (UTireflyStateInstance* StateInstance : *SlotStates)
		{
			if (IsValid(StateInstance) && StateInstance->GetCurrentStage() == ETireflyStateStage::SS_Active)
			{
				ActiveSlotStates.Add(StateInstance);
			}
		}
	}
	
	return ActiveSlotStates;
}

bool UTireflyStateComponent::TryAssignStateToSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return false;
	}

	// 获取或创建槽位数组
	TArray<UTireflyStateInstance*>& SlotStates = StateSlots.FindOrAdd(SlotTag);
	
	// 检查状态实例是否已经在槽位中
	if (SlotStates.Contains(StateInstance))
	{
		return true; // 已经在槽位中
	}

	// 添加到槽位
	SlotStates.Add(StateInstance);
	
	// 同步状态实例的槽位信息
	SyncStateInstanceToSlot(StateInstance);
	
	// 触发槽位变化事件
	OnStateSlotChanged(SlotTag);
	
	UE_LOG(LogTcsState, Log, TEXT("State [%s] assigned to slot [%s]"), 
		*StateInstance->GetStateDefId().ToString(), *SlotTag.ToString());
	
	return true;
}

void UTireflyStateComponent::RemoveStateFromSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag);
	if (!SlotStates)
	{
		return;
	}

	// 从槽位中移除状态实例
	int32 RemovedCount = SlotStates->Remove(StateInstance);
	
	if (RemovedCount > 0)
	{
		// 如果槽位为空，考虑移除整个槽位记录
		if (SlotStates->Num() == 0)
		{
			StateSlots.Remove(SlotTag);
		}
		
		// 触发槽位变化事件
		OnStateSlotChanged(SlotTag);
		
		UE_LOG(LogTcsState, Log, TEXT("State [%s] removed from slot [%s]"), 
			*StateInstance->GetStateDefId().ToString(), *SlotTag.ToString());
	}
}

void UTireflyStateComponent::ClearSlot(FGameplayTag SlotTag)
{
	TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag);
	if (!SlotStates)
	{
		return;
	}

	// 清空槽位中的所有状态实例
	SlotStates->Empty();
	
	// 移除槽位记录
	StateSlots.Remove(SlotTag);
	
	// 触发槽位变化事件
	OnStateSlotChanged(SlotTag);
	
	UE_LOG(LogTcsState, Log, TEXT("Slot [%s] cleared"), *SlotTag.ToString());
}

void UTireflyStateComponent::OnStateSlotChanged(FGameplayTag SlotTag)
{
	// StateTree状态槽变化事件处理
	// 这里可以添加状态槽变化的响应逻辑
	// 例如：通知StateTree系统、发送游戏事件等
	
	UE_LOG(LogTcsState, VeryVerbose, TEXT("State slot [%s] changed"), *SlotTag.ToString());
}

void UTireflyStateComponent::SyncStateInstanceToSlot(UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 根据状态定义自动分配到对应槽位
	const FTireflyStateDefinition& StateDef = StateInstance->GetStateDef();
	
	if (StateDef.StateSlotType.IsValid())
	{
		// 确保状态实例在正确的槽位中
		TArray<UTireflyStateInstance*>& SlotStates = StateSlots.FindOrAdd(StateDef.StateSlotType);
		
		if (!SlotStates.Contains(StateInstance))
		{
			SlotStates.Add(StateInstance);
		}
	}

	// 更新索引映射
	UpdateStateInstanceIndices(StateInstance);
}

void UTireflyStateComponent::RemoveStateInstanceFromSlot(UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	const FTireflyStateDefinition& StateDef = StateInstance->GetStateDef();
	
	if (StateDef.StateSlotType.IsValid())
	{
		RemoveStateFromSlot(StateInstance, StateDef.StateSlotType);
	}

	// 清理索引映射
	CleanupStateInstanceIndices(StateInstance);
}

void UTireflyStateComponent::UpdateStateInstanceIndices(UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 更新活跃状态列表
	if (!ActiveStateInstances.Contains(StateInstance))
	{
		ActiveStateInstances.Add(StateInstance);
	}

	// 更新ID索引
	int32 InstanceId = StateInstance->GetInstanceId();
	if (InstanceId >= 0)
	{
		StateInstancesById.FindOrAdd(InstanceId) = StateInstance;
	}

	// 更新定义ID索引
	FName StateDefId = StateInstance->GetStateDefId();
	if (!StateDefId.IsNone())
	{
		StateInstancesByDefId.FindOrAdd(StateDefId) = StateInstance;
	}

	// 类型索引维护已移除 - 改为动态查询以避免UE引擎TMap嵌套限制和GC风险
}

void UTireflyStateComponent::CleanupStateInstanceIndices(UTireflyStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 从活跃状态列表移除
	ActiveStateInstances.Remove(StateInstance);

	// 从ID索引移除
	int32 InstanceId = StateInstance->GetInstanceId();
	if (InstanceId >= 0)
	{
		StateInstancesById.Remove(InstanceId);
	}

	// 从定义ID索引移除
	FName StateDefId = StateInstance->GetStateDefId();
	if (!StateDefId.IsNone())
	{
		StateInstancesByDefId.Remove(StateDefId);
	}

	// 类型索引维护已移除 - 改为动态查询以避免UE引擎TMap嵌套限制和GC风险
}

#pragma endregion
