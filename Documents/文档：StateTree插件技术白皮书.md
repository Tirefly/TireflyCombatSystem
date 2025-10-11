# UE5 StateTree & GameplayStateTree 完全精通指导手册

## 概述

本文档是基于源码深度分析的 StateTree 和 GameplayStateTree 插件完全精通指导手册。StateTree 是 Epic Games 开发的通用分层状态机系统，GameplayStateTree 则是专门针对 AI/游戏行为的扩展插件。

### 插件概览

- **StateTree** (v0.1): 通用分层状态机系统，提供核心功能
- **GameplayStateTree** (v1.0): AI/游戏行为专用扩展，依赖 StateTree 核心插件

## 第一部分：架构深度解析

### 1.1 核心模块架构

#### StateTree 核心插件结构
```
StateTree/
├── StateTreeModule/              # 运行时核心 (Runtime)
├── StateTreeEditorModule/        # 编辑器支持 (UncookedOnly)  
└── StateTreeTestSuite/          # 测试套件 (UncookedOnly)
```

#### GameplayStateTree 扩展插件结构
```
GameplayStateTree/
└── GameplayStateTreeModule/     # AI/游戏行为运行时 (Runtime)
```

#### 依赖关系图
```
GameplayStateTreeModule 
    ↓ (depends on)
StateTreeModule ← StateTreeEditorModule ← StateTreeTestSuite
    ↓ (enables)
PropertyBindingUtils, PropertyAccessEditor, GameplayInsights
```

### 1.2 核心类层次结构

#### 节点基类继承树
```
FStateTreeNodeBase (抽象基类)
├── FStateTreeTaskBase (任务基类)
│   ├── FStateTreeTaskCommonBase (通用任务)
│   └── [自定义任务实现]
├── FStateTreeConditionBase (条件基类)
│   ├── FStateTreeConditionCommonBase (通用条件)
│   └── [自定义条件实现]
├── FStateTreeEvaluatorBase (评估器基类)
│   ├── FStateTreeEvaluatorCommonBase (通用评估器)
│   └── [自定义评估器实现]
└── FStateTreeConsiderationBase (考虑器基类)
```

#### 核心数据结构
```
UStateTree (主要资产类)
├── FStateTreeExecutionContext (执行上下文)
├── FStateTreeInstanceData (实例数据)
├── FStateTreePropertyBindings (属性绑定)
├── FCompactStateTreeState[] (运行时状态)
├── FCompactStateTreeFrame[] (运行时帧)
└── FInstancedStructContainer (节点容器)
```

### 1.3 执行上下文继承层次

```
FStateTreeReadOnlyExecutionContext (只读基类)
    ↓
FStateTreeMinimalExecutionContext (最小功能)
    ↓  
FStateTreeExecutionContext (完整功能)
    ↓
FStateTreeWeakExecutionContext (弱引用版本)
```

## 第二部分：核心系统详解

### 2.1 StateTree 资产系统

#### 资产结构组成
```cpp
class UStateTree : public UDataAsset
{
    // 运行时编译数据
    TArray<FCompactStateTreeFrame> Frames;         // 运行时帧
    TArray<FCompactStateTreeState> States;         // 运行时状态
    TArray<FCompactStateTransition> Transitions;   // 运行时转换
    FInstancedStructContainer Nodes;               // 节点容器
    
    // 实例数据
    FStateTreeInstanceData DefaultInstanceData;    // 默认实例数据
    FStateTreeInstanceData SharedInstanceData;     // 共享实例数据
    
    // 属性绑定与外部数据
    FStateTreePropertyBindings PropertyBindings;   // 属性绑定
    TArray<FStateTreeExternalDataDesc> ExternalDataDescs;
    TArray<FStateTreeExternalDataDesc> ContextDataDescs;
    
    // 参数系统
    FInstancedPropertyBag Parameters;              // 默认参数
    
    // 版本与链接信息
    uint32 LastCompiledEditorDataHash;            // 编译哈希
    bool bIsLinked;                                // 链接状态
};
```

#### 状态树生命周期
1. **编译期** (Compile): 编辑器数据 → 运行时数据
2. **链接期** (Link): 解析引用，验证数据完整性
3. **运行期** (Runtime): 执行状态逻辑

### 2.2 执行上下文系统

#### FStateTreeExecutionContext 核心功能

```cpp
struct FStateTreeExecutionContext : public FStateTreeMinimalExecutionContext
{
    // 生命周期管理
    EStateTreeRunStatus Start(FStartParameters Parameters);
    EStateTreeRunStatus Tick(float DeltaTime);
    EStateTreeRunStatus Stop(EStateTreeRunStatus CompletionStatus);
    
    // 数据访问
    template<typename T> T& GetExternalData(TStateTreeExternalDataHandle<T> Handle);
    template<typename T> T& GetInstanceData(const FStateTreeNodeBase& Node);
    
    // 事件系统
    void SendEvent(FGameplayTag Tag, FConstStructView Payload);
    void ForEachEvent(TFunc&& Function);
    
    // 转换请求
    void RequestTransition(FStateTreeStateHandle TargetState);
    void FinishTask(const FStateTreeTaskBase& Task, EStateTreeFinishTaskType Type);
    
    // 调试支持
    TArray<FName> GetActiveStateNames() const;
    EStateTreeRunStatus GetStateTreeRunStatus() const;
};
```

#### 上下文数据管理模式

```cpp
// 1. 设置上下文数据回调
Context.SetCollectExternalDataCallback(
    FOnCollectStateTreeExternalData::CreateUObject(
        this, &UMyComponent::CollectExternalData
    )
);

// 2. 设置命名上下文数据  
Context.SetContextDataByName(TEXT("Actor"), FStateTreeDataView(OwnerActor));

// 3. 验证上下文有效性
bool bValid = Context.AreContextDataViewsValid();
```

