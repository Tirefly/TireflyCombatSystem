// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "TcsCombatSystemLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"
#include "TcsCombatSystemSettings.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeExecutionContext.h"


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

    // 初始化槽位配置与StateTree映射
    InitializeStateSlots();
    BuildStateSlotMappings();
}

void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 检查StateTree状态变化并更新槽位Gate
    CheckAndUpdateStateTreeSlots();

    // 更新所有状态实例的持续时间
    TArray<UTcsStateInstance*> ExpiredStates;
	
	for (auto& DurationPair : StateDurationMap)
	{
		UTcsStateInstance* StateInstance = DurationPair.Key;
		FTcsStateDurationData& DurationData = DurationPair.Value;

		if (!IsValid(StateInstance))
		{
			// 状态实例已无效，标记为待移除
			ExpiredStates.Add(StateInstance);
			continue;
		}

		// 只更新激活状态且有时限的状态
		if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Active && 
			DurationData.DurationType == static_cast<uint8>(ETcsStateDurationType::SDT_Duration))
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
	for (UTcsStateInstance* ExpiredState : ExpiredStates)
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

    // 驱动所有激活状态的 StateTree 执行
    // 说明：UTcsStateInstance::TickStateTree 会依据其内部运行状态决定是否继续推进
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


bool UTcsStateComponent::TryAssignStateToStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag)
{
	// 向后兼容：调用新的智能分配方法
	return AssignStateToStateSlot(StateInstance, SlotTag);
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
	const UTcsCombatSystemSettings* Settings = GetDefault<UTcsCombatSystemSettings>();
	if (!Settings || !Settings->SlotConfigurationTable.IsValid())
	{
		UE_LOG(LogTcsState, Log, TEXT("[%s] No slot configuration table specified, using default configurations"), 
			*FString(__FUNCTION__));
		return;
	}
	
	UDataTable* ConfigTable = Settings->SlotConfigurationTable.LoadSynchronous();
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

	// 清理过期状态
	CleanupExpiredStates(Slot.States);

	// 检查是否存在相同定义ID的状态（应用Merger策略）
	const FName NewStateDefId = StateInstance->GetStateDefId();
	const AActor* NewInstigator = StateInstance->GetInstigator();

	for (UTcsStateInstance* ExistingState : Slot.States)
	{
		if (IsValid(ExistingState) && ExistingState->GetStateDefId() == NewStateDefId)
		{
			// 根据发起者是否相同，应用不同的合并策略
			bool bSameInstigator = (ExistingState->GetInstigator() == NewInstigator);
			return ApplyStateMergeStrategy(ExistingState, StateInstance, bSameInstigator);
		}
	}

	// 添加新状态
	Slot.States.Add(StateInstance);
	UpdateStateInstanceIndices(StateInstance);

	// 按优先级排序（优先级数值越小，优先级越高）
	SortSlotStatesByPriority(Slot.States);

	// 更新激活状态
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

    // TODO: 实现UE 5.6 StateTree API调用
    // 由于当前项目编译错误,无法确定准确的API
    // 以下是几种可能的实现方式,需要根据实际UE 5.6 API选择:

    // 方式1: 通过StateTreeExecutionContext (如果可用)
    // if (const FStateTreeExecutionContext* Context = GetStateTreeExecutionContext())
    // {
    //     ActiveStateNames = Context->GetActiveStateNames();
    // }

    // 方式2: 通过StateTree实例数据
    // if (const FStateTreeInstanceData* InstanceData = GetStateTreeInstanceData())
    // {
    //     ActiveStateNames = InstanceData->GetActiveStates();
    // }

    // 方式3: 通过UStateTreeComponent的公开接口
    // ActiveStateNames = GetActiveStates();

    return ActiveStateNames;
}

