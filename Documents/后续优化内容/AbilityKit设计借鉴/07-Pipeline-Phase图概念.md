# 借鉴 07：Pipeline Phase 图概念

## 概述

AbilityKit 的 `com.abilitykit.pipeline` 把技能流程建模为 **Phase 图**：基础阶段负责动作、延迟、等待、时间轴；组合阶段负责 Sequence/Parallel/Conditional/Repeat/Delay 嵌套。这让技能流程不止"冷却→吟唱→施法→后摇"线性序列，而是任意结构化组合。

TCS 当前用 UE StateTree 驱动技能流程。StateTree 本身已是强模型，但对于复杂多段技能、吟唱/引导、蓄力松手释放、并行表现与逻辑等场景，Phase 图的显式组合更直观。**风险最高的一篇借鉴**：可能与 UE StateTree 重叠，需谨慎权衡。

## AbilityKit 设计思想与架构实践

### 核心定位

AbilityKit 把 Pipeline 与 Triggering 区分明确（`README.md:306-340`）：

- **Pipeline** 回答"技能流程如何被编排、等待、分支、并行、嵌套"。
- **Triggering** 回答"事件发生后执行哪些规则和动作"。

二者协作：Pipeline 负责"流程什么时候推进"，Triggering 负责"事件发生后执行什么"。

### 核心抽象

`IAbilityPipelinePhase<TCtx>` 是最小执行单元（`README.md:334`）：

```csharp
public interface IAbilityPipelinePhase<TCtx>
{
    void Execute(TCtx context);
    void OnUpdate(TCtx context, float deltaTime);
    bool IsComplete { get; }
    void Reset();
}
```

`AbilityCompositePhase<TCtx>` 让 Sequence/Parallel/Conditional 等复合阶段递归嵌套。`IInterruptiblePhase<TCtx>` / `IDurationalPhase<TCtx>` / `IPhaseInstanceFactory<TCtx>` 解决中断、持续时间、运行实例克隆。

### 内置阶段类型

借鉴 04 已列举（`AbilityKit_vs_GAS_Comparison.md:594-619`）：

| 阶段类型 | 说明 |
|----------|------|
| AbilityInstantPhaseBase | 瞬时执行 |
| AbilityDurationalPhaseBase | 持续执行（OnUpdate 驱动） |
| AbilityInterruptiblePhaseBase | 可中断 |
| AbilityTimelinePhase | 时间轴 |
| AbilitySequencePhase | 序列 |
| AbilityParallelPhase | 并行 |
| AbilityConditionalPhase | 条件 |
| AbilityDelayPhase | 延迟 |

### 条件与等待

`AbilityConditionalPhase` 支持多分支、`OnEnter` / `Continuous` 条件检查、无命中时 `Wait/Complete/Fail/Skip`（`README.md:336`）。`AbilityGatePhase` 做入口门控；`AbilityWaitUntilPhase` 等待条件 / 超时；`AbilityRepeatPhase` 重复执行并设间隔。

### 与 Triggering / Behavior 的关系

Pipeline 负责"流程什么时候推进"，复杂规则与效果执行通过 Triggering 或业务 Phase 接入（`README.md:338`）。复杂行为通过 `AbilityBehaviorPhase` 嵌入 Pipeline（借鉴 04 提到的 behavior 模块）。

### 适用场景

AbilityKit 列举（`README.md:340`）：
- 技能释放流程（预施法、吟唱、引导、施法、后摇）。
- 条件化连招、多段技能、蓄力/松手释放。
- 等待外部信号、并行表现与逻辑。
- 重复波次、Timeline 事件。
- 复杂 AI / 行为树阶段。
- 与 TriggerPlan 组合的配置化技能流程。

## 与 TCS 现状的对比

### TCS 当前的 StateTree 执行

TCS 用 UE StateTree 驱动 State/Buff/Skill 执行（TCS 子代理报告 §2.3、§3.5）：

- 第一层（静态）：StateTree 管理状态槽位、转换规则，编辑器可视化配置。
- 第二层（动态）：每个 `UTcsStateInstance` 或派生执行态运行独立 StateTree 执行具体逻辑。
- Schema 分三类：`UTcsSTSchema_Buff` / `UTcsSTSchema_Skill` / `UTcsSTSchema_StateComponent`。

证据：`project.md:147-150`；`specs/state-runtime-access/spec.md:6-58`；`Public/StateTree/Schema/`。

### StateTree 能力

UE StateTree 本身支持：
- States 与 Transitions（含条件、延迟、优先级）。
- Tasks（强制类型、可 Blueprint）。
- Evaluator / Global Evaluator。
- Linked SubTree（TCS 已记录"统一支持方向"，`state-runtime-access/spec.md:67-79`）。
- 编辑器可视化。

### 差异点

- **模型**：AbilityKit Phase 是"图状节点组合"（Sequence/Parallel/Conditional 树形组合）；StateTree 是"状态转换图"（State 间 Transitions）。
- **等待/分支**：Phase 图显式建模 WaitUntil / Conditional；StateTree 靠 Transition 条件表达等待。
- **并行**：Phase 图显式 Parallel；StateTree 用并行 State 表达。
- **重复**：Phase 图有 Repeat Phase；StateTree 靠 Transition 回环表达。
- **中断**：Phase 图有 OnInterrupt；StateTree 靠 RequestedStop / Transition 表达（注意 UE 5.7 `RequestedStop` 延迟自保护，`project.md:226`）。