### 2.3 节点系统深度解析

#### 节点基类功能矩阵

| 节点类型 | 执行时机 | 主要方法 | 实例数据共享 | 属性绑定 |
|----------|----------|----------|--------------|----------|
| **Task** | 状态激活期间 | EnterState, Tick, ExitState, TriggerTransitions | 否 (独立) | 是 |
| **Condition** | 转换评估时 | TestCondition, EnterState, ExitState | 是 (共享) | 是 |
| **Evaluator** | 全局/每帧 | TreeStart, Tick, TreeStop | 否 (独立) | 是 |
| **Consideration** | 选择评估时 | CalculateScore | 是 (共享) | 是 |

#### 任务 (Task) 详细配置
```cpp
struct FStateTreeTaskBase : public FStateTreeNodeBase
{
    // 行为控制标志
    uint8 bShouldStateChangeOnReselect : 1;        // 重选时是否调用状态变化
    uint8 bShouldCallTick : 1;                     // 是否调用 Tick
    uint8 bShouldCallTickOnlyOnEvents : 1;         // 仅在事件时 Tick
    uint8 bShouldCopyBoundPropertiesOnTick : 1;    // Tick 时复制绑定属性
    uint8 bShouldCopyBoundPropertiesOnExitState : 1; // 退出时复制属性
    uint8 bShouldAffectTransitions : 1;            // 是否影响转换
    uint8 bConsideredForScheduling : 1;            // 是否考虑调度
    uint8 bTaskEnabled : 1;                        // 任务是否启用
    
    // 转换优先级
    EStateTreeTransitionPriority TransitionHandlingPriority;
};
```

#### 条件 (Condition) 评估模式
```cpp
enum class EStateTreeConditionEvaluationMode : uint8
{
    Evaluated,  // 正常评估 - 每次转换时计算
    Cached,     // 缓存模式 - 使用之前的结果  
    Disabled    // 禁用模式 - 始终返回 true
};
```

### 2.4 属性绑定系统

#### 绑定源类型层次
```cpp
enum class EStateTreeBindableStructSource : uint8
{
    Context,            // 上下文对象 (Schema 定义)
    Parameter,          // StateTree 参数
    Evaluator,          // 全局评估器
    GlobalTask,         // 全局任务
    StateParameter,     // 状态参数
    Task,              // 状态任务
    Condition,         // 状态条件
    Consideration,     // 效用考虑器
    TransitionEvent,   // 转换事件
    StateEvent,        // 状态选择事件
    PropertyFunction,  // 属性函数
    Transition,        // 转换
};
```

#### 属性绑定流程
```cpp
// 1. 编译期：创建绑定描述
FStateTreePropertyPathBinding Binding(SourcePath, TargetPath);

// 2. 链接期：解析为复制信息
FPropertyBindingCopyInfo CopyInfo = ResolvePath(Binding);

// 3. 运行期：执行属性复制
bool Success = CopyBatch(Context, TargetView, BindingsBatch);
```

### 2.5 事件驱动系统

#### 事件类型与生命周期
```cpp
struct FStateTreeEvent
{
    FGameplayTag Tag;           // 事件标签
    FConstStructView Payload;   // 事件载荷
    FName Origin;              // 事件来源
    float Timestamp;           // 时间戳
};

// 事件队列管理
class FStateTreeEventQueue
{
    void SendEvent(const FStateTreeEvent& Event);
    void ConsumeEvent(const FStateTreeSharedEvent& Event);
    TConstArrayView<FStateTreeSharedEvent> GetEventsView() const;
};
```

#### 事件处理模式
1. **转换事件**: 用于状态转换的触发条件
2. **状态选择事件**: 影响状态选择逻辑
3. **任务事件**: 任务内部通信机制

## 第三部分：GameplayStateTree 专业特性

### 3.1 组件系统架构

#### UStateTreeComponent 继承链
```
UBrainComponent (AI基础组件)
    ↓
UStateTreeComponent (通用StateTree组件)
    ↓  
UStateTreeAIComponent (AI专用组件)
```

#### 核心组件功能对比

| 特性 | UStateTreeComponent | UStateTreeAIComponent |
|------|-------------------|----------------------|
| **目标用途** | 通用游戏逻辑 | AI控制器专用 |
| **Schema** | StateTreeComponentSchema | StateTreeAIComponentSchema |
| **上下文保证** | Actor | AIController + Pawn |
| **BrainComponent集成** | 是 | 是 |
| **GameplayTask支持** | 是 | 是 |

### 3.2 AI集成特性

#### 行为树集成
```cpp
// BTTask_RunStateTree - 在行为树中运行StateTree
UCLASS()
class UBTTask_RunStateTree : public UBTTaskNode
{
    UPROPERTY(EditAnywhere)
    FStateTreeReference StateTreeRef;
    
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, 
                                           uint8* NodeMemory) override;
};

// BTTask_RunDynamicStateTree - 运行动态StateTree
UCLASS() 
class UBTTask_RunDynamicStateTree : public UBTTaskNode
{
    UPROPERTY(EditAnywhere)
    FBlackboardKeySelector StateTreeKey;  // 从黑板获取StateTree
};
```

#### AI专用任务示例
```cpp
// StateTreeMoveToTask - AI移动任务
USTRUCT()
struct FStateTreeMoveToTask : public FStateTreeTaskBase
{
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, 
                                          const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, 
                                    float DeltaTime) const override;
};

// StateTreeRunEnvQueryTask - 环境查询任务  
USTRUCT()
struct FStateTreeRunEnvQueryTask : public FStateTreeTaskBase
{
    UPROPERTY(EditAnywhere)
    TObjectPtr<UEnvQuery> QueryTemplate;
    
    UPROPERTY(EditAnywhere) 
    TArray<FEnvNamedValue> QueryParams;
};
```