void UTcsStateComponent::CheckAndUpdateStateTreeSlots()
{
    // 获取StateTree执行上下文
    if (!StateTreeRef.IsValid())
    {
        return;
    }

    // 获取当前激活的StateTree状态名
    TArray<FName> CurrentActiveStateNames = GetCurrentActiveStateTreeStates();

    // 检测状态变化
    bool bStateChanged = (CurrentActiveStateNames.Num() != CachedActiveStateNames.Num());
    if (!bStateChanged)
    {
        for (int32 i = 0; i < CurrentActiveStateNames.Num(); ++i)
        {
            if (CurrentActiveStateNames[i] != CachedActiveStateNames[i])
            {
                bStateChanged = true;
                break;
            }
        }
    }

    // 如果状态发生变化,更新槽位Gate
    if (bStateChanged)
    {
        TSet<FGameplayTag> SlotsToUpdate;

        for (const auto& Pair : SlotToStateHandleMap)
        {
            const FGameplayTag SlotTag = Pair.Key;
            const FTcsStateSlotDefinition* Def = StateSlotDefinitions.Find(SlotTag);
            if (!Def || Def->StateTreeStateName.IsNone())
            {
                continue;
            }

            // 检查该槽位对应的StateTree状态是否激活
            const bool bShouldOpen = CurrentActiveStateNames.Contains(Def->StateTreeStateName);
            const bool bWasOpen = IsSlotGateOpen(SlotTag);

            if (bShouldOpen != bWasOpen)
            {
                SetSlotGateOpen(SlotTag, bShouldOpen);
                SlotsToUpdate.Add(SlotTag);

                UE_LOG(LogTcsState, Verbose, TEXT("[StateTree Integration] Slot [%s] gate %s due to StateTree state '%s' %s"),
                    *SlotTag.ToString(),
                    bShouldOpen ? TEXT("opened") : TEXT("closed"),
                    *Def->StateTreeStateName.ToString(),
                    bShouldOpen ? TEXT("activated") : TEXT("deactivated"));
            }
        }

        // 更新所有变化的槽位激活状态
        for (const FGameplayTag& SlotTag : SlotsToUpdate)
        {
            UpdateStateSlotActivation(SlotTag);
        }

        // 缓存当前状态
        CachedActiveStateNames = CurrentActiveStateNames;
    }
}

void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag SlotTag)
{
    FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
    if (!Slot || Slot->States.IsEmpty())
    {
        return;
    }

    // 获取槽位配置
    FTcsStateSlotDefinition Def = GetStateSlotDefinition(SlotTag);

    // 若 Gate 关闭，则停用该槽位所有状态
    if (!Slot->bIsGateOpen)
    {
        for (UTcsStateInstance* State : Slot->States)
        {
            if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
            {
                DeactivateState(State);
            }
        }
        UE_LOG(LogTcsState, Verbose, TEXT("Slot [%s] gate closed: all states set Inactive"), *SlotTag.ToString());
        return;
    }

	switch (Def.ActivationMode)
	{
	case ETcsStateSlotActivationMode::SSAM_PriorityOnly:
		{
			// 优先级激活模式：只激活最高优先级状态
			UTcsStateInstance* HighestPriorityState = nullptr;
			int32 HighestPriority = INT32_MAX;

			// 先将所有状态设为非激活
			for (UTcsStateInstance* State : Slot->States)
			{
				if (IsValid(State))
				{
					if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
					{
						DeactivateState(State);
					}

					// 查找最高优先级状态
					const int32 StatePriority = State->GetStateDef().Priority;
					if (StatePriority < HighestPriority)
					{
						HighestPriority = StatePriority;
						HighestPriorityState = State;
        }
    }
}

			// 激活最高优先级状态
			if (HighestPriorityState)
			{
				ActivateState(HighestPriorityState);
				UE_LOG(LogTcsState, Verbose, TEXT("Activated highest priority state [%s] in slot [%s]"),
					*HighestPriorityState->GetStateDefId().ToString(), *SlotTag.ToString());
			}
		}
		break;

	case ETcsStateSlotActivationMode::SSAM_AllActive:
		{
			// 全部激活模式：激活所有有效状态
			for (UTcsStateInstance* State : Slot->States)
			{
				if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Inactive)
				{
					ActivateState(State);
				}
			}

			UE_LOG(LogTcsState, Verbose, TEXT("All states activated in slot [%s]"), *SlotTag.ToString());
		}
		break;
	}
}

void UTcsStateComponent::ActivateState(UTcsStateInstance* State)
{
	if (!IsValid(State)) return;
	
	State->SetCurrentStage(ETcsStateStage::SS_Active);
	if (!State->IsStateTreeRunning())
	{
		State->StartStateTree();
	}
}

void UTcsStateComponent::DeactivateState(UTcsStateInstance* State)
{
	if (!IsValid(State)) return;
	
	State->SetCurrentStage(ETcsStateStage::SS_Inactive);
	if (State->IsStateTreeRunning())
	{
		State->StopStateTree();
	}
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

bool UTcsStateComponent::ApplyStateMergeStrategy(UTcsStateInstance* ExistingState, UTcsStateInstance* NewState, bool bSameInstigator)
{
	if (!IsValid(ExistingState) || !IsValid(NewState))
	{
		return false;
	}
	
	// 暂时简化：如果状态相同，则拒绝添加新状态，让现有状态继续运行
	// TODO: 将来可以集成完整的UTcsStateMerger系统
	UE_LOG(LogTcsState, Log, TEXT("State [%s] already exists in slot, skipping duplicate"), 
		*ExistingState->GetStateDefId().ToString());
	
	return true; // 返回true表示已处理，不需要添加新状态
}

#pragma endregion
