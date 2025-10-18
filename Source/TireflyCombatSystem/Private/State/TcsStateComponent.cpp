// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"
#include "TcsDeveloperSettings.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeInstanceData.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "State/StateMerger/TcsStateMerger_NoMerge.h"


UTcsStateComponent::UTcsStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTcsStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// 获取状态管理器子系统
	if (UWorld* World = GetWorld())
	{
		StateManagerSubsystem = World->GetSubsystem<UTcsStateManagerSubsystem>();
	}

	bPendingFullGateRefresh = true;
	LastGateAutoRefreshTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bInitialGateSyncCompleted = false;

    // 初始化槽位配置与StateTree映射
    InitializeStateSlots();
    BuildStateSlotMappings();
}

void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bShouldAutoRefresh =
		GateAutoRefreshInterval > 0.f &&
		(!bInitialGateSyncCompleted || (CurrentTime - LastGateAutoRefreshTime) >= GateAutoRefreshInterval);

	if (bPendingFullGateRefresh || SlotsPendingGateRefresh.Num() > 0 || bShouldAutoRefresh)
	{
		CheckAndUpdateStateTreeSlots();
		bPendingFullGateRefresh = false;
		SlotsPendingGateRefresh.Reset();
		bInitialGateSyncCompleted = true;
		LastGateAutoRefreshTime = CurrentTime;
	}

	// 处理排队状态
	ProcessQueuedStates(DeltaTime);

	// 更新持续时间
	TArray<UTcsStateInstance*> ExpiredStates;

	for (auto& DurationPair : StateDurationMap)
	{
		UTcsStateInstance* StateInstance = DurationPair.Key;
		FTcsStateDurationData& DurationData = DurationPair.Value;

		if (!IsValid(StateInstance))
		{
			ExpiredStates.Add(StateInstance);
			continue;
		}

		if (DurationData.DurationType != static_cast<uint8>(ETcsStateDurationType::SDT_Duration))
		{
			continue;
		}

		const ETcsStateStage CurrentStage = StateInstance->GetCurrentStage();
		const bool bStateIsActive = (CurrentStage == ETcsStateStage::SS_Active);
		const FGameplayTag SlotTag = StateInstance->GetStateDef().StateSlotType;
		const FTcsStateSlotDefinition SlotDefinition = SlotTag.IsValid() ? GetStateSlotDefinition(SlotTag) : FTcsStateSlotDefinition();
		const FTcsStateSlot* SlotData = SlotTag.IsValid() ? StateSlots.Find(SlotTag) : nullptr;
		const bool bGateOpen = !SlotTag.IsValid() || (SlotData && SlotData->bIsGateOpen);

		bool bShouldTick = false;
		switch (SlotDefinition.DurationTickPolicy)
		{
		case ETcsDurationTickPolicy::DTP_Always:
			bShouldTick = true;
			break;
		case ETcsDurationTickPolicy::DTP_OnlyWhenGateOpen:
			bShouldTick = bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveOrGateOpen:
			bShouldTick = bStateIsActive || bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveAndGateOpen:
			bShouldTick = bStateIsActive && bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveOnly:
		default:
			bShouldTick = bStateIsActive;
			break;
		}

		if (!bShouldTick)
		{
			continue;
		}

		DurationData.RemainingDuration -= DeltaTime;

		if (DurationData.RemainingDuration <= 0.0f)
		{
			ExpiredStates.Add(StateInstance);
		}
	}

	for (UTcsStateInstance* ExpiredState : ExpiredStates)
	{
		if (IsValid(ExpiredState))
		{
			if (StateManagerSubsystem)
			{
				StateManagerSubsystem->OnStateInstanceDurationExpired(ExpiredState);
			}
		}

		StateDurationMap.Remove(ExpiredState);
	}

	// Tick 激活状态
	for (UTcsStateInstance* ActiveState : ActiveStateInstances)
	{
		if (IsValid(ActiveState) && ActiveState->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			ActiveState->TickStateTree(DeltaTime);
		}
	}
}

void UTcsStateComponent::AddStateInstance(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	// 获取状态定义
	const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();
	
	// 计算初始剩余时间
	float InitialRemainingDuration = 0.0f;
	if (StateDef.DurationType == ETcsStateDurationType::SDT_Duration)
	{
		InitialRemainingDuration = StateDef.Duration;
	}
	else if (StateDef.DurationType == ETcsStateDurationType::SDT_Infinite)
	{
		InitialRemainingDuration = -1.0f;
	}

	// 添加到管理映射
	FTcsStateDurationData DurationData(
		StateInstance,
		InitialRemainingDuration,
		StateDef.DurationType
	);
	
	StateDurationMap.Add(StateInstance, DurationData);
}

void UTcsStateComponent::RemoveStateInstance(const UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	StateDurationMap.Remove(StateInstance);
}

float UTcsStateComponent::GetStateInstanceDurationRemaining(const UTcsStateInstance* StateInstance) const
{
	if (const FTcsStateDurationData* DurationData = StateDurationMap.Find(StateInstance))
	{
		return DurationData->RemainingDuration;
	}
	
	return 0.0f;
}

void UTcsStateComponent::RefreshStateInstanceDurationRemaining(UTcsStateInstance* StateInstance)
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

	FTcsStateDurationData* DurationData = StateDurationMap.Find(StateInstance);
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

void UTcsStateComponent::SetStateInstanceDurationRemaining(UTcsStateInstance* StateInstance, float InDurationRemaining)
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

	FTcsStateDurationData* DurationData = StateDurationMap.Find(StateInstance);
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

