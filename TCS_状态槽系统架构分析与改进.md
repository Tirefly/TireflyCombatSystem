# TCS 状态槽系统架构分析与改进方案

## 文档概述

本文档分析了 TireflyCombatSystem (TCS) 插件中状态槽系统的当前架构，识别了关键设计限制，并提供了完整的改进方案来支持复杂的状态管理需求。

## 目录

1. [当前系统架构分析](#当前系统架构分析)
2. [关键设计限制](#关键设计限制)
3. [业务需求分析](#业务需求分析)
4. [改进方案设计](#改进方案设计)
5. [具体实现方案](#具体实现方案)
6. [使用示例](#使用示例)
7. [迁移指南](#迁移指南)

---

## 当前系统架构分析

### 核心数据结构

TCS 当前的状态槽系统基于以下核心设计：

```cpp
// UTireflyStateComponent 中的状态槽映射
TMap<FGameplayTag, TArray<UTireflyStateInstance*>> StateSlots;

// 状态定义中的槽位类型配置
struct FTireflyStateDefinition
{
    FGameplayTag StateSlotType;          // 状态槽类型
    int32 Priority = -1;                 // 状态优先级
    int32 MaxStackCount = 1;             // 最大叠层数
    TSubclassOf<UTireflyStateMerger> SameInstigatorMergerType;   // 同源合并策略
    TSubclassOf<UTireflyStateMerger> DiffInstigatorMergerType;   // 异源合并策略
};
```

### 当前功能特性

1. **多状态容器**: 每个槽位可以容纳多个状态实例
2. **基础查询API**: 提供槽位状态查询功能
3. **状态合并机制**: 支持不同的状态合并策略
4. **优先级定义**: 状态定义中包含优先级信息

### 主要API接口

```cpp
// 状态槽查询接口
bool IsSlotOccupied(FGameplayTag SlotTag) const;
UTireflyStateInstance* GetSlotState(FGameplayTag SlotTag) const;
TArray<UTireflyStateInstance*> GetActiveStatesInSlot(FGameplayTag SlotTag) const;

// 状态槽管理接口
bool TryAssignStateToSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag);
void RemoveStateFromSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag);
void ClearSlot(FGameplayTag SlotTag);
```

---

## 关键设计限制

### ❌ 问题1: 缺少槽位级别的容量控制

**当前实现问题**：
```cpp
bool UTireflyStateComponent::TryAssignStateToSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag)
{
    TArray<UTireflyStateInstance*>& SlotStates = StateSlots.FindOrAdd(SlotTag);
    SlotStates.Add(StateInstance); // 直接添加，没有任何容量检查！
    return true;
}
```

**问题后果**：
- 所有槽位都表现为"多状态模式"
- 无法实现"单一状态槽位"的行为
- 缺乏对槽位容量的精确控制

### ❌ 问题2: 缺少优先级排序机制

**当前实现问题**：
```cpp
UTireflyStateInstance* UTireflyStateComponent::GetSlotState(FGameplayTag SlotTag) const
{
    // 返回第一个找到的激活状态实例 - 没有优先级考量！
    for (UTireflyStateInstance* StateInstance : *SlotStates)
    {
        if (IsValid(StateInstance) && StateInstance->GetCurrentStage() == ETireflyStateStage::SS_Active)
        {
            return StateInstance; // 返回遇到的第一个，而不是优先级最高的
        }
    }
}
```

**问题后果**：
- 高优先级状态可能被低优先级状态覆盖
- 无法保证状态执行的正确顺序
- 缺乏智能的状态替换逻辑

### ❌ 问题3: 缺少槽位策略配置

**当前设计问题**：
- 没有定义槽位本身的行为策略（单一/多重，优先级排序等）
- 所有槽位使用相同的管理逻辑
- 无法满足不同类型槽位的差异化需求

---

## 业务需求分析

### 需求1: Action 槽位（单一状态模式）

**业务场景**：
- 槽位名称：`StateSlot.Action`
- 行为规则：只允许一个状态存在
- 状态类型：攻击、受击、交互等
- 优先级控制：高优先级状态应该能够打断低优先级状态

**期望行为**：
```
当前状态: 攻击中 (优先级: 1)
新状态: 受击 (优先级: 0)  ← 优先级更高
结果: 受击状态打断攻击状态，成为当前激活状态

当前状态: 受击中 (优先级: 0)  
新状态: 交互 (优先级: 2)  ← 优先级更低
结果: 交互状态被拒绝，受击状态继续
```

### 需求2: Debuff 槽位（多重状态模式）

**业务场景**：
- 槽位名称：`StateSlot.Debuff`
- 行为规则：允许多个状态共存
- 状态类型：中毒、燃烧、冰冻、沉默等
- 容量限制：合理的上限控制

**期望行为**：
```
槽位状态列表:
├── 中毒状态 (3层，来自不同毒刺)
├── 燃烧状态 (1层，来自火球术)  
├── 冰冻状态 (1层，来自冰霜新星)
└── 沉默状态 (1层，来自法师技能)

所有状态同时激活，各自独立运行
```

### 需求3: Mobility 槽位（优先级栈模式）

**业务场景**：
- 槽位名称：`StateSlot.Mobility`
- 行为规则：高优先级状态会挂起低优先级状态
- 状态类型：加速、减速、定身、传送等

**期望行为**：
```
初始状态: 加速效果 (优先级: 2, 激活中)
应用定身: 定身效果 (优先级: 0, 激活中) → 加速效果被挂起
定身结束: 加速效果恢复激活状态
```

---

## 改进方案设计

### 1. 槽位策略枚举

```cpp
// 槽位策略枚举
UENUM(BlueprintType)
enum class ETireflySlotPolicy : uint8
{
    Single          UMETA(DisplayName = "Single", ToolTip = "单一状态，按优先级替换"),
    Multiple        UMETA(DisplayName = "Multiple", ToolTip = "多重状态，允许共存"),
    PriorityStack   UMETA(DisplayName = "Priority Stack", ToolTip = "优先级栈，高优先级状态挂起低优先级")
};
```

### 2. 槽位配置结构

```cpp
// 槽位配置数据结构
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflySlotConfiguration
{
    GENERATED_BODY()

public:
    // 槽位策略
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Policy")
    ETireflySlotPolicy SlotPolicy = ETireflySlotPolicy::Multiple;
    
    // 最大容量（仅Multiple模式有效）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Policy", 
        meta = (EditCondition = "SlotPolicy == ETireflySlotPolicy::Multiple", ClampMin = "1", ClampMax = "50"))
    int32 MaxCapacity = 10;
    
    // 是否按优先级排序
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Policy")
    bool bSortByPriority = true;
    
    // 是否允许相同定义ID的状态共存
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Policy")
    bool bAllowSameDefId = false;
    
    // 容量满时的替换策略
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Policy",
        meta = (EditCondition = "SlotPolicy == ETireflySlotPolicy::Multiple"))
    bool bReplaceLowestPriority = true;
    
    // 状态挂起时是否保持Tick
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Policy",
        meta = (EditCondition = "SlotPolicy == ETireflySlotPolicy::PriorityStack"))
    bool bTickSuspendedStates = false;
};
```

### 3. 增强的组件接口

```cpp
class TIREFLYCOMBATSYSTEM_API UTireflyStateComponent : public UActorComponent
{
    GENERATED_BODY()

protected:
    // 槽位配置映射
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration")
    TMap<FGameplayTag, FTireflySlotConfiguration> SlotConfigurations;

public:
    // 增强的状态分配函数
    UFUNCTION(BlueprintCallable, Category = "State|StateTree")
    bool TryAssignStateToSlotAdvanced(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag);
    
    // 按优先级获取槽位状态
    UFUNCTION(BlueprintPure, Category = "State|StateTree")
    UTireflyStateInstance* GetSlotStateByPriority(FGameplayTag SlotTag) const;
    
    // 获取槽位中所有状态（按优先级排序）
    UFUNCTION(BlueprintPure, Category = "State|StateTree")
    TArray<UTireflyStateInstance*> GetSortedStatesInSlot(FGameplayTag SlotTag) const;
    
    // 获取槽位配置
    UFUNCTION(BlueprintPure, Category = "State|StateTree")
    FTireflySlotConfiguration GetSlotConfiguration(FGameplayTag SlotTag) const;
    
    // 设置槽位配置
    UFUNCTION(BlueprintCallable, Category = "State|StateTree")
    void SetSlotConfiguration(FGameplayTag SlotTag, const FTireflySlotConfiguration& Configuration);

private:
    // 内部实现函数
    bool ApplySlotPolicy(FGameplayTag SlotTag, UTireflyStateInstance* NewState);
    void SortSlotStatesByPriority(TArray<UTireflyStateInstance*>& SlotStates);
    bool HandleSingleSlotAssignment(TArray<UTireflyStateInstance*>& SlotStates, UTireflyStateInstance* NewState, const FTireflySlotConfiguration& Config);
    bool HandleMultipleSlotAssignment(TArray<UTireflyStateInstance*>& SlotStates, UTireflyStateInstance* NewState, const FTireflySlotConfiguration& Config);
    bool HandlePriorityStackAssignment(TArray<UTireflyStateInstance*>& SlotStates, UTireflyStateInstance* NewState, const FTireflySlotConfiguration& Config);
    bool ApplyStateMergeStrategy(UTireflyStateInstance* ExistingState, UTireflyStateInstance* NewState);
    bool ReplaceLowestPriorityState(TArray<UTireflyStateInstance*>& SlotStates, UTireflyStateInstance* NewState);
};
```

---

## 具体实现方案

### 1. 智能状态分配实现

```cpp
bool UTireflyStateComponent::TryAssignStateToSlotAdvanced(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
        return false;
    }

    // 获取槽位配置，如果没有配置则使用默认值
    FTireflySlotConfiguration SlotConfig;
    if (const FTireflySlotConfiguration* FoundConfig = SlotConfigurations.Find(SlotTag))
    {
        SlotConfig = *FoundConfig;
    }

    TArray<UTireflyStateInstance*>& SlotStates = StateSlots.FindOrAdd(SlotTag);
    
    // 根据槽位策略分派处理
    switch (SlotConfig.SlotPolicy)
    {
    case ETireflySlotPolicy::Single:
        return HandleSingleSlotAssignment(SlotStates, StateInstance, SlotConfig);
        
    case ETireflySlotPolicy::Multiple:
        return HandleMultipleSlotAssignment(SlotStates, StateInstance, SlotConfig);
        
    case ETireflySlotPolicy::PriorityStack:
        return HandlePriorityStackAssignment(SlotStates, StateInstance, SlotConfig);
    }
    
    return false;
}
```

### 2. 单一槽位模式实现

```cpp
bool UTireflyStateComponent::HandleSingleSlotAssignment(
    TArray<UTireflyStateInstance*>& SlotStates, 
    UTireflyStateInstance* NewState,
    const FTireflySlotConfiguration& Config)
{
    const int32 NewPriority = NewState->GetStateDef().Priority;
    
    // 如果槽位为空，直接添加
    if (SlotStates.IsEmpty())
    {
        SlotStates.Add(NewState);
        UpdateStateInstanceIndices(NewState);
        OnStateSlotChanged(SlotTag);
        return true;
    }
    
    // 查找当前激活的状态
    UTireflyStateInstance* CurrentActiveState = nullptr;
    for (UTireflyStateInstance* State : SlotStates)
    {
        if (IsValid(State) && State->GetCurrentStage() == ETireflyStateStage::SS_Active)
        {
            CurrentActiveState = State;
            break;
        }
    }
    
    if (!CurrentActiveState)
    {
        // 没有激活状态，直接添加新状态
        SlotStates.Add(NewState);
        UpdateStateInstanceIndices(NewState);
        OnStateSlotChanged(SlotTag);
        return true;
    }
    
    const int32 CurrentPriority = CurrentActiveState->GetStateDef().Priority;
    
    // 优先级更高（数值更小）则替换
    if (NewPriority < CurrentPriority)
    {
        // 停止并移除旧状态
        CurrentActiveState->StopStateTree();
        CurrentActiveState->SetCurrentStage(ETireflyStateStage::SS_Expired);
        RemoveStateInstance(CurrentActiveState);
        
        // 添加新状态
        SlotStates.Empty();
        SlotStates.Add(NewState);
        UpdateStateInstanceIndices(NewState);
        OnStateSlotChanged(SlotTag);
        
        UE_LOG(LogTcsState, Log, TEXT("State [%s] replaced [%s] in single slot [%s] due to higher priority"),
            *NewState->GetStateDefId().ToString(),
            *CurrentActiveState->GetStateDefId().ToString(),
            *SlotTag.ToString());
            
        return true;
    }
    else if (NewPriority == CurrentPriority)
    {
        // 优先级相同，应用状态合并策略
        return ApplyStateMergeStrategy(CurrentActiveState, NewState);
    }
    
    // 优先级不够，拒绝添加
    UE_LOG(LogTcsState, Verbose, TEXT("State [%s] rejected from single slot [%s] due to lower priority"),
        *NewState->GetStateDefId().ToString(), *SlotTag.ToString());
    
    return false;
}
```

### 3. 多重槽位模式实现

```cpp
bool UTireflyStateComponent::HandleMultipleSlotAssignment(
    TArray<UTireflyStateInstance*>& SlotStates,
    UTireflyStateInstance* NewState,
    const FTireflySlotConfiguration& Config)
{
    // 检查相同定义ID限制
    if (!Config.bAllowSameDefId)
    {
        const FName NewStateDefId = NewState->GetStateDefId();
        for (UTireflyStateInstance* ExistingState : SlotStates)
        {
            if (IsValid(ExistingState) && ExistingState->GetStateDefId() == NewStateDefId)
            {
                // 应用状态合并策略
                return ApplyStateMergeStrategy(ExistingState, NewState);
            }
        }
    }
    
    // 清理无效状态
    SlotStates.RemoveAll([](UTireflyStateInstance* State) {
        return !IsValid(State) || State->GetCurrentStage() == ETireflyStateStage::SS_Expired;
    });
    
    // 检查容量限制
    if (SlotStates.Num() >= Config.MaxCapacity)
    {
        if (Config.bReplaceLowestPriority)
        {
            return ReplaceLowestPriorityState(SlotStates, NewState);
        }
        else
        {
            UE_LOG(LogTcsState, Warning, TEXT("Multiple slot [%s] capacity full, state [%s] rejected"),
                *SlotTag.ToString(), *NewState->GetStateDefId().ToString());
            return false;
        }
    }
    
    // 添加新状态
    SlotStates.Add(NewState);
    UpdateStateInstanceIndices(NewState);
    
    // 按优先级排序
    if (Config.bSortByPriority)
    {
        SortSlotStatesByPriority(SlotStates);
    }
    
    OnStateSlotChanged(SlotTag);
    
    UE_LOG(LogTcsState, Log, TEXT("State [%s] added to multiple slot [%s], total states: %d"),
        *NewState->GetStateDefId().ToString(), *SlotTag.ToString(), SlotStates.Num());
    
    return true;
}
```

### 4. 优先级栈模式实现

```cpp
bool UTireflyStateComponent::HandlePriorityStackAssignment(
    TArray<UTireflyStateInstance*>& SlotStates,
    UTireflyStateInstance* NewState,
    const FTireflySlotConfiguration& Config)
{
    const int32 NewPriority = NewState->GetStateDef().Priority;
    
    // 添加新状态到槽位
    SlotStates.Add(NewState);
    UpdateStateInstanceIndices(NewState);
    
    // 按优先级排序（优先级越小越靠前）
    SortSlotStatesByPriority(SlotStates);
    
    // 管理状态的激活/挂起状态
    for (int32 i = 0; i < SlotStates.Num(); ++i)
    {
        UTireflyStateInstance* State = SlotStates[i];
        if (!IsValid(State)) continue;
        
        if (i == 0)
        {
            // 最高优先级状态应该激活
            if (State->GetCurrentStage() == ETireflyStateStage::SS_HangUp)
            {
                State->SetCurrentStage(ETireflyStateStage::SS_Active);
                // 恢复StateTree执行
                if (!State->IsStateTreeRunning())
                {
                    State->StartStateTree();
                }
                
                UE_LOG(LogTcsState, Log, TEXT("State [%s] resumed from suspension in priority stack [%s]"),
                    *State->GetStateDefId().ToString(), *SlotTag.ToString());
            }
        }
        else
        {
            // 非最高优先级状态应该挂起
            if (State->GetCurrentStage() == ETireflyStateStage::SS_Active)
            {
                State->SetCurrentStage(ETireflyStateStage::SS_HangUp);
                
                // 根据配置决定是否停止StateTree
                if (!Config.bTickSuspendedStates && State->IsStateTreeRunning())
                {
                    State->StopStateTree();
                }
                
                UE_LOG(LogTcsState, Log, TEXT("State [%s] suspended by higher priority state in stack [%s]"),
                    *State->GetStateDefId().ToString(), *SlotTag.ToString());
            }
        }
    }
    
    OnStateSlotChanged(SlotTag);
    return true;
}
```

### 5. 辅助函数实现

```cpp
void UTireflyStateComponent::SortSlotStatesByPriority(TArray<UTireflyStateInstance*>& SlotStates)
{
    SlotStates.Sort([](const UTireflyStateInstance& A, const UTireflyStateInstance& B) {
        // 优先级数值越小，优先级越高
        return A.GetStateDef().Priority < B.GetStateDef().Priority;
    });
}

bool UTireflyStateComponent::ReplaceLowestPriorityState(
    TArray<UTireflyStateInstance*>& SlotStates, 
    UTireflyStateInstance* NewState)
{
    const int32 NewPriority = NewState->GetStateDef().Priority;
    
    // 找到优先级最低（数值最大）的状态
    UTireflyStateInstance* LowestPriorityState = nullptr;
    int32 LowestPriority = INT32_MIN;
    
    for (UTireflyStateInstance* State : SlotStates)
    {
        if (IsValid(State))
        {
            const int32 StatePriority = State->GetStateDef().Priority;
            if (StatePriority > LowestPriority)
            {
                LowestPriority = StatePriority;
                LowestPriorityState = State;
            }
        }
    }
    
    // 如果新状态优先级更高，替换最低优先级状态
    if (LowestPriorityState && NewPriority < LowestPriority)
    {
        // 移除最低优先级状态
        LowestPriorityState->StopStateTree();
        LowestPriorityState->SetCurrentStage(ETireflyStateStage::SS_Expired);
        SlotStates.Remove(LowestPriorityState);
        RemoveStateInstance(LowestPriorityState);
        
        // 添加新状态
        SlotStates.Add(NewState);
        UpdateStateInstanceIndices(NewState);
        
        UE_LOG(LogTcsState, Log, TEXT("State [%s] replaced [%s] in multiple slot due to higher priority"),
            *NewState->GetStateDefId().ToString(),
            *LowestPriorityState->GetStateDefId().ToString());
        
        return true;
    }
    
    return false;
}

UTireflyStateInstance* UTireflyStateComponent::GetSlotStateByPriority(FGameplayTag SlotTag) const
{
    const TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag);
    if (!SlotStates || SlotStates->IsEmpty())
    {
        return nullptr;
    }
    
    // 找到优先级最高的激活状态
    UTireflyStateInstance* HighestPriorityState = nullptr;
    int32 HighestPriority = INT32_MAX;
    
    for (UTireflyStateInstance* State : *SlotStates)
    {
        if (IsValid(State) && State->GetCurrentStage() == ETireflyStateStage::SS_Active)
        {
            const int32 StatePriority = State->GetStateDef().Priority;
            if (StatePriority < HighestPriority)
            {
                HighestPriority = StatePriority;
                HighestPriorityState = State;
            }
        }
    }
    
    return HighestPriorityState;
}

TArray<UTireflyStateInstance*> UTireflyStateComponent::GetSortedStatesInSlot(FGameplayTag SlotTag) const
{
    TArray<UTireflyStateInstance*> Result;
    
    const TArray<UTireflyStateInstance*>* SlotStates = StateSlots.Find(SlotTag);
    if (!SlotStates)
    {
        return Result;
    }
    
    // 收集所有有效的激活状态
    for (UTireflyStateInstance* State : *SlotStates)
    {
        if (IsValid(State) && State->GetCurrentStage() == ETireflyStateStage::SS_Active)
        {
            Result.Add(State);
        }
    }
    
    // 按优先级排序
    Result.Sort([](const UTireflyStateInstance& A, const UTireflyStateInstance& B) {
        return A.GetStateDef().Priority < B.GetStateDef().Priority;
    });
    
    return Result;
}
```

---

## 使用示例

### 示例1: Action 槽位配置

```cpp
// 在组件初始化时配置Action槽位
void UTireflyStateComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 配置Action槽位为单一状态模式
    FTireflySlotConfiguration ActionSlotConfig;
    ActionSlotConfig.SlotPolicy = ETireflySlotPolicy::Single;
    ActionSlotConfig.bSortByPriority = true;
    
    SetSlotConfiguration(
        FGameplayTag::RequestGameplayTag("StateSlot.Action"), 
        ActionSlotConfig
    );
}

// 状态定义示例
AttackState.StateSlotType = FGameplayTag::RequestGameplayTag("StateSlot.Action");
AttackState.Priority = 10;  // 攻击优先级

HitReactState.StateSlotType = FGameplayTag::RequestGameplayTag("StateSlot.Action");  
HitReactState.Priority = 5;   // 受击优先级更高

InteractState.StateSlotType = FGameplayTag::RequestGameplayTag("StateSlot.Action");
InteractState.Priority = 15;  // 交互优先级最低
```

### 示例2: Debuff 槽位配置

```cpp
// 配置Debuff槽位为多重状态模式
FTireflySlotConfiguration DebuffSlotConfig;
DebuffSlotConfig.SlotPolicy = ETireflySlotPolicy::Multiple;
DebuffSlotConfig.MaxCapacity = 8;
DebuffSlotConfig.bAllowSameDefId = true;  // 允许多个相同类型的异常状态
DebuffSlotConfig.bSortByPriority = false; // 不需要排序
DebuffSlotConfig.bReplaceLowestPriority = true;

SetSlotConfiguration(
    FGameplayTag::RequestGameplayTag("StateSlot.Debuff"), 
    DebuffSlotConfig
);

// 使用示例：同时应用多种异常状态
TArray<UTireflyStateInstance*> DebuffStates = GetActiveStatesInSlot(
    FGameplayTag::RequestGameplayTag("StateSlot.Debuff")
);

// 结果可能包含：中毒、燃烧、冰冻、沉默等多种状态
for (UTireflyStateInstance* State : DebuffStates)
{
    UE_LOG(LogGame, Log, TEXT("Active debuff: %s"), *State->GetStateDefId().ToString());
}
```

### 示例3: Mobility 槽位配置

```cpp
// 配置Mobility槽位为优先级栈模式
FTireflySlotConfiguration MobilitySlotConfig;
MobilitySlotConfig.SlotPolicy = ETireflySlotPolicy::PriorityStack;
MobilitySlotConfig.bSortByPriority = true;
MobilitySlotConfig.bTickSuspendedStates = false;  // 挂起状态不继续Tick

SetSlotConfiguration(
    FGameplayTag::RequestGameplayTag("StateSlot.Mobility"), 
    MobilitySlotConfig
);

// 状态应用示例
SpeedBoostState.Priority = 10;    // 加速状态
SlowState.Priority = 8;           // 减速状态  
RootState.Priority = 5;           // 定身状态（优先级最高）

// 当定身状态应用时，加速状态会被挂起而不是移除
// 当定身状态结束时，加速状态会自动恢复
```

### 示例4: 蓝图中的使用

```cpp
// 蓝图可调用的便利函数
UFUNCTION(BlueprintCallable, Category = "State|Utility", CallInEditor = true)
void ConfigureCommonSlots()
{
    // Action槽位：单一状态，按优先级替换
    FTireflySlotConfiguration ActionConfig;
    ActionConfig.SlotPolicy = ETireflySlotPolicy::Single;
    SetSlotConfiguration(FGameplayTag::RequestGameplayTag("StateSlot.Action"), ActionConfig);
    
    // Debuff槽位：多重状态，容量限制
    FTireflySlotConfiguration DebuffConfig;
    DebuffConfig.SlotPolicy = ETireflySlotPolicy::Multiple;
    DebuffConfig.MaxCapacity = 10;
    SetSlotConfiguration(FGameplayTag::RequestGameplayTag("StateSlot.Debuff"), DebuffConfig);
    
    // Buff槽位：多重状态，允许相同ID
    FTireflySlotConfiguration BuffConfig;
    BuffConfig.SlotPolicy = ETireflySlotPolicy::Multiple;
    BuffConfig.bAllowSameDefId = true;
    SetSlotConfiguration(FGameplayTag::RequestGameplayTag("StateSlot.Buff"), BuffConfig);
    
    // Mobility槽位：优先级栈
    FTireflySlotConfiguration MobilityConfig;
    MobilityConfig.SlotPolicy = ETireflySlotPolicy::PriorityStack;
    SetSlotConfiguration(FGameplayTag::RequestGameplayTag("StateSlot.Mobility"), MobilityConfig);
}
```

---

## 迁移指南

### 步骤1: 更新现有代码

1. **替换函数调用**：
```cpp
// 旧版本
StateComponent->TryAssignStateToSlot(StateInstance, SlotTag);

// 新版本  
StateComponent->TryAssignStateToSlotAdvanced(StateInstance, SlotTag);
```

2. **添加槽位配置**：
```cpp
// 在BeginPlay中配置所有使用的槽位
void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    
    if (StateComponent)
    {
        StateComponent->ConfigureCommonSlots(); // 或手动配置
    }
}
```

### 步骤2: 兼容性考虑

为了保持向后兼容，建议：

1. 保留原有的`TryAssignStateToSlot`函数，内部调用新函数
2. 为没有配置的槽位提供合理的默认行为
3. 添加配置验证和警告日志

```cpp
// 向后兼容的包装函数
bool UTireflyStateComponent::TryAssignStateToSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag)
{
    // 如果没有配置，使用默认的Multiple模式以保持兼容性
    if (!SlotConfigurations.Contains(SlotTag))
    {
        UE_LOG(LogTcsState, Warning, TEXT("Slot [%s] not configured, using default Multiple mode"), *SlotTag.ToString());
        
        FTireflySlotConfiguration DefaultConfig;
        DefaultConfig.SlotPolicy = ETireflySlotPolicy::Multiple;
        SetSlotConfiguration(SlotTag, DefaultConfig);
    }
    
    return TryAssignStateToSlotAdvanced(StateInstance, SlotTag);
}
```

### 步骤3: 测试验证

1. **功能测试**：确保各种槽位策略按预期工作
2. **性能测试**：验证新的排序和查找逻辑不会显著影响性能
3. **边界测试**：测试极限情况（容量满、优先级相同等）

---

## 总结

本改进方案通过引入槽位策略配置系统，完美解决了 TCS 当前状态槽系统的设计限制：

### ✅ 解决的问题

1. **槽位容量控制**：支持单一状态、多重状态、优先级栈等不同模式
2. **优先级管理**：智能的优先级排序和替换机制  
3. **策略配置**：灵活的槽位行为配置，满足不同业务需求
4. **向后兼容**：保持现有API的兼容性

### 🚀 主要优势

1. **设计师友好**：数据驱动的配置方式，无需代码修改
2. **高度灵活**：支持复杂的状态管理场景
3. **性能优化**：智能的状态查找和排序算法
4. **易于维护**：清晰的架构设计和完整的文档

### 📈 业务价值

- **Action槽位**：完美支持战斗状态的优先级控制
- **Debuff槽位**：灵活的异常状态共存管理
- **Mobility槽位**：智能的移动状态栈管理
- **扩展性**：轻松支持未来的新业务需求

通过实施这个改进方案，TCS 将具备业界领先的状态管理能力，为复杂的游戏战斗系统提供强大而灵活的技术支撑。

---

**文档版本**: 1.0  
**创建日期**: 2024年  
**最后更新**: 2024年  
**作者**: TCS开发团队