### 3.3 Schema 系统

#### StateTreeComponentSchema vs StateTreeAIComponentSchema

```cpp
// StateTreeComponentSchema - 通用组件Schema
class UStateTreeComponentSchema : public UStateTreeSchema
{
    // 保证的上下文数据
    // - Actor: 组件所属的Actor
    // - StateTreeComponent: 组件自身
    // - World: 当前世界
};

// StateTreeAIComponentSchema - AI专用Schema  
class UStateTreeAIComponentSchema : public UStateTreeSchema
{
    // 额外保证的AI上下文数据
    // - AIController: AI控制器
    // - Pawn: 被控制的Pawn
    // - Blackboard: 黑板组件 
    // - BehaviorTree: 行为树组件
};
```

## 第四部分：高级使用模式

### 4.1 直接执行上下文使用 (无组件模式)

#### 完整实现示例
```cpp
UCLASS()
class UMyStateTreeManager : public UObject
{
    GENERATED_BODY()

public:
    // StateTree资产与实例数据
    UPROPERTY(EditAnywhere, Category = "StateTree")
    FStateTreeReference StateTreeRef;
    
    FStateTreeInstanceData InstanceData;
    bool bIsRunning = false;

    // 生命周期管理
    bool InitializeStateTree();
    void StartStateTree();  
    void TickStateTree(float DeltaTime);
    void StopStateTree();
    void CleanupStateTree();

protected:
    // 核心集成方法
    bool SetContextRequirements(FStateTreeExecutionContext& Context);
    bool CollectExternalData(const FStateTreeExecutionContext& Context,
                           const UStateTree* StateTree,
                           TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
                           TArrayView<FStateTreeDataView> OutDataViews);
};

// 实现关键方法
bool UMyStateTreeManager::SetContextRequirements(FStateTreeExecutionContext& Context)
{
    if (!Context.IsValid()) return false;
    
    // 设置外部数据收集回调
    Context.SetCollectExternalDataCallback(
        FOnCollectStateTreeExternalData::CreateUObject(
            this, &UMyStateTreeManager::CollectExternalData
        )
    );
    
    // 设置上下文数据
    if (AActor* OwnerActor = GetTypedOuter<AActor>())
    {
        Context.SetContextDataByName(TEXT("Actor"), FStateTreeDataView(OwnerActor));
    }
    
    return Context.AreContextDataViewsValid();
}

bool UMyStateTreeManager::CollectExternalData(
    const FStateTreeExecutionContext& Context,
    const UStateTree* StateTree,
    TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
    TArrayView<FStateTreeDataView> OutDataViews)
{
    UWorld* World = GetWorld();
    if (!World) return false;
    
    for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
    {
        const FStateTreeExternalDataDesc& Desc = ExternalDataDescs[Index];
        FStateTreeDataView& DataView = OutDataViews[Index];
        
        // 根据数据类型提供相应数据
        if (Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
        {
            UWorldSubsystem* Subsystem = World->GetSubsystemBase(
                Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get()))
            );
            DataView = FStateTreeDataView(Subsystem);
        }
        else if (Desc.Struct->IsChildOf(AActor::StaticClass()))
        {
            if (AActor* OwnerActor = GetTypedOuter<AActor>())
            {
                DataView = FStateTreeDataView(OwnerActor);
            }
        }
        // ... 其他数据类型处理
    }
    
    return true;
}
```

### 4.2 高级事件处理模式

#### 事件生产者-消费者模式
```cpp
// 事件生产者
class UEventProducerComponent : public UActorComponent
{
public:
    void TriggerGameplayEvent(FGameplayTag EventTag, FConstStructView Payload)
    {
        if (UStateTreeComponent* StateTreeComp = GetOwner()->FindComponentByClass<UStateTreeComponent>())
        {
            StateTreeComp->SendStateTreeEvent(EventTag, Payload, GetFName());
        }
    }
};

// 事件响应Task示例
USTRUCT()
struct FEventResponseTask : public FStateTreeTaskBase  
{
    UPROPERTY(EditAnywhere)
    FGameplayTag TriggerEventTag;
    
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override
    {
        // 检查是否有目标事件
        Context.ForEachEvent([this](const FStateTreeSharedEvent& Event) -> EStateTreeLoopEvents
        {
            if (Event->Tag.MatchesTag(TriggerEventTag))
            {
                // 处理事件
                HandleEvent(Event);
                return EStateTreeLoopEvents::Break;
            }
            return EStateTreeLoopEvents::Continue;
        });
        
        return EStateTreeRunStatus::Running;
    }
};
```

### 4.3 链接状态树 (Linked StateTree) 高级用法

#### 动态状态树切换
```cpp
// 链接状态树覆盖配置
FStateTreeReferenceOverrides LinkedOverrides;
LinkedOverrides.Overrides.Add(FGameplayTag::RequestGameplayTag("Combat.Melee"), 
                             FStateTreeReference(MeleeStateTree));
LinkedOverrides.Overrides.Add(FGameplayTag::RequestGameplayTag("Combat.Ranged"), 
                             FStateTreeReference(RangedStateTree));

// 运行时设置覆盖
StateTreeComponent->SetLinkedStateTreeOverrides(LinkedOverrides);

// 在StateTree中通过Tag引用链接状态
// 编辑器中创建 "Linked Asset State"，设置StateTag为 "Combat.Melee"
// 运行时会自动使用MeleeStateTree替换
```

### 4.4 调度Tick系统优化