UTcsStateInstance* UTcsStateComponent::GetStateInstance(FName StateDefId) const
{
	if (UTcsStateInstance* const* FoundInstance = StateInstancesByDefId.Find(StateDefId))
	{
		return *FoundInstance;
	}
	return nullptr;
}

TArray<UTcsStateInstance*> UTcsStateComponent::GetAllActiveStates() const
{
	return ActiveStateInstances;
}

TArray<UTcsStateInstance*> UTcsStateComponent::GetStatesByType(TEnumAsByte<ETcsStateType> StateType) const
{
	TArray<UTcsStateInstance*> Result;
	Result.Reserve(ActiveStateInstances.Num()); // 预分配内存以提高性能
	
	for (UTcsStateInstance* StateInstance : ActiveStateInstances)
	{
		if (IsValid(StateInstance) && StateInstance->GetStateDef().StateType == StateType)
		{
			Result.Add(StateInstance);
		}
	}
	
	return Result;
}

TArray<UTcsStateInstance*> UTcsStateComponent::GetStatesByTags(const FGameplayTagContainer& Tags) const
{
	TArray<UTcsStateInstance*> MatchingStates;
	
	for (UTcsStateInstance* StateInstance : ActiveStateInstances)
	{
		if (!IsValid(StateInstance))
		{
			continue;
		}

		const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();
		
		// 检查类别标签或功能标签是否匹配
		if (StateDef.CategoryTags.HasAny(Tags) || StateDef.FunctionTags.HasAny(Tags))
		{
			MatchingStates.Add(StateInstance);
		}
	}
	
	return MatchingStates;
}

void UTcsStateComponent::NotifyStateStageChanged(UTcsStateInstance* StateInstance, ETcsStateStage PreviousStage, ETcsStateStage NewStage)
{
	if (!IsValid(StateInstance) || PreviousStage == NewStage)
	{
		return;
	}

	if (NewStage == ETcsStateStage::SS_Active)
	{
		ActiveStateInstances.AddUnique(StateInstance);
	}
	else
	{
		ActiveStateInstances.Remove(StateInstance);
	}

	if (OnStateStageChanged.IsBound())
	{
		OnStateStageChanged.Broadcast(this, StateInstance, PreviousStage, NewStage);
	}
}

#pragma endregion

#pragma region StateTreeSlots Implementation

bool UTcsStateComponent::IsStateSlotOccupied(FGameplayTag SlotTag) const
{
	if (const FTcsStateSlot* Slot = StateSlots.Find(SlotTag))
	{
		// 检查槽位中是否有有效的状态实例
		for (UTcsStateInstance* StateInstance : Slot->States)
		{
			if (IsValid(StateInstance) && StateInstance->GetCurrentStage() == ETcsStateStage::SS_Active)
			{
				return true;
			}
		}
	}
	return false;
}

UTcsStateInstance* UTcsStateComponent::GetStateSlotCurrentState(FGameplayTag SlotTag) const
{
	// 向后兼容：返回最高优先级激活状态
	return GetHighestPriorityActiveState(SlotTag);
}

void UTcsStateComponent::RemoveStateFromStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot)
	{
		return;
	}

	// 从槽位中移除状态实例
	int32 RemovedCount = Slot->States.Remove(StateInstance);

	if (RemovedCount > 0)
	{
		// 如果槽位为空，考虑移除整个槽位记录
		if (Slot->States.Num() == 0)
		{
			StateSlots.Remove(SlotTag);
		}
		else
		{
			// 更新槽位中剩余状态的激活状态
			UpdateStateSlotActivation(SlotTag);
		}

		// 触发槽位变化事件
		OnStateSlotChanged(SlotTag);

		UE_LOG(LogTcsState, Log, TEXT("State [%s] removed from slot [%s]"),
			*StateInstance->GetStateDefId().ToString(), *SlotTag.ToString());
	}
}

void UTcsStateComponent::ClearStateSlot(FGameplayTag SlotTag)
{
	FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot)
	{
		return;
	}

	// 清空槽位中的所有状态实例
	Slot->States.Empty();

	// 移除槽位记录
	StateSlots.Remove(SlotTag);
	ClearQueuedStatesForSlot(SlotTag);

	// 触发槽位变化事件
	OnStateSlotChanged(SlotTag);

	UE_LOG(LogTcsState, Log, TEXT("Slot [%s] cleared"), *SlotTag.ToString());
}

void UTcsStateComponent::OnStateSlotChanged(FGameplayTag SlotTag)
{
	// StateTree状态槽变化事件处理
	// 这里可以添加状态槽变化的响应逻辑
	// 例如：通知StateTree系统、发送游戏事件等
	
	UE_LOG(LogTcsState, VeryVerbose, TEXT("State slot [%s] changed"), *SlotTag.ToString());
}

void UTcsStateComponent::SyncStateInstanceToStateSlot(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 根据状态定义自动分配到对应槽位
	const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();

	if (StateDef.StateSlotType.IsValid())
	{
		// 确保状态实例在正确的槽位中
		FTcsStateSlot& Slot = StateSlots.FindOrAdd(StateDef.StateSlotType);

		if (!Slot.States.Contains(StateInstance))
		{
			Slot.States.Add(StateInstance);
		}
	}

	// 更新索引映射
	UpdateStateInstanceIndices(StateInstance);
}

