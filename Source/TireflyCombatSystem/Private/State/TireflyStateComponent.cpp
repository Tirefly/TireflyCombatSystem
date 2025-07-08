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