#### 智能Tick调度配置
```cpp
USTRUCT()
struct FSmartTickTask : public FStateTreeTaskBase
{
    FSmartTickTask()
    {
        // 配置智能Tick行为
        bShouldCallTick = true;                    // 启用Tick
        bShouldCallTickOnlyOnEvents = false;      // 不仅限事件时Tick
        bConsideredForScheduling = true;          // 参与调度计算
        bShouldCopyBoundPropertiesOnTick = true;  // Tick时更新绑定
    }
    
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override
    {
        // 条件性请求下次Tick
        if (ShouldContinueTicking())
        {
            // 使用调度Tick请求特定频率
            UE::StateTree::FScheduledTickHandle TickHandle = 
                Context.AddScheduledTickRequest(FStateTreeScheduledTick::Frequency(2.0f)); // 2Hz
            return EStateTreeRunStatus::Running;
        }
        
        return EStateTreeRunStatus::Succeeded;
    }
};
```

## 第五部分：调试与性能优化

### 5.1 调试系统详解

#### StateTree Trace 系统
```cpp
// 启用追踪 (需要 WITH_STATETREE_TRACE)
#if WITH_STATETREE_TRACE
// 在Task中添加自定义调试信息
SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("CustomInfo: %s"), *CustomValue);

// 获取调试ID用于工具集成
FStateTreeInstanceDebugId DebugId = Context.GetInstanceDebugId();
#endif

// 运行时调试信息获取
#if WITH_GAMEPLAY_DEBUGGER
FString DebugInfo = StateTreeComponent->GetDebugInfoString();
TArray<FName> ActiveStates = StateTreeComponent->GetActiveStateNames();
#endif
```

#### 内存使用分析
```cpp
#if WITH_EDITOR
// 获取内存使用统计
TArray<FStateTreeMemoryUsage> MemoryUsage = StateTree->CalculateEstimatedMemoryUsage();

for (const FStateTreeMemoryUsage& Usage : MemoryUsage)
{
    UE_LOG(LogTemp, Log, TEXT("State: %s, Memory: %d bytes, Nodes: %d"), 
           *Usage.Name, Usage.EstimatedMemoryUsage, Usage.NodeCount);
}
#endif
```

### 5.2 性能优化最佳实践

#### 1. 实例数据优化
```cpp
// 使用POD标记减少构造开销
USTRUCT()
struct FOptimizedTaskData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere)
    float Value = 0.0f;
    
    UPROPERTY(EditAnywhere)  
    int32 Count = 0;
};

// 标记为POD类型
STATETREE_POD_INSTANCEDATA(FOptimizedTaskData);
```

#### 2. 条件评估优化
```cpp
USTRUCT()
struct FOptimizedCondition : public FStateTreeConditionBase
{
    FOptimizedCondition()
    {
        // 使用缓存模式减少重复计算
        EvaluationMode = EStateTreeConditionEvaluationMode::Cached;
    }
    
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override
    {
        // 快速路径检查
        if (FastPathCheck()) return true;
        
        // 昂贵的计算逻辑
        return ExpensiveCheck(Context);
    }
};
```

#### 3. 属性绑定优化
```cpp
// 减少不必要的属性复制
USTRUCT()
struct FEfficientTask : public FStateTreeTaskBase
{
    FEfficientTask()
    {
        // 仅在需要时复制属性
        bShouldCopyBoundPropertiesOnTick = false;      // Tick时不复制
        bShouldCopyBoundPropertiesOnExitState = true;  // 退出时复制结果
    }
};
```

## 第六部分：扩展开发指南

### 6.1 自定义节点开发

#### 自定义Task开发模板
```cpp
// 1. 定义实例数据结构
USTRUCT()
struct FMyCustomTaskInstanceData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, Category = "Input")
    float Duration = 1.0f;
    
    UPROPERTY(EditAnywhere, Category = "Input") 
    FString Message;
    
    // 运行时状态 (不暴露给编辑器)
    float ElapsedTime = 0.0f;
    bool bCompleted = false;
};

// 2. 定义Task类
USTRUCT()
struct FMyCustomTask : public FStateTreeTaskBase
{
    GENERATED_BODY()
    
    using FInstanceDataType = FMyCustomTaskInstanceData;
    
    virtual const UStruct* GetInstanceDataType() const override { return FMyCustomTaskInstanceData::StaticStruct(); }
    
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    
#if WITH_EDITOR
    virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

// 3. 实现Task逻辑
EStateTreeRunStatus FMyCustomTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FMyCustomTaskInstanceData& InstanceData = Context.GetInstanceData(*this);
    
    // 初始化运行时状态
    InstanceData.ElapsedTime = 0.0f;
    InstanceData.bCompleted = false;
    
    UE_LOG(LogTemp, Log, TEXT("Starting task: %s"), *InstanceData.Message);
    
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMyCustomTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FMyCustomTaskInstanceData& InstanceData = Context.GetInstanceData(*this);
    
    InstanceData.ElapsedTime += DeltaTime;
    
    if (InstanceData.ElapsedTime >= InstanceData.Duration)
    {
        InstanceData.bCompleted = true;
        return EStateTreeRunStatus::Succeeded;
    }
    
    return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FMyCustomTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
    const FMyCustomTaskInstanceData* Data = InstanceDataView.GetPtr<FMyCustomTaskInstanceData>();
    if (!Data) return FText::GetEmpty();
    
    if (Formatting == EStateTreeNodeFormatting::RichText)
    {
        return FText::FromString(FString::Printf(TEXT("<b>My Custom Task</> Duration: %.1fs, Message: %s"), 
                                               Data->Duration, *Data->Message));
    }
    
    return FText::FromString(FString::Printf(TEXT("My Custom Task (%.1fs): %s"), Data->Duration, *Data->Message));
}
#endif
```

### 6.2 自定义Schema开发