void UTcsStateComponent::RemoveStateInstanceFromStateSlot(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();
	
	if (StateDef.StateSlotType.IsValid())
	{
		RemoveStateFromStateSlot(StateInstance, StateDef.StateSlotType);
	}

	// 清理索引映射
	CleanupStateInstanceIndices(StateInstance);
}

void UTcsStateComponent::UpdateStateInstanceIndices(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 更新活跃状态列表
	if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Active)
	{
		ActiveStateInstances.AddUnique(StateInstance);
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

void UTcsStateComponent::CleanupStateInstanceIndices(UTcsStateInstance* StateInstance)
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


#pragma region SlotConfiguration

void UTcsStateComponent::InitializeStateSlots()
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings || !Settings->StateSlotDefTable.IsValid())
	{
		UE_LOG(LogTcsState, Log, TEXT("[%s] No slot configuration table specified, using default configurations"), 
			*FString(__FUNCTION__));
		return;
	}
	
	UDataTable* ConfigTable = Settings->StateSlotDefTable.LoadSynchronous();
	if (!ConfigTable)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to load slot configuration table"), *FString(__FUNCTION__));
		return;
	}
	
	// 读取所有配置行
	TArray<FTcsStateSlotDefinition*> AllRows;
	ConfigTable->GetAllRows<FTcsStateSlotDefinition>(TEXT("SlotConfiguration"), AllRows);
	
	for (const FTcsStateSlotDefinition* Row : AllRows)
	{
		if (Row && Row->SlotTag.IsValid())
		{
			SetStateSlotDefinition(*Row);
			UE_LOG(LogTcsState, Log, TEXT("Loaded slot configuration: [%s] -> Mode: %d"), 
				*Row->SlotTag.ToString(), (int32)Row->ActivationMode);
		}
	}
}

bool UTcsStateComponent::AssignStateToStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return false;
	}

	// 获取或创建槽位
	FTcsStateSlot& Slot = StateSlots.FindOrAdd(SlotTag);
	CleanupExpiredStates(Slot.States);

	// 获取槽位配置，若不存在则使用默认值
	FTcsStateSlotDefinition& SlotDefinition = StateSlotDefinitions.FindOrAdd(SlotTag);
	if (!SlotDefinition.SlotTag.IsValid())
	{
		SlotDefinition.SlotTag = SlotTag;
	}

	// 检查重复定义，使用合并器处理
	const FName NewStateDefId = StateInstance->GetStateDefId();
	const AActor* NewInstigator = StateInstance->GetInstigator();
	for (UTcsStateInstance* ExistingState : Slot.States)
	{
		if (IsValid(ExistingState) && ExistingState->GetStateDefId() == NewStateDefId)
		{
			const bool bSameInstigator = (ExistingState->GetInstigator() == NewInstigator);
			if (ApplyStateMergeStrategy(SlotTag, StateInstance, bSameInstigator))
			{
				return true;
			}
			break;
		}
	}

	// Gate关闭时，根据配置选择排队
	if (!Slot.bIsGateOpen && SlotDefinition.bEnableQueue)
	{
		QueueStateForSlot(StateInstance, SlotDefinition, SlotTag);
		return true;
	}

	// 直接加入槽位
	Slot.States.Add(StateInstance);
	UpdateStateInstanceIndices(StateInstance);
	SortSlotStatesByPriority(Slot.States);

	UpdateStateSlotActivation(SlotTag);
	OnStateSlotChanged(SlotTag);

	UE_LOG(LogTcsState, Log, TEXT("State [%s] assigned to slot [%s] with priority %d"),
		*StateInstance->GetStateDefId().ToString(), *SlotTag.ToString(),
		StateInstance->GetStateDef().Priority);

	return true;
}

UTcsStateInstance* UTcsStateComponent::GetHighestPriorityActiveState(FGameplayTag SlotTag) const
{
	const FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot || Slot->States.IsEmpty())
	{
		return nullptr;
	}

	UTcsStateInstance* HighestPriorityState = nullptr;
	int32 HighestPriority = INT32_MAX;

	for (UTcsStateInstance* State : Slot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			const int32 Priority = State->GetStateDef().Priority;
			if (Priority < HighestPriority)
			{
				HighestPriority = Priority;
				HighestPriorityState = State;
			}
		}
	}

	return HighestPriorityState;
}

TArray<UTcsStateInstance*> UTcsStateComponent::GetActiveStatesInStateSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsStateInstance*> ActiveStates;

	const FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot) return ActiveStates;

	for (UTcsStateInstance* State : Slot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			ActiveStates.Add(State);
		}
	}

	// 按优先级排序
	ActiveStates.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B) {
		return A.GetStateDef().Priority < B.GetStateDef().Priority;
	});

	return ActiveStates;
}

TArray<UTcsStateInstance*> UTcsStateComponent::GetAllStatesInStateSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsStateInstance*> AllStates;

	const FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot) return AllStates;

	for (UTcsStateInstance* State : Slot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() != ETcsStateStage::SS_Expired)
		{
			AllStates.Add(State);
		}
	}

	// 按优先级排序
	AllStates.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B) {
		return A.GetStateDef().Priority < B.GetStateDef().Priority;
	});

	return AllStates;
}

