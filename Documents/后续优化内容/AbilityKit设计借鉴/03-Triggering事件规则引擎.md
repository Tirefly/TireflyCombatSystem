# 借鉴 03：Triggering 事件规则引擎

## 概述

AbilityKit 的 `com.abilitykit.triggering` 是一个独立的事件规则执行引擎，回答"**战斗事件发生后，按什么规则执行什么动作**"。它把主动技能、被动、Buff Tick、投射物命中、区域进入/离开等统一成"事件→排序→条件→Action→Cue/Trace"链路。

TCS 当前事件是 State 生命周期内的零散通知，没有独立的事件规则层。借鉴此模型对 TCS 接下来推进"被动触发/AI 触发/区域效果"子域价值最高、时机最好（Skill 系统完成度仅 35%）。

## AbilityKit 设计思想与架构实践

### 定位

Triggering 不是简单事件总线，而是承接所有战斗事件的规则执行层（`README.md:344-385`）。事件来源包括：

```
技能阶段 / Timeline  /  被动 / 属性变化  /  Buff / Continuous Tick
投射物 Hit / Exit    /  区域 Enter / Stay / Exit
```

### 执行链路

AbilityKit 的执行链路（`README.md:348-379`、`AbilityKit_vs_GAS_Comparison.md:223-309`）：

```
事件来源 → EventBus / DirectTrigger
        → TriggerRunner（Phase → Priority → Order 排序）
        → ExecCtx<TCtx>（注入 World Services / EventBus / Registries）
        → TriggerPlan / Executable Tree（可执行节点树）
        → Conditions（Payload / Blackboard / NumericExpr）
        → ActionCallPlan（强类型 Action 调用）
        → ActionSchemaRegistry（validate / resolve args）
        → PlannedTriggerActionExecutor（执行）
        → 解析为 giveDamage / addBuff / shootProjectile / playCue
        → Cue / Lifecycle / Trace 注入
```

### 核心抽象

#### TriggerRunner

`TriggerRunner<TCtx>` 负责事件订阅、触发器排序、条件评估、执行控制、生命周期通知、ActionScheduler 推进（`README.md:381`、`AbilityKit_vs_GAS_Comparison.md:276-309`）。

排序双重维度：Phase + Priority。Phase 是粗粒度分组（如前置/主效果/后置），Priority 是同 Phase 内细粒度。

#### 分层触发器

AbilityKit 独有的是**分层 TriggerRunner**（`AbilityKit_vs_GAS_Comparison.md:286-309`）：

```csharp
public readonly struct HierarchicalOptions
{
    public bool ExecuteParentFirst;       // 父级先执行
    public bool ShortCircuitStopsParent;  // 短路停止父级
    public bool PropagateToParent;        // 向上传播
}
```

子级可覆盖/补充父级规则。典型场景：技能自身 Trigger 覆盖全局被动规则。

#### ExecCtx

`ExecCtx<TCtx>` 是执行上下文，注入 EventBus、FunctionRegistry、ActionRegistry、BlackboardResolver、PayloadAccessorRegistry、NumericDomains、ExecutionControl（`AbilityKit_vs_GAS_Comparison.md:239-251`）。

ExecutionControl 负责中断、跳过、短路执行控制，保证规则链可控停止。

#### TriggerPlan

`TriggerPlan` 把配置化规则表达成可执行节点树，由条件节点和 Action 节点组成。Action 通过 `ActionSchemaRegistry` 校验参数，由 `PlannedTriggerActionExecutor` 落到强类型运行时代码。

### 框架价值

AbilityKit 强调"主动技能、被动技能、Buff、投射物、区域、表现 Cue 共享同一套'事件→条件→Action→服务'执行模型"（`README.md:383`）。新增一个 `giveDamage` / `addBuff` / `shootProjectile` Action 后，可被多个玩法来源复用；Trace、Cue、执行控制也不需要每个业务系统重复实现。

## 与 TCS 现状的对比

### TCS 当前事件模型

TCS 的事件是 State 生命周期内的零散通知（TCS 子代理报告 §3.6）：

- `NotifyStateStageChanged`、`NotifyStateRemoved`、`NotifyStateApplySuccess` / `NotifyStateApplyFailed`（带 `ETcsStateApplyFailReason`）。
- `TcsAttributeChangeEventPayload`、`TcsBuffChangeEventPayload` 是事件载荷。
- 消息路由依赖引擎 `GameplayMessageRuntime`（`Build.cs:36`）。

事件被广播出去后，由订阅者自行处理，没有统一的"条件→Action"规则层。

### 差异点

- **维度**：TCS 是"状态广播通知"；AbilityKit 是"事件规则执行"，两者职责不同。
- **复用**：TCS 每个玩法来源（Buff/Skill/被动）需各自处理事件；AbilityKit 统一规则层复用 Action。
- **排序**：TCS 没有跨触发器的排序机制；AbilityKit 有 Phase+Priority。
- **控制流**：TCS 通知发出即结束；AbilityKit 有 ExecutionControl 中断/短路。