#### 自定义Schema示例
```cpp
UCLASS()
class UMyGameStateTreeSchema : public UStateTreeSchema
{
    GENERATED_BODY()

public:
    UMyGameStateTreeSchema();

protected:
    virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
    virtual bool IsClassAllowed(const UClass* InClass) const override;
    virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
    virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;

private:
    mutable TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};

UMyGameStateTreeSchema::UMyGameStateTreeSchema()
{
    // 定义Schema保证的上下文数据, 官方版本现在FStateTreeExternalDataDesc的第三个参数为FGuid
    
    // 第一种定义方法：直接使用FGuid::NewGuid()
    ContextDataDescs.Add(FStateTreeExternalDataDesc(FName("GameMode"), AGameModeBase::StaticClass(), FGuid::NewGuid()));
    // 第二种定义方法：手动设置固定的FGuid
    ContextDataDescs.Add(FStateTreeExternalDataDesc(FName("PlayerController"), APlayerController::StaticClass(), FGuid(0x12345678, 0x9ABCDEF0, 0x12345678, 0x9ABCDEF0)));
    // 第三种定义方法：如果不想设置FGuid，可以按照如下格式初始化一个ContextrDataDesc
    ContextDataDescs.Add(FStateTreeExternalDataDesc(
		AGameStateBase::StaticClass(),
		EStateTreeExternalDataRequirement::Required
	));
    // 然后单独设置Name                               
	ContextDataDescs.Last().Name = FName("GameState");
}

bool UMyGameStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
    // 只允许继承自特定基类的节点
    return InScriptStruct && InScriptStruct->IsChildOf(FMyGameTaskBase::StaticStruct());
}

TConstArrayView<FStateTreeExternalDataDesc> UMyGameStateTreeSchema::GetContextDataDescs() const
{
    return ContextDataDescs;
}
```

### 6.3 蓝图集成扩展

#### 自定义蓝图Task基类
```cpp
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, CollapseCategories)
class UMyGameStateTreeTaskBlueprintBase : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

public:
    UMyGameStateTreeTaskBlueprintBase(const FObjectInitializer& ObjectInitializer);

protected:
    // 蓝图可实现的事件
    UFUNCTION(BlueprintImplementableEvent, Category = "StateTree", meta = (DisplayName = "On Game Event"))
    EStateTreeRunStatus ReceiveGameEvent(const FGameplayTag& EventTag, const FMyGameEventData& EventData);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "StateTree")
    bool CanExecuteTask() const;
    
    // C++辅助方法
    UFUNCTION(BlueprintCallable, Category = "StateTree")
    void BroadcastGameEvent(FGameplayTag EventTag, const FMyGameEventData& EventData);
    
    // 覆盖基类方法，集成自定义逻辑
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;
};
```

## 第七部分：实际应用案例

### 7.1 AI行为系统案例

#### 复杂AI行为StateTree结构
```
RootState
├── IdleState
│   ├── Tasks: [LookAroundTask, IdleAnimationTask]
│   └── Transitions: 
│       ├── OnEvent("EnemySighted") → CombatState  
│       └── OnEvent("PatrolOrder") → PatrolState
├── PatrolState  
│   ├── Tasks: [MoveToWaypointTask, CheckSurroundingsTask]
│   └── Transitions:
│       ├── OnCompletion → IdleState
│       └── OnEvent("EnemySighted") → CombatState
└── CombatState (LinkedAssetState: Tag="Combat.Type")
    ├── Evaluators: [DistanceEvaluator, HealthEvaluator]
    └── SubStates:
        ├── EngageState → MeleeStateTree (if distance < 5)
        └── RangedCombatState → RangedStateTree (if distance > 5)
```

#### AI组件集成代码
```cpp
UCLASS()
class AMyAICharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMyAICharacter();

protected:
    // AI组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    class UStateTreeAIComponent* StateTreeComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    class UBlackboardComponent* BlackboardComponent;
    
    // StateTree资产
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    FStateTreeReference MainBehaviorTree;
    
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    FStateTreeReferenceOverrides CombatBehaviorOverrides;

    // 生命周期
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // AI事件处理
    UFUNCTION(BlueprintCallable, Category = "AI")
    void OnEnemySighted(AActor* Enemy);
    
    UFUNCTION(BlueprintCallable, Category = "AI") 
    void OnTakeDamage(float DamageAmount, AActor* DamageSource);

private:
    void SetupAIBehavior();
};

void AMyAICharacter::BeginPlay()
{
    Super::BeginPlay();
    SetupAIBehavior();
}

void AMyAICharacter::SetupAIBehavior()
{
    if (StateTreeComponent && MainBehaviorTree.IsValid())
    {
        // 设置主要行为
        StateTreeComponent->SetStateTreeReference(MainBehaviorTree);
        
        // 配置战斗行为覆盖
        StateTreeComponent->SetLinkedStateTreeOverrides(CombatBehaviorOverrides);
        
        // 启动AI逻辑
        StateTreeComponent->StartLogic();
    }
}

void AMyAICharacter::OnEnemySighted(AActor* Enemy)
{
    if (StateTreeComponent)
    {
        // 创建事件数据
        FEnemySightedEvent EventData;
        EventData.Enemy = Enemy;
        EventData.Distance = FVector::Dist(GetActorLocation(), Enemy->GetActorLocation());
        
        // 发送事件到StateTree
        StateTreeComponent->SendStateTreeEvent(
            FGameplayTag::RequestGameplayTag("AI.Event.EnemySighted"),
            FConstStructView::Make(EventData)
        );
    }
}
```

### 7.2 游戏系统状态管理案例