FString UTcsStateComponent::GetSlotDebugSnapshot(FGameplayTag SlotFilter) const
{
	auto BuildLine = [this](const FGameplayTag& SlotTag, const FTcsStateSlot& Slot) -> FString
	{
		FString Line = FString::Printf(TEXT("[%s] Gate=%s"),
			*SlotTag.ToString(),
			Slot.bIsGateOpen ? TEXT("Open") : TEXT("Closed"));

		TArray<FString> ActiveStates;
		TArray<FString> HangUpStates;
		TArray<FString> StoredStates;

		for (UTcsStateInstance* State : Slot.States)
		{
			if (!IsValid(State))
			{
				continue;
			}

			const FString StateName = State->GetStateDefId().ToString();
			switch (State->GetCurrentStage())
			{
			case ETcsStateStage::SS_Active:
				ActiveStates.Add(StateName);
				break;
			case ETcsStateStage::SS_HangUp:
				HangUpStates.Add(StateName);
				break;
			case ETcsStateStage::SS_Inactive:
				StoredStates.Add(StateName);
				break;
			default:
				StoredStates.Add(FString::Printf(TEXT("%s(%s)"),
					*StateName,
					*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage()))));
				break;
			}
		}

		auto AppendList = [&Line](const TCHAR* Label, const TArray<FString>& Names)
		{
			if (Names.Num() > 0)
			{
				Line += FString::Printf(TEXT(" %s={%s}"), Label, *FString::Join(Names, TEXT(", ")));
			}
		};

		AppendList(TEXT("Active"), ActiveStates);
		AppendList(TEXT("HangUp"), HangUpStates);
		AppendList(TEXT("Stored"), StoredStates);

		return Line;
	};

	if (SlotFilter.IsValid())
	{
		if (const FTcsStateSlot* Slot = StateSlots.Find(SlotFilter))
		{
			return BuildLine(SlotFilter, *Slot);
		}
		return FString::Printf(TEXT("[%s] <slot not initialized>"), *SlotFilter.ToString());
	}

	FString Accumulator;
	for (const TPair<FGameplayTag, FTcsStateSlot>& Pair : StateSlots)
	{
		if (!Accumulator.IsEmpty())
		{
			Accumulator += TEXT("\n");
		}
		Accumulator += BuildLine(Pair.Key, Pair.Value);
	}

	if (Accumulator.IsEmpty())
	{
		Accumulator = TEXT("<no slots>");
	}

	return Accumulator;
}

void UTcsStateComponent::SetStateSlotDefinition(const FTcsStateSlotDefinition& SlotDef)
{
	StateSlotDefinitions.FindOrAdd(SlotDef.SlotTag) = SlotDef;
	
	// 如果槽位已有状态，重新应用激活逻辑
	if (StateSlots.Contains(SlotDef.SlotTag))
	{
		UpdateStateSlotActivation(SlotDef.SlotTag);
	}
	
	UE_LOG(LogTcsState, Log, TEXT("Slot [%s] configuration set to mode: %d"), 
		*SlotDef.SlotTag.ToString(), (int32)SlotDef.ActivationMode);
}

FTcsStateSlotDefinition UTcsStateComponent::GetStateSlotDefinition(FGameplayTag SlotTag) const
{
	if (const FTcsStateSlotDefinition* Def = StateSlotDefinitions.Find(SlotTag))
	{
		return *Def;
	}
	
	// 返回默认配置（AllActive模式）
	return FTcsStateSlotDefinition();
}

void UTcsStateComponent::BuildStateSlotMappings()
{
    SlotToStateHandleMap.Empty();
    StateHandleToSlotMap.Empty();

    const UStateTree* StateTree = StateTreeRef.GetStateTree();
    if (!StateTree)
    {
        UE_LOG(LogTcsState, Verbose, TEXT("[%s] No StateTree assigned on StateComponent of %s"), *FString(__FUNCTION__), *GetOwner()->GetName());
        return;
    }

    // 遍历配置中包含 StateTreeStateName 的槽位定义，尝试匹配到 StateTree 状态
    for (const auto& Pair : StateSlotDefinitions)
    {
        const FGameplayTag SlotTag = Pair.Key;
        const FTcsStateSlotDefinition& Def = Pair.Value;
        if (Def.StateTreeStateName.IsNone())
        {
            continue; // 未指定映射，跳过
        }

        // 迭代StateTree的运行时状态，匹配名字
        bool bMapped = false;
        // 注意：GetStates() 在5.6可用；若未来API变化，可改为其它查找方案
        const TArrayView<const FCompactStateTreeState> States = StateTree->GetStates();
        for (int32 Index = 0; Index < States.Num(); ++Index)
        {
            const FCompactStateTreeState& S = States[Index];
            if (S.Name == Def.StateTreeStateName)
            {
                FStateTreeStateHandle Handle(Index);
                SlotToStateHandleMap.Add(SlotTag, Handle);
                StateHandleToSlotMap.Add(Handle, SlotTag);
                bMapped = true;
                break;
            }
        }

        UE_LOG(LogTcsState, Log, TEXT("[%s] Slot [%s] -> StateTree State [%s] %s"),
            *FString(__FUNCTION__), *SlotTag.ToString(), *Def.StateTreeStateName.ToString(), bMapped ? TEXT("mapped") : TEXT("not found"));
    }
}

bool UTcsStateComponent::IsStateTreeSlotActive(FGameplayTag SlotTag) const
{
    // 回退策略：若无法直接查询StateTree执行器的激活状态，则使用槽位激活状态判断
    return IsStateSlotOccupied(SlotTag);
}

