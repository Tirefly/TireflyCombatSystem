# StateTree 测试配置详解

> **补充文档**: StateSlotActivationIntegrity-TestGuide.md
> **版本**: 1.0
> **创建日期**: 2026-02-03

---

## 目录

1. [StateTree 基础概念](#statetree-基础概念)
2. [测试用 StateTree 配置](#测试用-statetree-配置)
3. [自定义 StateTree Tasks](#自定义-statetree-tasks)
4. [完整配置示例](#完整配置示例)

---

## StateTree 基础概念

### 什么是 StateTree

在 TCS 中，每个状态实例都有自己的 StateTree，用于定义状态的行为逻辑。

**StateTree 组成部分**:
- **Tasks**: 执行具体逻辑的任务节点
- **Transitions**: 状态转换条件
- **Evaluators**: 持续评估的条件
- **Events**: 事件处理

### StateTree 生命周期

```
Enter State (EnterState)
    ↓
Tick (每帧调用)
    ↓
Exit State (ExitState)
```

---

## 测试用 StateTree 配置

### Phase 1: 合并移除测试 StateTree

**Asset 名称**: `ST_TestBuff_MergeRemoval`

**路径**: `Content/Test/StateTree/ST_TestBuff_MergeRemoval`

**用途**: 测试合并淘汰是否通过 RequestStateRemoval

#### 创建步骤

1. **创建 StateTree Asset**
   - 右键 Content Browser → Miscellaneous → State Tree
   - 命名为 `ST_TestBuff_MergeRemoval`
   - Schema 选择 `TcsStateTreeSchema_StateInstance`

2. **配置 Root State**

   **Enter Tasks**:
   ```
   Task 1: Print String
   - Message: "Buff Activated - Waiting for merge"
   - Text Color: Green
   - Duration: 2.0
   - Print to Screen: true
   - Print to Log: true
   ```

   **Tick Tasks**:
   ```
   (无需添加，或添加简单的 Wait Task)
   ```

   **Exit Tasks**:
   ```
   Task 1: Print String
   - Message: "Buff Removed - Check removal reason in log"
   - Text Color: Yellow
   - Duration: 3.0
   - Print to Screen: true
   - Print to Log: true
   ```

3. **添加事件处理**

   **Transition 1**:
   ```
   Event: Event_RemovalRequested (Tag: Event.RemovalRequested)
   Condition: Always True
   Target: Exit State
   ```

#### 验证要点

运行测试时，日志应显示：
```
LogTcsState: Warning: Buff Activated - Waiting for merge
LogTcsState: Verbose: [RequestStateRemoval] Reason=Custom:MergedOut
LogTcsState: Warning: Buff Removed - Check removal reason in log
```

---

### Phase 2: 去再入测试 StateTree

**Asset 名称**: `ST_TestAction_Reentrancy`

**路径**: `Content/Test/StateTree/ST_TestAction_Reentrancy`

**用途**: 测试嵌套状态更新时的延迟请求机制

#### 创建步骤

1. **创建 StateTree Asset**

2. **配置 Root State**

   **Enter Tasks**:
   ```
   Task 1: Print String
   - Message: "High Priority Action - Will trigger nested update"
   - Text Color: Cyan
   - Duration: 2.0

   Task 2: Send Gameplay Event (需要自定义或使用蓝图)
   - Event Tag: Event.Test.TriggerNested
   - Payload: (可选)
   ```

3. **添加 Tick Task**
   ```
   Task: Wait
   - Duration: 2.0 seconds
   - Random Deviation: 0.0
   ```

4. **添加 Exit Task**
   ```
   Task: Print String
   - Message: "Action Completed"
   - Text Color: Green
   ```

5. **添加 Transitions**
   ```
   Transition 1:
   - Condition: Wait Task Completed
   - Target: Exit State

   Transition 2:
   - Event: Event_RemovalRequested
   - Target: Exit State
   ```

#### 配合蓝图事件

在测试 Actor 的蓝图中：

```
Event: On Gameplay Event Received (Event.Test.TriggerNested)
├─ Get World
├─ Get Subsystem (TcsStateManagerSubsystem)
└─ Try Apply State To Target
   ├─ Target: Self
   ├─ State Def Id: TestState_Action_Low
   └─ Instigator: Self
```

#### 验证要点

日志应显示：
```
LogTcsState: Warning: High Priority Action - Will trigger nested update
LogTcsState: Verbose: [RequestUpdateStateSlotActivation] Deferred slot activation update
LogTcsState: Verbose: [DrainPendingSlotActivationUpdates] Processing 1 pending updates
```

---

### Phase 3: 同优先级策略测试 StateTree

**Asset 名称**: `ST_TestSkill_Priority`

**路径**: `Content/Test/StateTree/ST_TestSkill_Priority`

**用途**: 测试 UseNewest/UseOldest 策略

#### 创建步骤

1. **创建 StateTree Asset**

2. **配置 Root State**

   **Enter Tasks**:
   ```
   Task 1: Print String with Timestamp
   - Message: "Skill Activated at {CurrentTime}"
   - Text Color: Magenta
   - Duration: 3.0

   使用蓝图节点：
   ├─ Get Current Time
   ├─ Format Text: "Skill {SkillName} Activated at {Time}"
   └─ Print String
   ```

3. **添加 Tick Task**
   ```
   Task: Wait
   - Duration: 3.0 seconds
   ```

4. **添加 Exit Task**
   ```
   Task: Print String
   - Message: "Skill Finished"
   ```

#### 测试流程

1. 配置槽位使用 **UseNewest** 策略
2. 应用 Skill_1 (记录时间 T1)
3. 延迟 0.5 秒
4. 应用 Skill_2 (记录时间 T2)
5. 观察哪个技能是 Active

**预期结果 (UseNewest)**:
- Skill_2 应该是 Active
- Skill_1 应该被 HangUp/Pause/Cancel（根据 PreemptionPolicy）

**预期结果 (UseOldest)**:
- Skill_1 应该保持 Active
- Skill_2 应该被 HangUp/Pause/Cancel

---

### Phase 4: Gate 关闭测试 StateTree

**Asset 名称**: `ST_TestGateClose`

**路径**: `Content/Test/StateTree/ST_TestGateClose`

**用途**: 测试 Gate 关闭时的行为

#### 创建步骤

1. **创建 StateTree Asset**

2. **配置 Root State**

   **Enter Tasks**:
   ```
   Task 1: Print String
   - Message: "State Active (Gate Open)"
   - Text Color: Green
   - Duration: 2.0
   ```

3. **添加 Tick Task**
   ```
   Task: Wait
   - Duration: 5.0 seconds (足够长，用于测试 Gate 关闭)
   ```

4. **添加 Exit Task**
   ```
   Task: Print String
   - Message: "State Exited"
   - Text Color: Red
   ```

#### 测试流程

**测试 HangUp 策略**:
```
1. 应用状态 → 观察 "State Active (Gate Open)"
2. 关闭 Gate → 观察状态变为 HangUp
3. 打开 Gate → 观察状态恢复 Active
```

**测试 Pause 策略**:
```
1. 应用状态 → 观察 "State Active (Gate Open)"
2. 关闭 Gate → 观察状态变为 Pause
3. 持续时间停止计算
```

**测试 Cancel 策略**:
```
1. 应用状态 → 观察 "State Active (Gate Open)"
2. 关闭 Gate → 观察 "State Exited"
3. 状态被移除
```

---

## 自定义 StateTree Tasks

### Task 1: TcsTestPrintStateInfoTask

**用途**: 打印状态详细信息，用于调试

#### 代码实现

**头文件**: `TcsTestPrintStateInfoTask.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TcsTestPrintStateInfoTask.generated.h"

USTRUCT()
struct FTcsTestPrintStateInfoTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    FString Prefix = TEXT("State Info");

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bPrintTimestamp = true;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bPrintPriority = true;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bPrintStage = true;
};

USTRUCT(meta = (DisplayName = "Test: Print State Info"))
struct FTcsTestPrintStateInfoTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FTcsTestPrintStateInfoTaskInstanceData;

    FTcsTestPrintStateInfoTask() = default;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(
        FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
};
```

**实现文件**: `TcsTestPrintStateInfoTask.cpp`

```cpp
#include "TcsTestPrintStateInfoTask.h"
#include "State/TcsState.h"

EStateTreeRunStatus FTcsTestPrintStateInfoTask::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    // 获取当前状态实例
    UTcsStateInstance* StateInstance = Cast<UTcsStateInstance>(Context.GetOwner());
    if (!StateInstance)
    {
        return EStateTreeRunStatus::Failed;
    }

    FString Message = InstanceData.Prefix;

    // 添加状态名称
    Message += FString::Printf(TEXT(" | State: %s"),
        *StateInstance->GetStateDefId().ToString());

    // 添加时间戳
    if (InstanceData.bPrintTimestamp)
    {
        Message += FString::Printf(TEXT(" | Timestamp: %lld"),
            StateInstance->GetApplyTimestamp());
    }

    // 添加优先级
    if (InstanceData.bPrintPriority)
    {
        Message += FString::Printf(TEXT(" | Priority: %d"),
            StateInstance->GetStateDef().Priority);
    }

    // 添加阶段
    if (InstanceData.bPrintStage)
    {
        Message += FString::Printf(TEXT(" | Stage: %s"),
            *StaticEnum<ETcsStateStage>()->GetNameStringByValue(
                static_cast<int64>(StateInstance->GetCurrentStage())));
    }

    // 输出到日志和屏幕
    UE_LOG(LogTcsState, Warning, TEXT("%s"), *Message);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Message);
    }

    return EStateTreeRunStatus::Succeeded;
}
```

#### 使用方法

1. 将代码添加到项目中
2. 编译项目
3. 在 StateTree 编辑器中：
   - 右键节点 → Add Task
   - 搜索 "Test: Print State Info"
   - 配置参数：
     - Prefix: "Phase 1 Test"
     - Print Timestamp: true
     - Print Priority: true
     - Print Stage: true

---

### Task 2: TcsTriggerNestedStateTask

**用途**: 触发嵌套状态更新，测试去再入机制

#### 代码实现

**头文件**: `TcsTriggerNestedStateTask.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TcsTriggerNestedStateTask.generated.h"

USTRUCT()
struct FTcsTriggerNestedStateTaskInstanceData
{
    GENERATED_BODY()

    // 要触发的状态名称
    UPROPERTY(EditAnywhere, Category = "Parameter")
    FName StateToTrigger;

    // 延迟时间（秒）
    UPROPERTY(EditAnywhere, Category = "Parameter")
    float DelaySeconds = 0.0f;

    // 是否打印日志
    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bLogTrigger = true;
};

USTRUCT(meta = (DisplayName = "Test: Trigger Nested State"))
struct FTcsTriggerNestedStateTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FTcsTriggerNestedStateTaskInstanceData;

    FTcsTriggerNestedStateTask() = default;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(
        FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
};
```

**实现文件**: `TcsTriggerNestedStateTask.cpp`

```cpp
#include "TcsTriggerNestedStateTask.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"

EStateTreeRunStatus FTcsTriggerNestedStateTask::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    UTcsStateInstance* StateInstance = Cast<UTcsStateInstance>(Context.GetOwner());
    if (!StateInstance)
    {
        return EStateTreeRunStatus::Failed;
    }

    AActor* Owner = StateInstance->GetOwner();
    if (!Owner)
    {
        return EStateTreeRunStatus::Failed;
    }

    UWorld* World = Owner->GetWorld();
    if (!World)
    {
        return EStateTreeRunStatus::Failed;
    }

    UTcsStateManagerSubsystem* StateMgr = World->GetSubsystem<UTcsStateManagerSubsystem>();
    if (!StateMgr)
    {
        return EStateTreeRunStatus::Failed;
    }

    if (InstanceData.bLogTrigger)
    {
        UE_LOG(LogTcsState, Warning,
            TEXT("[TriggerNestedState] Triggering: %s (Delay: %.2fs)"),
            *InstanceData.StateToTrigger.ToString(),
            InstanceData.DelaySeconds);
    }

    // 触发嵌套状态（测试去再入机制）
    if (InstanceData.DelaySeconds > 0.0f)
    {
        // 延迟触发
        FTimerHandle TimerHandle;
        World->GetTimerManager().SetTimer(TimerHandle,
            [StateMgr, Owner, InstanceData]()
            {
                StateMgr->TryApplyStateToTarget(
                    Owner,
                    InstanceData.StateToTrigger,
                    Owner);
            },
            InstanceData.DelaySeconds,
            false);
    }
    else
    {
        // 立即触发（测试同步嵌套更新）
        StateMgr->TryApplyStateToTarget(
            Owner,
            InstanceData.StateToTrigger,
            Owner);
    }

    return EStateTreeRunStatus::Succeeded;
}
```

#### 使用方法

在 StateTree 的 Enter Tasks 中添加：
```
Task: Trigger Nested State
- State To Trigger: TestState_Action_Low
- Delay Seconds: 0.0 (立即触发，测试同步嵌套)
- Log Trigger: true
```

---

### Task 3: TcsWaitForRemovalRequestTask

**用途**: 等待移除请求，用于测试 Phase 1

#### 代码实现

**头文件**: `TcsWaitForRemovalRequestTask.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TcsWaitForRemovalRequestTask.generated.h"

USTRUCT()
struct FTcsWaitForRemovalRequestTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bLogWhenReceived = true;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bPrintRemovalReason = true;
};

USTRUCT(meta = (DisplayName = "TCS: Wait For Removal Request"))
struct FTcsWaitForRemovalRequestTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FTcsWaitForRemovalRequestTaskInstanceData;

    FTcsWaitForRemovalRequestTask() = default;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(
        FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;

    virtual EStateTreeRunStatus Tick(
        FStateTreeExecutionContext& Context,
        const float DeltaTime) const override;
};
```

**实现文件**: `TcsWaitForRemovalRequestTask.cpp`

```cpp
#include "TcsWaitForRemovalRequestTask.h"
#include "State/TcsState.h"

EStateTreeRunStatus FTcsWaitForRemovalRequestTask::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    if (InstanceData.bLogWhenReceived)
    {
        UE_LOG(LogTcsState, Verbose,
            TEXT("[WaitForRemovalRequest] Task started, waiting for removal request..."));
    }

    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTcsWaitForRemovalRequestTask::Tick(
    FStateTreeExecutionContext& Context,
    const float DeltaTime) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    UTcsStateInstance* StateInstance = Cast<UTcsStateInstance>(Context.GetOwner());
    if (!StateInstance)
    {
        return EStateTreeRunStatus::Failed;
    }

    // 检查是否有待处理的移除请求
    if (StateInstance->HasPendingRemovalRequest())
    {
        const FTcsStateRemovalRequest& Request = StateInstance->GetPendingRemovalRequest();
        FName ReasonName = Request.ToRemovalReasonName();

        if (InstanceData.bLogWhenReceived)
        {
            UE_LOG(LogTcsState, Warning,
                TEXT("[WaitForRemovalRequest] ✅ Received removal request!"));
        }

        if (InstanceData.bPrintRemovalReason)
        {
            UE_LOG(LogTcsState, Warning,
                TEXT("[WaitForRemovalRequest] Reason: %s"),
                *ReasonName.ToString());

            if (GEngine)
            {
                FString Message = FString::Printf(
                    TEXT("Removal Request Received: %s"),
                    *ReasonName.ToString());
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Message);
            }
        }

        // 完成任务，允许状态退出
        return EStateTreeRunStatus::Succeeded;
    }

    return EStateTreeRunStatus::Running;
}
```

#### 使用方法

在 StateTree 的 Tick Tasks 中添加：
```
Task: Wait For Removal Request
- Log When Received: true
- Print Removal Reason: true
```

这个 Task 会持续运行，直到收到移除请求。

---

## 完整配置示例

### 示例 1: Phase 1 完整 StateTree

**ST_Phase1_Complete**

```
Root State
│
├─ Enter Tasks
│  ├─ [1] Print State Info Task
│  │   ├─ Prefix: "Phase 1: Buff Entered"
│  │   ├─ Print Timestamp: true
│  │   ├─ Print Priority: true
│  │   └─ Print Stage: true
│  │
│  └─ [2] Print String
│      └─ Message: "Waiting for merge or removal..."
│
├─ Tick Tasks
│  └─ [1] Wait For Removal Request Task
│      ├─ Log When Received: true
│      └─ Print Removal Reason: true
│
├─ Exit Tasks
│  └─ [1] Print String
│      ├─ Message: "✅ Phase 1: Buff exited via RequestStateRemoval"
│      ├─ Text Color: Green
│      └─ Duration: 3.0
│
└─ Transitions
   └─ [1] On Event: Event_RemovalRequested
      ├─ Condition: Always True
      └─ Target: Exit State
```

**测试步骤**:
1. 应用状态 A（使用此 StateTree）
2. 应用状态 A 第二次（触发合并）
3. 观察日志：
   ```
   Phase 1: Buff Entered | Timestamp: xxx | Priority: 10
   Waiting for merge or removal...
   [RequestStateRemoval] Reason=Custom:MergedOut
   ✅ Received removal request! Reason: MergedOut
   ✅ Phase 1: Buff exited via RequestStateRemoval
   ```

---

### 示例 2: Phase 2 完整 StateTree

**ST_Phase2_Complete**

```
Root State
│
├─ Enter Tasks
│  ├─ [1] Print State Info Task
│  │   └─ Prefix: "Phase 2: High Priority Action"
│  │
│  ├─ [2] Print String
│  │   └─ Message: "About to trigger nested state update..."
│  │
│  └─ [3] Trigger Nested State Task
│      ├─ State To Trigger: TestState_Action_Low
│      ├─ Delay Seconds: 0.0
│      └─ Log Trigger: true
│
├─ Tick Tasks
│  └─ [1] Wait Task
│      └─ Duration: 2.0 seconds
│
├─ Exit Tasks
│  └─ [1] Print String
│      └─ Message: "✅ Phase 2: Action completed without recursion"
│
└─ Transitions
   ├─ [1] On Completed: Wait Task
   │  └─ Target: Exit State
   │
   └─ [2] On Event: Event_RemovalRequested
      └─ Target: Exit State
```

**测试步骤**:
1. 应用状态（使用此 StateTree）
2. 观察日志：
   ```
   Phase 2: High Priority Action
   About to trigger nested state update...
   [TriggerNestedState] Triggering: TestState_Action_Low
   [RequestUpdateStateSlotActivation] Deferred slot activation update
   [DrainPendingSlotActivationUpdates] Processing 1 pending updates
   ✅ Phase 2: Action completed without recursion
   ```

---

### 示例 3: Phase 3 完整 StateTree

**ST_Phase3_Complete**

```
Root State
│
├─ Enter Tasks
│  ├─ [1] Print State Info Task
│  │   ├─ Prefix: "Phase 3: Skill"
│  │   ├─ Print Timestamp: true (重要！用于验证策略)
│  │   ├─ Print Priority: true
│  │   └─ Print Stage: true
│  │
│  └─ [2] Print String
│      └─ Message: "Skill executing..."
│
├─ Tick Tasks
│  └─ [1] Wait Task
│      └─ Duration: 3.0 seconds
│
├─ Exit Tasks
│  └─ [1] Print String
│      └─ Message: "Skill finished or interrupted"
│
└─ Transitions
   ├─ [1] On Completed: Wait Task
   │  └─ Target: Exit State
   │
   └─ [2] On Event: Event_RemovalRequested
      └─ Target: Exit State
```

**测试步骤**:
1. 配置槽位使用 UseNewest 策略
2. 应用 Skill_1（记录 Timestamp T1）
3. 延迟 0.5 秒
4. 应用 Skill_2（记录 Timestamp T2）
5. 观察日志，验证 T2 > T1 且 Skill_2 是 Active

---

## 蓝图辅助工具

### 创建测试控制器蓝图

**BP_StateTreeTestController**

**功能**:
- 快速应用测试状态
- 切换 Gate 状态
- 显示当前状态信息

**Event Graph**:

```
Event BeginPlay
├─ Create Widget (WBP_StateDebugWidget)
├─ Add to Viewport
└─ Store Widget Reference

Event Tick
├─ Update Debug Widget
└─ Check for Input

Input Action: Apply State 1 (Key: 1)
├─ Get World
├─ Get Subsystem (TcsStateManagerSubsystem)
└─ Try Apply State To Target
   ├─ Target: Test Actor
   ├─ State: TestState_Buff_A
   └─ Instigator: Test Actor

Input Action: Apply State 2 (Key: 2)
└─ (同上，使用 TestState_Buff_B)

Input Action: Toggle Gate (Key: G)
├─ Get State Component
├─ Is Slot Gate Open?
├─ Branch
│  ├─ True: Set Slot Gate Open (false)
│  └─ False: Set Slot Gate Open (true)
└─ Print: "Gate toggled"
```

---

## 总结

### 关键要点

1. **StateTree 是状态行为的核心**
   - 每个测试状态都需要配置对应的 StateTree
   - StateTree 定义了状态的生命周期逻辑

2. **自定义 Tasks 提供测试能力**
   - Print State Info: 显示状态详细信息
   - Trigger Nested State: 测试去再入机制
   - Wait For Removal Request: 验证移除路径

3. **事件处理是关键**
   - Event_RemovalRequested: 处理移除请求
   - 自定义事件: 触发嵌套更新

4. **日志是最好的验证工具**
   - 在关键节点添加日志输出
   - 使用屏幕消息辅助可视化

### 下一步

1. 创建上述 StateTree Assets
2. 实现自定义 Tasks（可选，也可以用 Print String 代替）
3. 配置数据表引用这些 StateTree
4. 运行测试，观察日志输出

---

**文档结束**