### 相似点

- 两者都用 GameplayTag 作为状态标识与过滤手段。
- 两者都依赖外层消息路由（TCS 用 `GameplayMessageRuntime`，AbilityKit 用 `EventBus`）。

### TCS 缺口

TCS 在以下子域目前缺少统一规则层：
- **被动触发**：属性变化触发、受击触发、击杀触发、血量阈值触发，散落在各业务逻辑。
- **区域效果**：`Enter/Stay/Exit` 与区域内 Tick 缺乏统一执行模型。
- **AI 触发**：AI 决策触发的技能/被动没有共用规则层。

## 借鉴建议

### 1. 引入"事件规则层"作为 TCS 新 capability

建议创建 OpenSpec capability `add-event-rule-engine`（或类似命名），在 TCS 增设介于 StateComponent 与业务代码之间的"事件规则层"：

```
事件来源（State 通知 / 属性变化 / 区域事件）
  → 事件规则层（TriggerRunner 等价物）
  → 排序（Phase + Priority）
  → 条件评估（Tag / 属性阈值 / 黑板）
  → Action 执行（强类型 Action Registry）
  → 统一 Cue / Trace 钩子
```

#### 阶段化推进

- **阶段 A**：只在"被动触发"子域引入规则层，不涉及现有 Buff/Skill 主路径。对应"被动效果规则"capability。
- **阶段 B**：扩展到"区域效果"，把 `Enter/Stay/Exit` 收进规则层。
- **阶段 C**：扩展到投射物命中、AI 触发。

### 2. 强类型 Action Registry

借鉴 AbilityKit 的 `ActionSchemaRegistry` + `PlannedTriggerActionExecutor`（见借鉴 06），在 TCS 引入"Action 定义 → 参数校验 → 服务解析 → 执行"链路，避免散落字符串脚本。

### 3. 评估 TriggerPlan 数据结构在 TCS 中的形态

TCS 是 UE 原生，TriggerPlan 可考虑：
- 基于 `UObject` 派生的 `UTcsActionNode` 树，编辑器可视化。
- 或基于 UE DataAsset 的 `UTcsTriggerPlanAsset`，数据驱动。
- 不沿用 AbilityKit 的纯 C# `TriggerPlan` 结构。

### 4. 评估"分层规则"在 TCS 的对应

TCS 的"跨 Actor 效果施加必须改走独立 State"约束（`project.md:152-156, 225`）与 AbilityKit 的"分层规则"有联系但不等同。可将"实体全局规则"与"技能局部规则"作为父子层级，技能触发可短路全局被动规则。

## 风险与前置条件

- **风险：中高**。事件规则层是架构性增量，会改变 TCS 的运行时拓扑。需 OpenSpec 评审。
- **前置条件**：
  - TCS Skill 系统完成度 35%（`project.md:215`），是引入规则层的有利时机——Skill 主路径还没冻结。
  - `runtime-network-identity` 设计约束需要先评估规则层在网络下的位置（事件链是否权威、是否预测）。
  - 需先确认规则层不破坏 StateTree 双层架构（`project.md:147-150`）：规则层是 State 之外的事件执行层，不应侵入 State 内部 StateTree。
- **落地形式**：
  1. 先做 design.md 评估与现有 `NotifyStateStageChanged` 的关系。
  2. 创建 OpenSpec capability `add-passive-trigger-rule-engine`（仅被动子域）。
  3. 先做最小可用版本（只支持 Tag 条件 + 受击事件 + 单 Action），再扩展。

## 不建议的做法

- **不要**一次性覆盖所有事件来源（Buff Tick / 投射物命中 / 区域）。先从"被动触发"子域切入，避免破坏现有八步移除时序。
- **不要**让规则层绕过 TCS 的"跨 Actor 效果必须走独立 State"约束。规则层执行的 Action 仍要施加独立 State 而非直接修改目标 Actor。
- **不要**用纯 C# `TriggerPlan` 结构。TCS 是 UE 原生，结构应可反射、可 Blueprint friendly。

## 参考

- AbilityKit: `Docs/游戏效果系统统一抽象.md`（统一执行模型）
- AbilityKit: `Docs/AbilityKit_vs_GAS_Comparison.md`（Trigger 模块对比）
- AbilityKit: `README.md:344-385`（Triggering 设计理念）
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/state-management/spec.md`
- TCS: `Plugins/TireflyCombatSystem/openspec/project.md`（完成度、跨 Actor 约束）
- TCS: `Plugins/TireflyCombatSystem/Source/.../TireflyCombatSystem.Build.cs:36`（GameplayMessageRuntime 依赖）
- TCS 借鉴 06：`06-强类型ActionSchema与ActionCallPlan.md`（强类型 Action）