void UTcsStateComponent::SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen)
{
    FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
    if (!Slot)
    {
        // 如果槽位不存在,创建一个新的槽位
        FTcsStateSlot& NewSlot = StateSlots.Add(SlotTag);
        NewSlot.bIsGateOpen = bOpen;
        UE_LOG(LogTcsState, Verbose, TEXT("Slot gate %s -> %s (new slot created)"), *SlotTag.ToString(), bOpen ? TEXT("Open") : TEXT("Closed"));
        return;
    }

	if (Slot->bIsGateOpen != bOpen)
	{
		Slot->bIsGateOpen = bOpen;
		UE_LOG(LogTcsState, Verbose, TEXT("Slot gate %s -> %s"), *SlotTag.ToString(), bOpen ? TEXT("Open") : TEXT("Closed"));
		UpdateStateSlotActivation(SlotTag);
	}
}

bool UTcsStateComponent::IsSlotGateOpen(FGameplayTag SlotTag) const
{
    if (const FTcsStateSlot* Slot = StateSlots.Find(SlotTag))
    {
        return Slot->bIsGateOpen;
    }
    // 默认开启，保持向后兼容
    return true;
}

TArray<FName> UTcsStateComponent::GetCurrentActiveStateTreeStates() const
{
	TArray<FName> ActiveStateNames;

	if (!StateTreeRef.IsValid())
	{
		return ActiveStateNames;
	}

	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		return ActiveStateNames;
	}

	// StateTree API 目前只提供 const 访问接口，这里通过 const_cast 获取可写指针以创建 ExecutionContext。
	UTcsStateComponent* MutableThis = const_cast<UTcsStateComponent*>(this);
	FStateTreeInstanceData* MutableInstanceData = &MutableThis->InstanceData;
	FStateTreeExecutionContext Context(*MutableThis, *StateTree, *MutableInstanceData);
	ActiveStateNames = Context.GetActiveStateNames();

	// 如果StateTree没有激活状态，则返回缓存，避免外部逻辑误判为发生变化。
	if (ActiveStateNames.IsEmpty() && !CachedActiveStateNames.IsEmpty())
	{
		ActiveStateNames = CachedActiveStateNames;
	}

	return ActiveStateNames;
}

void UTcsStateComponent::OnStateTreeStateChanged(const FStateTreeExecutionContext& Context)
{
	// 标记已收到Task通知
	bHasTaskNotification = true;
	LastTaskNotificationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// 【关键API】从ExecutionContext获取当前激活状态
	TArray<FName> CurrentActiveStates = Context.GetActiveStateNames();

	// 检测变化
	if (!AreStateNamesEqual(CurrentActiveStates, CachedActiveStateNames))
	{
		RefreshSlotsForStateChange(CurrentActiveStates, CachedActiveStateNames);
		CachedActiveStateNames = CurrentActiveStates;

		UE_LOG(LogTcsState, Log,
			   TEXT("[StateTree Event] State changed: %d active states"),
			   CurrentActiveStates.Num());
	}
}

void UTcsStateComponent::RefreshSlotsForStateChange(
	const TArray<FName>& NewStates,
	const TArray<FName>& OldStates)
{
	// 计算新增的状态
	TSet<FName> AddedStates(NewStates);
	for (const FName& OldState : OldStates)
	{
		AddedStates.Remove(OldState);
	}

	// 计算移除的状态
	TSet<FName> RemovedStates(OldStates);
	for (const FName& NewState : NewStates)
	{
		RemovedStates.Remove(NewState);
	}

	// 遍历槽位映射，更新Gate状态
	for (const auto& Pair : SlotToStateHandleMap)
	{
		const FGameplayTag SlotTag = Pair.Key;
		const FTcsStateSlotDefinition* SlotDef = StateSlotDefinitions.Find(SlotTag);

		if (!SlotDef || SlotDef->StateTreeStateName.IsNone())
		{
			continue;
		}

		const FName& MappedStateName = SlotDef->StateTreeStateName;
		bool bShouldOpen = false;

		if (AddedStates.Contains(MappedStateName))
		{
			bShouldOpen = true;
		}
		else if (RemovedStates.Contains(MappedStateName))
		{
			bShouldOpen = false;
		}
		else
		{
			bShouldOpen = NewStates.Contains(MappedStateName);
		}

		const bool bWasOpen = IsSlotGateOpen(SlotTag);
		if (bShouldOpen != bWasOpen)
		{
			SetSlotGateOpen(SlotTag, bShouldOpen);
			UpdateStateSlotActivation(SlotTag);

			UE_LOG(LogTcsState, Log,
				   TEXT("[StateTree Event] Slot [%s] gate %s due to StateTree state '%s'"),
				   *SlotTag.ToString(),
				   bShouldOpen ? TEXT("opened") : TEXT("closed"),
				   *MappedStateName.ToString());
		}
	}
}

bool UTcsStateComponent::AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 i = 0; i < A.Num(); ++i)
	{
		if (A[i] != B[i])
		{
			return false;
		}
	}

	return true;
}