#### 游戏模式StateTree应用
```cpp
UCLASS()
class AMyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMyGameMode();

protected:
    // 游戏状态管理
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameState")
    class UStateTreeComponent* GameStateComponent;
    
    UPROPERTY(EditDefaultsOnly, Category = "GameState")
    FStateTreeReference GameStateTree;
    
    // 游戏事件
    virtual void StartPlay() override;
    virtual void HandleMatchIsWaitingToStart() override;
    virtual void HandleMatchHasStarted() override;
    virtual void HandleMatchHasEnded() override;
    
    UFUNCTION(BlueprintCallable, Category = "Game")
    void OnPlayerEliminated(APlayerController* Player);
    
    UFUNCTION(BlueprintCallable, Category = "Game")
    void OnObjectiveCompleted(FGameplayTag ObjectiveTag);

private:
    void InitializeGameStateTree();
    bool CollectGameModeExternalData(
        const FStateTreeExecutionContext& Context,
        const UStateTree* StateTree,
        TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
        TArrayView<FStateTreeDataView> OutDataViews
    );
};
```

## 第八部分：故障排除与最佳实践

### 8.1 常见问题诊断

#### 问题1: StateTree无法启动
```cpp
// 诊断清单
bool DiagnoseStateTreeIssues(UStateTreeComponent* Component)
{
    // 1. 检查StateTree资产
    if (!Component->GetStateTree())
    {
        UE_LOG(LogError, Error, TEXT("StateTree asset is null"));
        return false;
    }
    
    // 2. 检查StateTree是否准备就绪
    if (!Component->GetStateTree()->IsReadyToRun())
    {
        UE_LOG(LogError, Error, TEXT("StateTree is not ready to run - check compilation"));
        return false;
    }
    
    // 3. 检查Schema兼容性
    TSubclassOf<UStateTreeSchema> RequiredSchema = Component->GetSchema();
    if (!Component->GetStateTree()->GetSchema()->IsA(RequiredSchema))
    {
        UE_LOG(LogError, Error, TEXT("StateTree schema mismatch"));
        return false;
    }
    
    // 4. 检查上下文数据完整性
    FStateTreeExecutionContext TempContext(*Component->GetOwner(), *Component->GetStateTree(), Component->GetInstanceData());
    if (!Component->SetContextRequirements(TempContext))
    {
        UE_LOG(LogError, Error, TEXT("Failed to set context requirements"));
        return false;
    }
    
    return true;
}
```

#### 问题2: 属性绑定失效
```cpp
// 调试属性绑定
void DebugPropertyBindings(const FStateTreePropertyBindings& Bindings)
{
    FString DebugString = Bindings.DebugAsString();
    UE_LOG(LogDebug, Log, TEXT("Property Bindings Debug:\n%s"), *DebugString);
    
    // 检查绑定源数量
    int32 NumSources = Bindings.GetNumBindableStructDescriptors();
    UE_LOG(LogDebug, Log, TEXT("Number of binding sources: %d"), NumSources);
    
    // 逐个检查绑定源
    for (int32 i = 0; i < NumSources; i++)
    {
        // ... 详细检查逻辑
    }
}
```

### 8.2 性能监控

#### StateTree性能分析工具
```cpp
USTRUCT()
struct FStateTreePerformanceMonitor
{
    // 统计数据
    float TotalTickTime = 0.0f;
    int32 TickCount = 0;
    float AverageTickTime = 0.0f;
    float MaxTickTime = 0.0f;
    
    // 活跃状态统计
    TMap<FName, int32> StateActivationCounts;
    TMap<FName, float> StateExecutionTimes;
    
    void RecordTick(float TickTime)
    {
        TotalTickTime += TickTime;
        TickCount++;
        AverageTickTime = TotalTickTime / TickCount;
        MaxTickTime = FMath::Max(MaxTickTime, TickTime);
    }
    
    void RecordStateExecution(FName StateName, float ExecutionTime)
    {
        StateActivationCounts.FindOrAdd(StateName)++;
        StateExecutionTimes.FindOrAdd(StateName) += ExecutionTime;
    }
    
    FString GetPerformanceReport() const
    {
        return FString::Printf(TEXT("Avg: %.3fms, Max: %.3fms, Ticks: %d"), 
                              AverageTickTime * 1000.0f, MaxTickTime * 1000.0f, TickCount);
    }
};
```

### 8.3 最佳实践总结

#### 设计原则
1. **单一职责**: 每个Task/Condition只处理一个明确的逻辑
2. **状态最小化**: 尽量减少实例数据的大小和复杂度
3. **事件驱动**: 优先使用事件而不是轮询检查
4. **延迟绑定**: 仅在需要时创建和维护属性绑定

#### 性能优化清单
- [ ] 使用POD标记优化实例数据
- [ ] 配置合理的Tick频率
- [ ] 避免深层状态嵌套
- [ ] 使用缓存条件评估模式
- [ ] 最小化属性绑定复制
- [ ] 合理使用共享实例数据

#### 调试策略
- [ ] 启用StateTree Trace系统
- [ ] 使用Gameplay Debugger集成
- [ ] 实现自定义调试信息
- [ ] 监控内存使用情况
- [ ] 性能数据统计分析

## 结语

StateTree 和 GameplayStateTree 代表了 Epic Games 在状态机系统设计上的最新成果，提供了传统状态机、行为树和实用AI的最佳特性组合。通过深入理解其架构设计和运行机制，开发者可以构建出高性能、可维护且功能强大的游戏逻辑系统。

本指南基于对源码的深度分析，涵盖了从基础概念到高级应用的完整知识体系。随着插件的持续发展，建议开发者保持对官方文档和源码变更的关注，以获得最新的特性和优化。

---

*本文档基于 StateTree v0.1 和 GameplayStateTree v1.0 源码分析编写*

### 步骤1：准备基础结构

