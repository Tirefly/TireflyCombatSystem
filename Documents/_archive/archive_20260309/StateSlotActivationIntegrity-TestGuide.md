# TCS 状态槽位激活完整性重构 - 详细测试指南

> **文档版本**: 1.0  
> **创建日期**: 2026-02-03  
> **适用版本**: TCS 1.0+  
> **对应提案**: refactor-state-slot-activation-integrity

---

## 📋 目录

1. [测试环境准备](#测试环境准备)
2. [数据表配置](#数据表配置)
3. [测试 Actor 创建](#测试-actor-创建)
4. [Phase 1: 合并移除统一化测试](#phase-1-合并移除统一化测试)
5. [Phase 2: 槽位激活去再入测试](#phase-2-槽位激活去再入测试)
6. [Phase 3: 同优先级策略测试](#phase-3-同优先级策略测试)
7. [Phase 4: Gate 关闭逻辑测试](#phase-4-gate-关闭逻辑测试)
8. [综合测试场景](#综合测试场景)
9. [故障排查](#故障排查)

---

## 测试环境准备

### 1.1 前置条件

- ✅ UE 5.7 编辑器已安装
- ✅ TireflyCombatSystem 插件已编译成功
- ✅ 项目已正确配置 GameplayTags
- ✅ 已创建测试关卡

### 1.2 启用详细日志

在编辑器启动时，修改日志配置：

**方法 1：编辑器控制台**
```
Log LogTcsState Verbose
```

**方法 2：配置文件**

编辑 `Config/DefaultEngine.ini`，添加：
```ini
[Core.Log]
LogTcsState=Verbose
```

### 1.3 创建测试关卡

1. 创建新关卡：`Content/Test/TestLevel_StateIntegrity`
2. 添加 `PlayerStart`
3. 保存关卡

---

## 数据表配置

### 2.1 状态槽位定义表

创建或修改 `DT_StateSlotDefinitions_Test`：

**路径**: `Content/Test/Data/DT_StateSlotDefinitions_Test`

**表结构**: `FTcsStateSlotDefinition`

**配置示例**:

| Row Name | SlotTag | ActivationMode | GateCloseBehavior | PreemptionPolicy | SamePriorityPolicy |
|----------|---------|----------------|-------------------|------------------|-------------------|
| TestSlot_Buff | StateSlot.Test.Buff | AllActive | Pause | PauseLowerPriority | UseNewest |
| TestSlot_Action | StateSlot.Test.Action | PriorityOnly | HangUp | HangUpLowerPriority | UseNewest |
| TestSlot_Skill | StateSlot.Test.Skill | PriorityOnly | Cancel | CancelLowerPriority | UseOldest |

**详细配置步骤**:

1. 右键 Content Browser → Miscellaneous → Data Table
2. 选择 `FTcsStateSlotDefinition` 作为 Row Structure
3. 命名为 `DT_StateSlotDefinitions_Test`
4. 打开数据表，添加上述行
5. 配置每行的字段：
   - **SlotTag**: 从 GameplayTag 选择器中选择或创建
   - **ActivationMode**: 下拉选择
   - **GateCloseBehavior**: 下拉选择
   - **PreemptionPolicy**: 下拉选择
   - **SamePriorityPolicy**: 选择策略类（UseNewest 或 UseOldest）

### 2.2 状态定义表

创建 `DT_StateDefinitions_Test`：

**路径**: `Content/Test/Data/DT_StateDefinitions_Test`

**表结构**: `FTcsStateDefinition`

**配置示例**:

| Row Name | StateSlotType | Priority | MergerType | DurationType | Duration | StateTree | 测试用途 |
|----------|---------------|----------|------------|--------------|----------|-----------|---------|
| TestState_Buff_A | StateSlot.Test.Buff | 10 | UseNewest | Duration | 5.0 | ST_TestBuff_MergeRemoval | Phase 1 |
| TestState_Buff_B | StateSlot.Test.Buff | 10 | UseNewest | Duration | 5.0 | ST_TestBuff_MergeRemoval | Phase 1 |
| TestState_Action_High | StateSlot.Test.Action | 100 | NoMerge | Duration | 2.0 | ST_TestAction_Reentrancy | Phase 2 |
| TestState_Action_Low | StateSlot.Test.Action | 50 | NoMerge | Duration | 2.0 | ST_TestAction_Reentrancy | Phase 2 |
| TestState_Skill_1 | StateSlot.Test.Skill | 10 | NoMerge | Duration | 3.0 | ST_TestSkill_Priority | Phase 3 |
| TestState_Skill_2 | StateSlot.Test.Skill | 10 | NoMerge | Duration | 3.0 | ST_TestSkill_Priority | Phase 3 |
| TestState_GateTest | StateSlot.Test.Action | 50 | NoMerge | Duration | 5.0 | ST_TestGateClose | Phase 4 |

**详细配置步骤**:

1. 创建数据表，选择 `FTcsStateDefinition`
2. 添加上述行
3. 配置字段：
   - **StateSlotType**: 选择对应的槽位 Tag
   - **Priority**: 输入优先级数值（越大越高）
   - **MergerType**: 选择合并策略类
   - **DurationType**: 选择 Duration（有持续时间）
   - **Duration**: 输入持续时间（秒）
   - **StateTree**: 选择对应的 StateTree Asset（见下方 StateTree 创建说明）

### 2.2.1 StateTree Assets 创建

在配置状态定义表之前，需要先创建以下 StateTree Assets：

**创建路径**: `Content/Test/StateTree/`

**需要创建的 StateTree**:

1. **ST_TestBuff_MergeRemoval** (Phase 1 测试)
   - 用途: 测试合并移除统一化
   - 详细配置: 参见 `StateTree-TestConfiguration.md` 第 2.1 节

2. **ST_TestAction_Reentrancy** (Phase 2 测试)
   - 用途: 测试槽位激活去再入
   - 详细配置: 参见 `StateTree-TestConfiguration.md` 第 2.2 节

3. **ST_TestSkill_Priority** (Phase 3 测试)
   - 用途: 测试同优先级策略
   - 详细配置: 参见 `StateTree-TestConfiguration.md` 第 2.3 节

4. **ST_TestGateClose** (Phase 4 测试)
   - 用途: 测试 Gate 关闭逻辑
   - 详细配置: 参见 `StateTree-TestConfiguration.md` 第 2.4 节

**快速创建步骤**:

1. 右键 Content Browser → Miscellaneous → State Tree
2. 命名为对应的 StateTree 名称
3. Schema 选择 `TcsSTSchema_StateInstance`
4. 按照 `StateTree-TestConfiguration.md` 文档配置 Tasks 和 Transitions
5. 保存 Asset

**重要提示**:
- 必须先创建 StateTree Assets，然后才能在状态定义表中引用
- 如果暂时没有创建 StateTree，可以先留空，后续再配置
- 详细的 StateTree 配置说明请参考 `StateTree-TestConfiguration.md` 文档

### 2.3 GameplayTags 配置

在 `Config/DefaultGameplayTags.ini` 中添加：

```ini
[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="StateSlot.Test.Buff",DevComment="测试用 Buff 槽位")
+GameplayTagList=(Tag="StateSlot.Test.Action",DevComment="测试用 Action 槽位")
+GameplayTagList=(Tag="StateSlot.Test.Skill",DevComment="测试用 Skill 槽位")
```

或在编辑器中：
1. Project Settings → GameplayTags
2. 添加新 Tag
3. 重启编辑器

---

## 测试 Actor 创建

### 3.1 创建测试 Actor 类

**C++ 类**: `ATestStateActor`

**路径**: `Source/TireflyGameplayUtils/Test/TestStateActor.h`

```cpp
// TestStateActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TcsEntityInterface.h"
#include "TestStateActor.generated.h"

UCLASS()
class TIREFLYGAMEPLAYUTILS_API ATestStateActor : public AActor, public ITcsEntityInterface
{
    GENERATED_BODY()

public:
    ATestStateActor();

protected:
    virtual void BeginPlay() override;

public:
    // ITcsEntityInterface 实现
    virtual UTcsAttributeComponent* GetAttributeComponent_Implementation() const override;
    virtual UTcsStateComponent* GetStateComponent_Implementation() const override;
    virtual UTcsSkillComponent* GetSkillComponent_Implementation() const override;
    virtual ETcsCombatEntityType GetCombatEntityType_Implementation() const override;
    virtual int32 GetCombatEntityLevel_Implementation() const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TCS")
    UTcsAttributeComponent* AttributeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TCS")
    UTcsStateComponent* StateComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TCS")
    UTcsSkillComponent* SkillComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TCS")
    int32 EntityLevel = 1;
};
```

```cpp
// TestStateActor.cpp
#include "Test/TestStateActor.h"
#include "State/TcsStateComponent.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Skill/TcsSkillComponent.h"

ATestStateActor::ATestStateActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建组件
    AttributeComponent = CreateDefaultSubobject<UTcsAttributeComponent>(TEXT("AttributeComponent"));
    StateComponent = CreateDefaultSubobject<UTcsStateComponent>(TEXT("StateComponent"));
    SkillComponent = CreateDefaultSubobject<UTcsSkillComponent>(TEXT("SkillComponent"));
}

void ATestStateActor::BeginPlay()
{
    Super::BeginPlay();

    // 初始化状态槽位映射
    if (UWorld* World = GetWorld())
    {
        if (UTcsStateManagerSubsystem* StateMgr = World->GetSubsystem<UTcsStateManagerSubsystem>())
        {
            StateMgr->InitStateSlotMappings(this);
        }
    }
}

UTcsAttributeComponent* ATestStateActor::GetAttributeComponent_Implementation() const
{
    return AttributeComponent;
}

UTcsStateComponent* ATestStateActor::GetStateComponent_Implementation() const
{
    return StateComponent;
}

UTcsSkillComponent* ATestStateActor::GetSkillComponent_Implementation() const
{
    return SkillComponent;
}

ETcsCombatEntityType ATestStateActor::GetCombatEntityType_Implementation() const
{
    return ETcsCombatEntityType::CET_Character;
}

int32 ATestStateActor::GetCombatEntityLevel_Implementation() const
{
    return EntityLevel;
}
```

### 3.2 创建蓝图测试 Actor

如果不想写 C++ 代码，可以创建蓝图：

1. 创建 Blueprint Class，父类选择 `Actor`
2. 命名为 `BP_TestStateActor`
3. 添加组件：
   - `TcsAttributeComponent`
   - `TcsStateComponent`
   - `TcsSkillComponent`
4. 实现 `ITcsEntityInterface` 接口：
   - Add Interface → TcsEntityInterface
   - 实现所有接口函数
5. 在 BeginPlay 中调用初始化：
   ```
   Get World → Get Subsystem (TcsStateManagerSubsystem) → Init State Slot Mappings (Self)
   ```

### 3.3 创建测试控制器

**蓝图**: `BP_TestController`

**功能**: 提供测试用的按键绑定

**步骤**:
1. 创建 Blueprint Class，父类 `Actor`
2. 添加以下函数：
   - `ApplyTestState(StateName, TargetActor)`
   - `RemoveTestState(StateName, TargetActor)`
   - `ToggleGate(SlotTag, TargetActor)`
3. 绑定按键（在 Event Graph）：
   - `1` 键：应用 TestState_Buff_A
   - `2` 键：应用 TestState_Buff_B
   - `3` 键：应用 TestState_Action_High
   - `4` 键：应用 TestState_Action_Low
   - `G` 键：切换 Gate 状态


---

## Phase 1: 合并移除统一化测试

### 4.1 测试目标

验证合并淘汰的状态通过 RequestStateRemoval 路径移除。

### 4.2 测试步骤

1. 应用第一个 Buff 状态
2. 应用第二个相同 DefId 的 Buff
3. 观察日志，确认旧状态通过 RequestStateRemoval 移除
4. 验证槽位中只有 1 个状态

### 4.3 预期结果

日志应包含：
- [RequestStateRemoval] Reason=Custom:MergedOut
- 槽位中只有新状态

---

## Phase 2: 槽位激活去再入测试

### 5.1 测试目标

验证延迟请求机制防止递归调用。

### 5.2 测试步骤

1. 应用状态 A，在其激活时触发状态 B
2. 观察日志中的 Deferred 消息
3. 验证没有递归调用

### 5.3 预期结果

- 看到 Deferred slot activation update 消息
- 队列正确排空
- 无堆栈溢出

---

## Phase 3: 同优先级策略测试

### 6.1 测试目标

验证 UseNewest 和 UseOldest 策略。

### 6.2 测试步骤

1. 配置槽位使用 UseNewest 策略
2. 依次应用两个同优先级状态
3. 验证最新的状态是 Active

### 6.3 预期结果

- UseNewest: 最后应用的状态 Active
- UseOldest: 最先应用的状态 Active

---

## Phase 4: Gate 关闭逻辑测试

### 7.1 测试目标

验证 Gate 关闭时的统一处理。

### 7.2 测试步骤

1. 应用状态到槽位
2. 关闭 Gate
3. 观察状态阶段变化

### 7.3 预期结果

- HangUp 策略: Active → HangUp
- Pause 策略: Active → Pause  
- Cancel 策略: 状态被移除

测试文档第三部分

## 附录：测试检查清单

### Phase 1 检查
- 合并淘汰使用 RequestStateRemoval
- 看到 Custom:MergedOut 原因

### Phase 2 检查
- 看到 Deferred 消息
- 无递归调用

### Phase 3 检查
- UseNewest: 新状态 Active
- UseOldest: 旧状态 Active

### Phase 4 检查
- HangUp/Pause/Cancel 策略正确
- 不变量断言生效

---
文档创建完成！

---

## 附录 D: StateTree 配置快速参考

### D.1 StateTree 与测试状态对应关系

| StateTree Asset | 使用的状态 | 测试阶段 | 主要功能 |
|----------------|-----------|---------|---------|
| ST_TestBuff_MergeRemoval | TestState_Buff_A, TestState_Buff_B | Phase 1 | 验证合并移除通过 RequestStateRemoval |
| ST_TestAction_Reentrancy | TestState_Action_High, TestState_Action_Low | Phase 2 | 验证延迟请求机制防止递归 |
| ST_TestSkill_Priority | TestState_Skill_1, TestState_Skill_2 | Phase 3 | 验证 UseNewest/UseOldest 策略 |
| ST_TestGateClose | TestState_GateTest | Phase 4 | 验证 Gate 关闭行为 |

### D.2 StateTree 创建优先级

**必须创建** (核心测试):
1. ✅ ST_TestBuff_MergeRemoval (Phase 1)
2. ✅ ST_TestAction_Reentrancy (Phase 2)

**推荐创建** (完整测试):
3. ⭐ ST_TestSkill_Priority (Phase 3)
4. ⭐ ST_TestGateClose (Phase 4)

**简化方案**:
如果时间有限，可以先创建一个通用的 StateTree，包含基本的 Enter/Exit Tasks：

**ST_TestGeneric** (通用测试 StateTree):
```
Root State
├─ Enter Tasks
│  └─ Print String: "State Activated: {StateName}"
├─ Tick Tasks
│  └─ Wait: 3.0 seconds
├─ Exit Tasks
│  └─ Print String: "State Exited: {StateName}"
└─ Transitions
   ├─ On Completed: Wait Task → Exit State
   └─ On Event: Event_RemovalRequested → Exit State
```

然后所有测试状态都可以先使用这个通用 StateTree，后续再根据需要创建专用的。

### D.3 StateTree 配置检查清单

创建每个 StateTree 时，确保：

**基础配置**:
- [ ] Schema 设置为 `TcsSTSchema_StateInstance`
- [ ] Root State 已创建
- [ ] 至少有一个 Enter Task
- [ ] 至少有一个 Exit Task

**事件处理**:
- [ ] 添加了 `Event_RemovalRequested` 的 Transition
- [ ] Transition 目标设置为 Exit State
- [ ] Condition 设置为 Always True

**日志输出**:
- [ ] Enter Task 中有 Print String（显示状态激活）
- [ ] Exit Task 中有 Print String（显示状态退出）
- [ ] 关键操作有日志输出

**Phase 特定**:
- [ ] Phase 1: Exit Task 提示检查移除原因
- [ ] Phase 2: Enter Task 触发嵌套状态
- [ ] Phase 3: Enter Task 打印时间戳
- [ ] Phase 4: 根据 GateCloseBehavior 配置不同逻辑

### D.4 StateTree 与数据表配置流程

**推荐流程**:

```
步骤 1: 创建 StateTree Assets
├─ Content/Test/StateTree/ST_TestBuff_MergeRemoval
├─ Content/Test/StateTree/ST_TestAction_Reentrancy
├─ Content/Test/StateTree/ST_TestSkill_Priority
└─ Content/Test/StateTree/ST_TestGateClose

步骤 2: 配置 StateTree 内容
├─ 参考 StateTree-TestConfiguration.md
├─ 添加 Enter/Tick/Exit Tasks
├─ 配置 Transitions
└─ 保存 Assets

步骤 3: 创建状态定义数据表
├─ Content/Test/Data/DT_StateDefinitions_Test
└─ 添加状态行

步骤 4: 关联 StateTree
├─ 在数据表中选择对应的 StateTree Asset
└─ 保存数据表

步骤 5: 验证配置
├─ 打开状态定义数据表
├─ 确认每个状态都有 StateTree 引用
└─ 确认 StateTree 路径正确
```

### D.5 常见配置错误

**错误 1: StateTree Schema 不正确**
```
❌ 错误: 使用了默认的 StateTreeSchema
✅ 正确: 使用 TcsSTSchema_StateInstance
```

**错误 2: 缺少事件处理**
```
❌ 错误: 没有添加 Event_RemovalRequested 的 Transition
✅ 正确: 添加 Transition，Event = Event_RemovalRequested，Target = Exit State
```

**错误 3: 数据表引用错误**
```
❌ 错误: StateTree 字段为空或路径错误
✅ 正确: 正确选择 StateTree Asset
```

**错误 4: Tasks 配置不完整**
```
❌ 错误: 只有 Enter Task，没有 Exit Task
✅ 正确: Enter 和 Exit Tasks 都要配置
```

### D.6 StateTree 调试技巧

**技巧 1: 使用 Print String 追踪执行流程**
```cpp
Enter Task: Print ">>> Entering State: {StateName}"
Tick Task: Print "... Ticking State: {StateName}"
Exit Task: Print "<<< Exiting State: {StateName}"
```

**技巧 2: 打印关键变量**
```cpp
// 在 Enter Task 中
Print: "State Info | Priority: {Priority} | Timestamp: {Timestamp}"
```

**技巧 3: 使用不同颜色区分阶段**
```cpp
Enter Task: Green (激活)
Tick Task: Cyan (运行中)
Exit Task: Yellow (退出)
Error: Red (错误)
```

**技巧 4: 启用 StateTree 调试**
```
控制台命令:
statetree.debug 1
statetree.debugger 1
```

### D.7 StateTree 性能优化建议

1. **避免在 Tick 中频繁打印**
   - 使用条件判断，只在关键时刻打印
   - 或使用计时器，每秒打印一次

2. **合理使用 Wait Task**
   - 不要设置过短的 Duration
   - 建议最小 0.1 秒

3. **及时清理不用的 Tasks**
   - 测试完成后，移除调试用的 Print Tasks
   - 保留核心逻辑 Tasks

### D.8 完整配置示例参考

详细的 StateTree 配置示例请参考：
- **文档**: `StateTree-TestConfiguration.md`
- **章节**: 
  - 第 2 节: 测试用 StateTree 配置
  - 第 3 节: 自定义 StateTree Tasks
  - 第 4 节: 完整配置示例

---

## 附录 E: 测试执行顺序建议

### E.1 首次测试（最小配置）

**目标**: 验证基础功能

**步骤**:
1. 创建 `ST_TestGeneric` (通用 StateTree)
2. 所有测试状态都使用这个 StateTree
3. 运行 Phase 1 测试
4. 观察日志，验证基本流程

**预期时间**: 30 分钟

### E.2 完整测试（推荐配置）

**目标**: 验证所有功能

**步骤**:
1. 创建所有 4 个专用 StateTree
2. 配置数据表引用
3. 按 Phase 1-4 顺序测试
4. 记录测试结果

**预期时间**: 2-3 小时

### E.3 深度测试（完整配置）

**目标**: 包含自定义 Tasks

**步骤**:
1. 实现 3 个自定义 Tasks
2. 在 StateTree 中使用自定义 Tasks
3. 运行完整测试
4. 分析详细日志

**预期时间**: 4-6 小时

---

**文档更新完成！**