void UTcsStateComponent::CheckAndUpdateStateTreeSlots()
{
	// 【性能优化】智能轮询决策树:
	// 1. 如果Task正常通知 → 完全跳过轮询
	// 2. 如果没有Task通知过 → 启用低频轮询（兜底）
	// 3. 如果Task曾通知但很久没通知 → 启用低频轮询（检测异常）

	if (!StateTreeRef.IsValid())
	{
		return; // StateTree未设置，无需轮询
	}

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// 【第一层判断】如果Task最近有通知，完全禁用轮询
	if (bHasTaskNotification && (CurrentTime - LastTaskNotificationTime) < 1.0)
	{
		// Task在过去1秒内有通知，说明Task工作正常
		// 可以完全信任Task的回调，跳过轮询
		return;
	}

	// 【第二层判断】检查轮询频率，避免每帧都执行
	if (CurrentTime - LastPollingTime < PollingFallbackInterval)
	{
		return; // 轮询间隔未到，继续等待
	}

	LastPollingTime = CurrentTime;

	// 【第三层:执行轮询】低频查询当前激活状态
	TArray<FName> CurrentActiveStates = GetCurrentActiveStateTreeStates();

	if (!AreStateNamesEqual(CurrentActiveStates, CachedActiveStateNames))
	{
		// 状态发生变化，需要更新Gate
		const FString OldStatesStr = FString::JoinBy(CachedActiveStateNames, TEXT(","), [](const FName& N) { return N.ToString(); });
		const FString NewStatesStr = FString::JoinBy(CurrentActiveStates, TEXT(","), [](const FName& N) { return N.ToString(); });

		if (bHasTaskNotification)
		{
			// Task之前有通知过，但现在轮询检测到变化→可能是边界情况或Task配置有问题
			UE_LOG(LogTcsState, Warning,
				   TEXT("[Fallback Polling] Detected state change after Task notification lost. Old: [%s] New: [%s]"),
				   *OldStatesStr, *NewStatesStr);
		}
		else
		{
			// Task从未通知过，完全依赖轮询（正常的兜底行为）
			UE_LOG(LogTcsState, Log,
				   TEXT("[Fallback Polling] No Task notification detected, using polling. Old: [%s] New: [%s]"),
				   *OldStatesStr, *NewStatesStr);
		}

		RefreshSlotsForStateChange(CurrentActiveStates, CachedActiveStateNames);
		CachedActiveStateNames = CurrentActiveStates;
	}
}

void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag SlotTag)
{
	FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot)
	{
		return;
	}

	// 清理无效/过期状态
	CleanupExpiredStates(Slot->States);

	FTcsStateSlotDefinition SlotDefinition = GetStateSlotDefinition(SlotTag);
	TArray<UTcsStateInstance*> StatesToRemove;

	// Gate关闭时的处理
	if (!Slot->bIsGateOpen)
	{
		const bool bPauseOnGateClose = SlotDefinition.GateCloseBehavior == ETcsStateSlotGateClosePolicy::SSGCP_Pause;

		for (UTcsStateInstance* State : Slot->States)
		{
			if (!IsValid(State))
			{
				StatesToRemove.Add(State);
				continue;
			}

			if (bPauseOnGateClose)
			{
				if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
				{
					const ETcsStateStage PreviousStage = State->GetCurrentStage();
					State->PauseStateTree();
					NotifyStateStageChanged(State, PreviousStage, State->GetCurrentStage());
				}
			}
			else
			{
				DeactivateState(State);
				const ETcsStateStage PreExpireStage = State->GetCurrentStage();
				State->SetCurrentStage(ETcsStateStage::SS_Expired);
				NotifyStateStageChanged(State, PreExpireStage, ETcsStateStage::SS_Expired);
				StatesToRemove.Add(State);
			}
		}

		for (UTcsStateInstance* Removed : StatesToRemove)
		{
			Slot->States.Remove(Removed);
			CleanupStateInstanceIndices(Removed);
		}

		if (!bPauseOnGateClose && StatesToRemove.Num() > 0)
		{
			OnStateSlotChanged(SlotTag);
		}

		UE_LOG(LogTcsState, Verbose, TEXT("Slot [%s] gate closed (%s)."),
			*SlotTag.ToString(),
			bPauseOnGateClose ? TEXT("pause states") : TEXT("cancel states"));
		return;
	}

	bool bSlotChanged = false;

	switch (SlotDefinition.ActivationMode)
	{
	case ETcsStateSlotActivationMode::SSAM_PriorityOnly:
		{
			SortSlotStatesByPriority(Slot->States);

			UTcsStateInstance* HighestPriorityState = nullptr;
			int32 HighestPriority = TNumericLimits<int32>::Max();

			for (UTcsStateInstance* State : Slot->States)
			{
				if (!IsValid(State))
				{
					StatesToRemove.Add(State);
					continue;
				}

				const int32 Priority = State->GetStateDef().Priority;
				if (!HighestPriorityState || Priority < HighestPriority)
				{
					HighestPriorityState = State;
					HighestPriority = Priority;
				}
			}

			for (UTcsStateInstance* State : Slot->States)
			{
				if (!IsValid(State))
				{
					continue;
				}

				if (State == HighestPriorityState)
				{
					if (State->GetCurrentStage() == ETcsStateStage::SS_HangUp)
					{
						const ETcsStateStage PreviousStage = State->GetCurrentStage();
						State->ResumeStateTree();
						NotifyStateStageChanged(State, PreviousStage, State->GetCurrentStage());
						bSlotChanged = true;
					}
					else if (State->GetCurrentStage() != ETcsStateStage::SS_Active)
					{
						ActivateState(State);
						bSlotChanged = true;
					}
					continue;
				}

				if (SlotDefinition.PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_PauseLowerPriority)
				{
					if (State->GetCurrentStage() != ETcsStateStage::SS_HangUp)
					{
						const ETcsStateStage PreviousStage = State->GetCurrentStage();
						State->PauseStateTree();
						NotifyStateStageChanged(State, PreviousStage, State->GetCurrentStage());
						bSlotChanged = true;
					}
				}
				else
				{
					DeactivateState(State);
					const ETcsStateStage PreExpireStage = State->GetCurrentStage();
					State->SetCurrentStage(ETcsStateStage::SS_Expired);
					NotifyStateStageChanged(State, PreExpireStage, ETcsStateStage::SS_Expired);
					StatesToRemove.Add(State);
					bSlotChanged = true;
				}
			}
		}
		break;

	case ETcsStateSlotActivationMode::SSAM_AllActive:
		{
			for (UTcsStateInstance* State : Slot->States)
			{
				if (!IsValid(State))
				{
					StatesToRemove.Add(State);
					continue;
				}

				if (State->GetCurrentStage() == ETcsStateStage::SS_HangUp)
				{
					const ETcsStateStage PreviousStage = State->GetCurrentStage();
					State->ResumeStateTree();
					NotifyStateStageChanged(State, PreviousStage, State->GetCurrentStage());
					bSlotChanged = true;
				}
				else if (State->GetCurrentStage() == ETcsStateStage::SS_Inactive)
				{
					ActivateState(State);
					bSlotChanged = true;
				}
			}
		}
		break;
	}

	if (StatesToRemove.Num() > 0)
	{
		for (UTcsStateInstance* Removed : StatesToRemove)
		{
			Slot->States.Remove(Removed);
			CleanupStateInstanceIndices(Removed);
			RemoveStateInstance(Removed);
		}
		bSlotChanged = true;
	}

	if (bSlotChanged)
	{
		OnStateSlotChanged(SlotTag);
	}
}