```cpp
// 在你的UObject类中添加必要的成员变量
UCLASS()
class MYGAME_API UMyStateTreeManager : public UObject
{
    GENERATED_BODY()

public:
    UMyStateTreeManager();
    
    // StateTree相关成员
    UPROPERTY(EditAnywhere, Category = "StateTree")
    TObjectPtr<UStateTree> StateTreeAsset;
    
    // 实例数据存储
    FStateTreeInstanceData InstanceData;
    
    // 是否正在运行
    bool bIsRunning = false;
    
    // StateTree管理函数
    bool InitializeStateTree();
    void StartStateTree();
    void TickStateTree(float DeltaTime);
    void StopStateTree();
    void CleanupStateTree();
    
protected:
    // 设置上下文需求
    bool SetContextRequirements(FStateTreeExecutionContext& Context);
    
    // 收集外部数据
    bool CollectExternalData(
        const FStateTreeExecutionContext& Context, 
        const UStateTree* StateTree, 
        TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, 
        TArrayView<FStateTreeDataView> OutDataViews);
};
```

### 步骤2：初始化StateTree

```cpp
bool UMyStateTreeManager::InitializeStateTree()
{
    if (!StateTreeAsset)
    {
        UE_LOG(LogStateTree, Error, TEXT("StateTree asset is null"));
        return false;
    }
    
    // 重置实例数据
    InstanceData.Reset();
    bIsRunning = false;
    
    return true;
}
```

### 步骤3：设置上下文需求

```cpp
bool UMyStateTreeManager::SetContextRequirements(FStateTreeExecutionContext& Context)
{
    if (!Context.IsValid())
    {
        UE_LOG(LogStateTree, Error, TEXT("Invalid execution context"));
        return false;
    }
    
    // 设置外部数据收集回调
    Context.SetCollectExternalDataCallback(
        FOnCollectStateTreeExternalData::CreateUObject(
            this, 
            &UMyStateTreeManager::CollectExternalData
        )
    );
    
    // 设置上下文数据 - 这里以Actor为例
    if (AActor* OwnerActor = GetTypedOuter<AActor>())
    {
        Context.SetContextDataByName(
            TEXT("Actor"), 
            FStateTreeDataView(OwnerActor)
        );
    }
    
    // 验证所有上下文数据是否有效
    bool bValid = Context.AreContextDataViewsValid();
    if (!bValid)
    {
        UE_LOG(LogStateTree, Error, TEXT("Context data views are not valid"));
    }
    
    return bValid;
}
```

### 步骤4：收集外部数据

```cpp
bool UMyStateTreeManager::CollectExternalData(
    const FStateTreeExecutionContext& Context, 
    const UStateTree* StateTree, 
    TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, 
    TArrayView<FStateTreeDataView> OutDataViews)
{
    check(ExternalDataDescs.Num() == OutDataViews.Num());
    
    // 获取World引用
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    
    // 遍历所有外部数据需求
    for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
    {
        const FStateTreeExternalDataDesc& Desc = ExternalDataDescs[Index];
        FStateTreeDataView& DataView = OutDataViews[Index];
        
        // 根据数据类型提供相应的数据
        if (Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
        {
            // 提供World子系统
            UWorldSubsystem* Subsystem = World->GetSubsystemBase(
                Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get()))
            );
            DataView = FStateTreeDataView(Subsystem);
        }
        else if (Desc.Struct->IsChildOf(UGameInstanceSubsystem::StaticClass()))
        {
            // 提供GameInstance子系统
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                UGameInstanceSubsystem* Subsystem = GameInstance->GetSubsystemBase(
                    Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get()))
                );
                DataView = FStateTreeDataView(Subsystem);
            }
        }
        else if (Desc.Struct->IsChildOf(AActor::StaticClass()))
        {
            // 提供Actor数据
            if (AActor* OwnerActor = GetTypedOuter<AActor>())
            {
                DataView = FStateTreeDataView(OwnerActor);
            }
        }
        // 可以根据需要添加更多数据类型的处理
        
        // 如果没有找到匹配的数据，记录警告
        if (!DataView.IsValid())
        {
            UE_LOG(LogStateTree, Warning, 
                TEXT("Could not provide external data for type: %s"), 
                *Desc.Struct->GetName());
        }
    }
    
    return true;
}
```

### 步骤5：启动StateTree

```cpp
void UMyStateTreeManager::StartStateTree()
{
    if (!StateTreeAsset || bIsRunning)
    {
        return;
    }
    
    // 创建执行上下文
    FStateTreeExecutionContext Context(*this, *StateTreeAsset, InstanceData);
    
    // 设置上下文需求
    if (!SetContextRequirements(Context))
    {
        UE_LOG(LogStateTree, Error, TEXT("Failed to set context requirements"));
        return;
    }
    
    // 启动StateTree
    FStateTreeExecutionContext::FStartParameters StartParams;
    // StartParams.GlobalParameters = &MyGlobalParameters; // 可选的全局参数
    // StartParams.RandomSeed = FMath::Rand(); // 可选的随机种子
    
    EStateTreeRunStatus StartStatus = Context.Start(StartParams);
    
    if (StartStatus == EStateTreeRunStatus::Running)
    {
        bIsRunning = true;
        UE_LOG(LogStateTree, Log, TEXT("StateTree started successfully"));
    }
    else
    {
        UE_LOG(LogStateTree, Error, TEXT("Failed to start StateTree, status: %d"), 
            (int32)StartStatus);
    }
}
```

### 步骤6：更新StateTree