### 相似点

- 两者都能表达"分段、等待、并行、重复"概念，只是表达力不同。
- 两者都有强类型、可中断、可 Tick 驱动。

### 关键问题

AbilityKit Phase 与 UE StateTree **重叠度高**。如果 TCS 引入 Phase 图，会与 StateTree 形成两套技能流程引擎，造成：
- 编辑器 authoring 双轨：开发者既要用 StateTree Editor，又要用 Phase 图 Editor。
- 运行时双引擎：StateTree 跑状态转换，Phase 图跑节点组合，互相调用复杂。
- 维护成本翻倍。

## 借鉴建议

### 0. 谨慎结论：不要全盘引入 Phase 图

最关键建议：**不要把 AbilityKit Phase 图作为 TCS 的主技能流程引擎**。会与 UE StateTree 重叠且破坏现有 90% 完成度的 State 系统。

### 1. 把 Phase 图作为"设计参考语言"

借鉴 AbilityKit 的 Phase 类型词汇，作为描述复杂技能流程的**设计语言**，不作为运行时引擎：

- 用 Sequence/Parallel/Conditional/Repeat/Delay/WaitUntil 词汇描述某个 Skill 的流程。
- 落地时仍用 StateTree Transitions + Tasks 表达这些词汇的语义。
- 在 `UTcsSkillDefinition` 或文档中标注"这个技能流程是 Sequence + Delay + Conditional"等描述。

### 2. 补强 StateTree 缺失能力

如果 StateTree 在某些场景表达不直观，评估在 StateTree 内补强而非引入 Phase 图：

- **Timeline 事件**：用 StateTree Evaluator + 时间条件表达（TCS 已有 `FTcsSTEvaluator_BuffPeriod` 周期 Evaluator，`buff-runtime/spec.md:98-122`）。
- **并行多效果**：用并行 State 表达。
- **蓄力松手释放**：用 Transition 监听松手事件表达。
- **等待外部信号**：用 Transition 等待事件表达。

### 3. 仅在"StateTree 明显不擅长"的子域评估

StateTree 弱于：
- 跨多阶段的复杂数据流（前一阶段输出喂给后一阶段）——Phase 图有显式上下文传递。
- 流程嵌套深度大、跨子流程回调复杂——Phase 图有 FlowContext 作用域（借鉴 04）。

但这些场景 TCS 当前是否大量出现？若是少数特例，StateTree 仍可表达，不值得引第二引擎。只有大量出现，且 StateTree 表达成本高时，再评估引入局部 Phase 子引擎。

### 4. Flow 模块独立评估

AbilityKit 还有独立 `com.abilitykit.flow`（IFlowNode 节点树、WAKE/PUMP、FlowContext、AwaitCompletionNode），用于异步技能演出序列、UI 动画编排、跨系统协调流程（`README.md:389-408`）。

这部分与 StateTree 重叠更小——Flow 解决"跨系统的异步协调"，StateTree 解决"实体状态转换"。如果 TCS 后期出现"战斗演出需要跨多个系统异步协调"场景，可独立评估借鉴 Flow，不引入 Pipeline。

## 风险与前置条件

- **风险：高**。直接引入 Phase 图会与 StateTree 重叠，破坏现有 90% 完成度的 State 系统。
- **前置条件**：
  - StateTree 集成完成度 85%（`project.md:215`），是 TCS 主路径，不可轻易改朝换代。
  - 引入 Phase 图前必须做 capability 评估与 POC，确认 StateTree 在哪些场景确实表达不动。
- **落地形式**：
  1. 暂不落地，作为"设计语言"吸收词汇。
  2. 若未来确实出现 StateTree 表达不动的场景，再单独 OpenSpec 提案评估局部 Phase 子引擎。
  3. Flow 异步协调可独立评估。

## 不建议的做法

- **不要**把 Phase 图作为 TCS 主技能流程引擎。会破坏现有 StateTree 架构。
- **不要**并行运行 Phase 图和 StateTree跑同一技能流程。会双引擎冲突。
- **不要**因为 AbilityKit 有 Phase 图就觉得 TCS 必须有。TCS 是 UE 原生，StateTree 是引擎同等能力，没必要复刻。

## 参考

- AbilityKit: `README.md:306-340`（Pipeline 设计理念）
- AbilityKit: `Docs/AbilityKit_vs_GAS_Comparison.md:536-619`（Pipeline 对比）
- AbilityKit: `README.md:389-408`（Flow 模块）
- TCS: `Plugins/TireflyCombatSystem/openspec/project.md:147-150`（StateTree 双层架构）
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/state-runtime-access/spec.md`
- TCS: `Plugins/TireflyCombatSystem/openspec/project.md:226`（StateTree RequestedStop 约束）
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/buff-runtime/spec.md:98-122`（Period Evaluator）