void UTcsStateComponent::ActivateState(UTcsStateInstance* State)
{
	if (!IsValid(State)) return;

	const ETcsStateStage PreviousStage = State->GetCurrentStage();
	State->SetCurrentStage(ETcsStateStage::SS_Active);
	if (!State->IsStateTreeRunning())
	{
		State->StartStateTree();
	}

	NotifyStateStageChanged(State, PreviousStage, ETcsStateStage::SS_Active);
}

void UTcsStateComponent::DeactivateState(UTcsStateInstance* State)
{
	if (!IsValid(State)) return;

	const ETcsStateStage PreviousStage = State->GetCurrentStage();
	State->SetCurrentStage(ETcsStateStage::SS_Inactive);
	if (State->IsStateTreeRunning())
	{
		State->StopStateTree();
	}

	NotifyStateStageChanged(State, PreviousStage, ETcsStateStage::SS_Inactive);
}

void UTcsStateComponent::SortSlotStatesByPriority(TArray<UTcsStateInstance*>& SlotStates)
{
	SlotStates.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B) {
		// 优先级数值越小，优先级越高
		return A.GetStateDef().Priority < B.GetStateDef().Priority;
	});
}

void UTcsStateComponent::CleanupExpiredStates(TArray<UTcsStateInstance*>& SlotStates)
{
	SlotStates.RemoveAll([this](UTcsStateInstance* State) {
		if (!IsValid(State) || State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			if (IsValid(State))
			{
				CleanupStateInstanceIndices(State);
			}
			return true;
		}
		return false;
	});
}

bool UTcsStateComponent::ApplyStateMergeStrategy(FGameplayTag SlotTag, UTcsStateInstance* NewState, bool bSameInstigator)
{
	if (!IsValid(NewState))
	{
		return false;
	}

	FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
	if (!Slot)
	{
		return false;
	}

	const FName StateDefId = NewState->GetStateDefId();
	const FTcsStateDefinition& StateDef = NewState->GetStateDef();

	TSubclassOf<UTcsStateMerger> MergerClass = bSameInstigator ? StateDef.SameInstigatorMergerType : StateDef.DiffInstigatorMergerType;
	if (!MergerClass)
	{
		// 未配置合并器，交由上层继续添加
		return false;
	}

	UTcsStateMerger* Merger = MergerClass->GetDefaultObject<UTcsStateMerger>();
	if (!Merger)
	{
		UE_LOG(LogTcsState, Warning, TEXT("Merger class %s is invalid for state %s"),
			*MergerClass->GetName(), *StateDefId.ToString());
		return false;
	}

	// 构建候选列表（包含槽内同定义状态以及新状态）
	TArray<UTcsStateInstance*> Candidates;
	for (UTcsStateInstance* ExistingState : Slot->States)
	{
		if (IsValid(ExistingState) && ExistingState->GetStateDefId() == StateDefId)
		{
			Candidates.Add(ExistingState);
		}
	}
	Candidates.Add(NewState);

	TArray<UTcsStateInstance*> MergedStates;
	Merger->Merge(Candidates, MergedStates);

	TSet<UTcsStateInstance*> MergedSet;
	for (UTcsStateInstance* Merged : MergedStates)
	{
		if (IsValid(Merged))
		{
			MergedSet.Add(Merged);
		}
	}

	bool bSlotChanged = false;

	// 移除未被保留的旧状态
	for (int32 Index = Slot->States.Num() - 1; Index >= 0; --Index)
	{
		UTcsStateInstance* Existing = Slot->States[Index];
		if (!IsValid(Existing) || Existing->GetStateDefId() != StateDefId)
		{
			continue;
		}

		if (!MergedSet.Contains(Existing))
		{
			Slot->States.RemoveAt(Index);
			CleanupStateInstanceIndices(Existing);
			DeactivateState(Existing);
			const ETcsStateStage PreExpireStage = Existing->GetCurrentStage();
			Existing->SetCurrentStage(ETcsStateStage::SS_Expired);
			NotifyStateStageChanged(Existing, PreExpireStage, ETcsStateStage::SS_Expired);
			RemoveStateInstance(Existing);
			bSlotChanged = true;
		}
	}

	// 新状态若被保留，则加入槽位
	for (UTcsStateInstance* Merged : MergedStates)
	{
		if (!IsValid(Merged))
		{
			continue;
		}

		if (!Slot->States.Contains(Merged))
		{
			Slot->States.Add(Merged);
			UpdateStateInstanceIndices(Merged);
			bSlotChanged = true;
		}
	}

	// 如果新状态未被保留，直接标记过期
	if (!MergedSet.Contains(NewState))
	{
		DeactivateState(NewState);
		const ETcsStateStage PreExpireStage = NewState->GetCurrentStage();
		NewState->SetCurrentStage(ETcsStateStage::SS_Expired);
		NotifyStateStageChanged(NewState, PreExpireStage, ETcsStateStage::SS_Expired);
		RemoveStateInstance(NewState);
	}

	if (bSlotChanged)
	{
		SortSlotStatesByPriority(Slot->States);
		UpdateStateSlotActivation(SlotTag);
		OnStateSlotChanged(SlotTag);
	}

	UE_LOG(LogTcsState, Verbose, TEXT("Merged state [%s] in slot [%s] using %s (SameInstigator=%s)"),
		*StateDefId.ToString(),
		*SlotTag.ToString(),
		*MergerClass->GetName(),
		bSameInstigator ? TEXT("true") : TEXT("false"));

	return true;
}