```cpp
void UMyStateTreeManager::TickStateTree(float DeltaTime)
{
    if (!bIsRunning || !StateTreeAsset)
    {
        return;
    }
    
    // 创建执行上下文
    FStateTreeExecutionContext Context(*this, *StateTreeAsset, InstanceData);
    
    // 设置上下文需求
    if (!SetContextRequirements(Context))
    {
        UE_LOG(LogStateTree, Error, TEXT("Failed to set context requirements during tick"));
        StopStateTree();
        return;
    }
    
    // 执行Tick
    EStateTreeRunStatus TickStatus = Context.Tick(DeltaTime);
    
    // 检查运行状态
    switch (TickStatus)
    {
    case EStateTreeRunStatus::Running:
        // 继续运行
        break;
        
    case EStateTreeRunStatus::Succeeded:
        UE_LOG(LogStateTree, Log, TEXT("StateTree completed successfully"));
        bIsRunning = false;
        break;
        
    case EStateTreeRunStatus::Failed:
        UE_LOG(LogStateTree, Warning, TEXT("StateTree failed"));
        bIsRunning = false;
        break;
        
    case EStateTreeRunStatus::Stopped:
        UE_LOG(LogStateTree, Log, TEXT("StateTree was stopped"));
        bIsRunning = false;
        break;
        
    default:
        UE_LOG(LogStateTree, Warning, TEXT("Unexpected StateTree status: %d"), 
            (int32)TickStatus);
        break;
    }
}
```

### 步骤7：停止StateTree

```cpp
void UMyStateTreeManager::StopStateTree()
{
    if (!bIsRunning || !StateTreeAsset)
    {
        return;
    }
    
    // 创建执行上下文
    FStateTreeExecutionContext Context(*this, *StateTreeAsset, InstanceData);
    
    // 设置上下文需求（即使是停止也需要有效的上下文）
    if (SetContextRequirements(Context))
    {
        // 停止StateTree
        EStateTreeRunStatus StopStatus = Context.Stop(EStateTreeRunStatus::Stopped);
        UE_LOG(LogStateTree, Log, TEXT("StateTree stopped with status: %d"), 
            (int32)StopStatus);
    }
    
    bIsRunning = false;
}
```

### 步骤8：清理资源

```cpp
void UMyStateTreeManager::CleanupStateTree()
{
    StopStateTree();
    InstanceData.Reset();
    StateTreeAsset = nullptr;
}
```

## 事件处理

### 发送事件到StateTree

```cpp
void UMyStateTreeManager::SendStateTreeEvent(const FGameplayTag& EventTag, const FConstStructView& Payload)
{
    if (!bIsRunning || !StateTreeAsset)
    {
        return;
    }
    
    // 创建执行上下文
    FStateTreeExecutionContext Context(*this, *StateTreeAsset, InstanceData);
    
    // 设置上下文需求
    if (SetContextRequirements(Context))
    {
        // 发送事件
        Context.SendEvent(EventTag, Payload);
    }
}
```

## 调试和状态查询

### 获取当前状态信息

```cpp
TArray<FName> UMyStateTreeManager::GetActiveStateNames() const
{
    if (!bIsRunning || !StateTreeAsset)
    {
        return {};
    }
    
    // 创建只读执行上下文
    FStateTreeMinimalExecutionContext Context(*this, *StateTreeAsset, 
        const_cast<FStateTreeInstanceData&>(InstanceData));
    
    return Context.GetActiveStateNames();
}

EStateTreeRunStatus UMyStateTreeManager::GetCurrentRunStatus() const
{
    if (!bIsRunning || !StateTreeAsset)
    {
        return EStateTreeRunStatus::Unset;
    }
    
    FStateTreeMinimalExecutionContext Context(*this, *StateTreeAsset, 
        const_cast<FStateTreeInstanceData&>(InstanceData));
    
    return Context.GetStateTreeRunStatus();
}
```

## 使用示例

### 在Actor中集成

```cpp
UCLASS()
class MYGAME_API AMyActor : public AActor
{
    GENERATED_BODY()
    
public:
    AMyActor();
    
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StateTree")
    TObjectPtr<UMyStateTreeManager> StateTreeManager;
};

// 实现
AMyActor::AMyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    StateTreeManager = CreateDefaultSubobject<UMyStateTreeManager>(TEXT("StateTreeManager"));
}

void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    
    if (StateTreeManager && StateTreeManager->InitializeStateTree())
    {
        StateTreeManager->StartStateTree();
    }
}

void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (StateTreeManager)
    {
        StateTreeManager->TickStateTree(DeltaTime);
    }
}

void AMyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (StateTreeManager)
    {
        StateTreeManager->CleanupStateTree();
    }
    
    Super::EndPlay(EndPlayReason);
}
```

## 注意事项和最佳实践

### 1. 内存管理
- `FStateTreeExecutionContext`是临时对象，不要跨帧存储
- `FStateTreeInstanceData`需要在StateTree生命周期内保持有效
- 确保UObject Owner的生命周期足够长

### 2. 性能考虑
- 每次操作都需要重新创建ExecutionContext，这是设计上的要求
- 避免在Tick中进行复杂的外部数据收集
- 合理使用事件系统而不是频繁查询状态

### 3. 错误处理
- 始终检查Context.IsValid()
- 验证Context.AreContextDataViewsValid()
- 处理各种StateTree运行状态

### 4. 调试建议
- 使用UE_LOG记录关键状态变化
- 利用StateTree的内置调试功能
- 监控GetActiveStateNames()的变化

## 与StateTreeComponent的对比

| 特性 | ExecutionContext | StateTreeComponent |
|------|------------------|---------------------|
| 控制精度 | 高 | 中 |
| 实现复杂度 | 高 | 低 |
| 自动Tick | 否，需手动 | 是 |
| Blueprint集成 | 需要封装 | 原生支持 |
| 适用场景 | 自定义集成 | 标准AI行为 |

## 总结

直接使用`FStateTreeExecutionContext`提供了对StateTree的完全控制，但需要开发者手动管理整个生命周期。这种方式适合需要精细控制StateTree执行的高级场景，或者需要将StateTree集成到现有系统中的情况。

关键是理解StateTree的事件驱动特性，正确设置外部数据收集，以及妥善处理各种运行状态。通过遵循本文档的流程，你可以成功地在任何UObject中集成StateTree功能。