void UTcsStateComponent::QueueStateForSlot(UTcsStateInstance* StateInstance, const FTcsStateSlotDefinition& SlotDefinition, FGameplayTag SlotTag)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	FTcsQueuedStateData QueuedEntry;
	QueuedEntry.StateInstance = StateInstance;
	QueuedEntry.TargetSlot = SlotTag;
	QueuedEntry.EnqueueTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	QueuedEntry.TimeToLive = SlotDefinition.QueueTimeToLive;

	TArray<FTcsQueuedStateData>& Queue = QueuedStatesBySlot.FindOrAdd(SlotTag);
	Queue.Add(QueuedEntry);

	UE_LOG(LogTcsState, Verbose, TEXT("State [%s] queued for slot [%s] (TTL=%.2fs)"),
		*StateInstance->GetStateDefId().ToString(),
		*SlotTag.ToString(),
		SlotDefinition.QueueTimeToLive);
}

void UTcsStateComponent::ProcessQueuedStates(float DeltaTime)
{
	if (QueuedStatesBySlot.Num() == 0)
	{
		return;
	}

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TArray<FGameplayTag> SlotsToRetry;
	auto HandleExpiration = [this](const FTcsQueuedStateData& Entry)
	{
		if (UTcsStateInstance* ExpiredState = Entry.StateInstance.Get())
		{
			DeactivateState(ExpiredState);
			const ETcsStateStage PreExpireStage = ExpiredState->GetCurrentStage();
			ExpiredState->SetCurrentStage(ETcsStateStage::SS_Expired);
			NotifyStateStageChanged(ExpiredState, PreExpireStage, ETcsStateStage::SS_Expired);
			RemoveStateInstance(ExpiredState);
		}
	};

	for (TPair<FGameplayTag, TArray<FTcsQueuedStateData>>& Pair : QueuedStatesBySlot)
	{
		FGameplayTag SlotTag = Pair.Key;
		TArray<FTcsQueuedStateData>& Queue = Pair.Value;

		if (Queue.Num() == 0)
		{
			continue;
		}

		FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
		if (!Slot || !Slot->bIsGateOpen)
		{
			for (int32 Index = Queue.Num() - 1; Index >= 0; --Index)
			{
				if (!Queue[Index].StateInstance.IsValid())
				{
					Queue.RemoveAt(Index);
					continue;
				}

				if (Queue[Index].IsExpired(CurrentTime))
				{
					HandleExpiration(Queue[Index]);
					Queue.RemoveAt(Index);
				}
			}
			continue;
		}

		SlotsToRetry.Add(SlotTag);
	}

	for (const FGameplayTag& SlotTag : SlotsToRetry)
	{
		TArray<FTcsQueuedStateData>& Queue = QueuedStatesBySlot.FindChecked(SlotTag);
		for (int32 Index = Queue.Num() - 1; Index >= 0; --Index)
		{
			if (!Queue[Index].StateInstance.IsValid())
			{
				Queue.RemoveAt(Index);
				continue;
			}

			UTcsStateInstance* QueuedState = Queue[Index].StateInstance.Get();
			if (AssignStateToStateSlot(QueuedState, SlotTag))
			{
				Queue.RemoveAt(Index);
			}
			else if (Queue[Index].IsExpired(CurrentTime))
			{
				HandleExpiration(Queue[Index]);
				Queue.RemoveAt(Index);
			}
		}
	}
}

void UTcsStateComponent::ClearQueuedStatesForSlot(FGameplayTag SlotTag)
{
	QueuedStatesBySlot.Remove(SlotTag);
}

void UTcsStateComponent::RequestGateRefresh(FGameplayTag SlotTag)
{
	if (SlotTag.IsValid())
	{
		SlotsPendingGateRefresh.Add(SlotTag);
	}
}

void UTcsStateComponent::RequestGateRefreshForAll()
{
	bPendingFullGateRefresh = true;
}

#pragma endregion
