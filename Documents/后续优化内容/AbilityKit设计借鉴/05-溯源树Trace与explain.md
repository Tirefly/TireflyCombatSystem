# 借鉴 05：Trace 溯源树与 explain

## 概述

AbilityKit 的 `com.abilitykit.trace` 模块是一套运行时**溯源树**，追踪技能、效果、Action、投射物、Buff 等来源上下文与父子关系，并提供 explain 可解释输出。这让一次战斗结果可以反查到"哪个技能、哪个触发器、哪个 Action、哪个目标"。

TCS 当前有 `FTcsSourceHandle`（Id+CausalityChain+Instigator+SourceTags）作为线性单点归因，缺少树状血缘与可解释输出。这是一项低风险高价值的增量。

## AbilityKit 设计思想与架构实践

### 模块定位

AbilityKit 把 `trace` 列为核心基础设施模块，与 `core` / `gameplaytags` / `modifiers` / `attributes` / `diagnostics` 并列（`README.md:226-227`）：

> `com.abilitykit.trace` - 溯源树运行时，用于追踪技能、效果、Action、投射物、Buff 等来源上下文和父子关系。

### 端到端能力管线的血缘

AbilityKit 在"能力管线"描述里强调运行实例与溯源（`README.md:82-85`）：

> 技能释放产生的投射物、区域、Buff 和后续伤害保留 root/parent/owner context，方便战斗归因、调试和回放。

整条管线（`README.md:91-100`）：

```
可读配置 / Trigger Plan
  → 强类型 Action Schema
  → ExecCtx / World Service 上下文解析
  → 技能、投射物、召唤等参数 Modifier
  → 技能 Runtime / Origin / Trace 血缘
  → 投射物、区域、Buff、Continuous 生命周期
  → 业务事件处理与 Snapshot 输出
  → 纯 C# 验收、回放、同步或 Unity 表现消费
```

### explain 框架

AbilityKit 还独立提出 `com.abilitykit.ability.explain`（`README.md:253`）：

> 技能解释/调试框架：Forest、Tree + Navigation Protocol。

这把溯源从"日志"提升为"可导航的结构化诊断树"，能回答"这次伤害是怎么发生的"。

### 框架价值

- **战斗归因**：调试时反查"哪个技能哪个触发器哪个 Action 哪个目标"。
- **回放与验收**：纯 C# 验收、回放、同步可通过 TraceTree 反演。
- **跨来源复用**：同一 `giveDamage` / `addBuff` / `shootProjectile` Action 被多来源复用时，TraceTree 保留原始上下文。

## 与 TCS 现状的对比

### TCS 当前的 SourceHandle

TCS 的 `FTcsSourceHandle` 是全局唯一单调递增 Id + CausalityChain + Instigator + SourceTags（TCS 子代理报告 §2.2）。

证据：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem\Source\TireflyCombatSystem\Public\TcsSourceHandle.h:11-65`。

- Id：全局唯一，-1 为无效。
- CausalityChain：使用 `FPrimaryAssetId`，兼容所有 `UPrimaryDataAsset` 子类。
- 跨 Actor 修改必须改走"施加独立 State"，由目标 State 的 SourceHandle 管理其 Modifier（`project.md:152-156, 225`）。

### SourceHandle 网络化

`FTcsSourceHandle` 已实现 `NetSerialize` 与 `WithNetSerializer`，但这是结构序列化能力，不等于完整网络同步实现（TCS 子代理报告 §2.2、§3.7）。

证据：`TcsSourceHandle.h:108-150`。

### 差异点

- **结构**：TCS 的 SourceHandle 是"单点归因"——每个效果自带来源描述，但没有"父子树"。
- **回溯**：TCS 顺着 SourceHandle 可找到 Instigator 与 CausalityChain，但不能反查"这个 SourceHandle 又派生出了哪些子效果"。
- **可解释输出**：TCS 没有 explain 框架，调试时只能查日志，不能导航式浏览血缘。
- **回放**：TCS 没有 TraceTree 驱动的回放能力。

### 相似点

- 两者都把"来源"作为效果施加的一等公民。
- 两者都用 GameplayTag 作为来源标签。
- 两者都强调"跨 Actor 的效果施加必须可归因"。

## 借鉴建议

### 1. 在 SourceHandle 之上引入"溯源树"capability

不替换 `FTcsSourceHandle`，而在其之上引入"溯源关系"：

```cpp
// 仅作设计示意
struct FTcsSourceLineage
{
    FTcsSourceHandle This;              // 当前 SourceHandle
    FTcsSourceHandle Parent;           // 直接父来源（如触发的 Action 来自的 Skill）
    TArray<FTcsSourceHandle> Children; // 派生子效果（如 Skill 派生的投射物/Buff）
    FName OriginKind;                  // 来源类型：Skill/Trigger/Projectile/Area/BuffTick
};
```

运行时维护一个全局或按 Actor 的 `LineageIndex`，由事件施加时建立父子链接。

### 2. 引入 explain 框架作为调试能力

借鉴 AbilityKit 的 Forest/Tree + Navigation Protocol，在 TCS 编辑器期引入：

- 一个 `UTcsTraceTreeView` 调试资产或窗口，显示某次战斗的 SourceHandle 血缘树。
- 节点可点击跳转到对应 Skill / Buff / Action 在编辑器中的资产。
- 与 TCS 已有的 `TcsConsoleCommands` / `TcsConsoleCommandRuntime` 集成（TCS 子代理报告 §3.8 debug-console-surface）。

### 3. 分阶段推进

- **阶段 A**：仅维护父子链接（Parent / Children），不引入 explain UI。在 SourceHandle 创建 / 移除时建立 / 断开链接。低风险。
- **阶段 B**：引入 explain 调试窗口，可视化血缘树。
- **阶段 C**：与 Snapshot / 回放联动，支持战斗回放时按血缘回放。

### 4. 与网络身份设计对齐

`runtime-network-identity` spec 已规划 `DefId / 实例级 authority 身份（StateInstId / AttrModInstId / SkillModInstId）/ SourceHandle / PredictionKey` 分层（TCS 子代理报告 §2.2）。

证据：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem\openspec\specs\runtime-network-identity\spec.md:8-62`。

血缘树应作为网络身份的"扩展维度"——同一运行实例在不同权威域（Client/Server）的 Parent/Children 关系需要保持一致或显式标记为"仅本地诊断"。

## 风险与前置条件

- **风险：低到中**。血缘链接是 SourceHandle 之上的增量，不破坏现有八步移除时序与跨 Actor 独立 State 约束。
- **前置条件**：
  - SourceHandle 机制完成度 100%（`project.md:210-218`），是引入血缘的好时机。
  - 需评估血缘链接在 State 移除时的清理时序，避免悬挂引用。
  - 需确认血缘不破坏 `FTcsSourceHandle` 的 `NetSerialize` 契约——血缘应作为本地诊断数据，不进网络序列化（除非 explain 要支持跨端）。
- **落地形式**：
  1. 先做 design.md 评估血缘数据结构与存储位置（Component / Subsystem）。
  2. 创建 OpenSpec capability `add-source-lineage`。
  3. 分阶段落地（A→C），UI 与回放可后置。

## 不建议的做法

- **不要**把血缘树塞进 `FTcsSourceHandle` 结构体本身。SourceHandle 应保持轻量、可网络序列化；血缘树是上层诊断数据，应分开。
- **不要**让血缘链接成为运行时正确性依赖。血缘是诊断 / 调试 / 回放辅助，业务逻辑不应依赖 Clan 血缘查询判断（避免强耦合）。
- **不要**默认记录全部历史血缘。需设计裁剪策略（如只保留最近 N 帧、只保留存活效果的血缘），避免内存膨胀。

## 参考

- AbilityKit: `README.md:226-227`（trace 模块）
- AbilityKit: `README.md:82-85`（运行实例与溯源）
- AbilityKit: `README.md:91-100`（端到端能力管线）
- AbilityKit: `README.md:253`（ability.explain）
- TCS: `Plugins/TireflyCombatSystem/Source/.../Public/TcsSourceHandle.h:11-150`
- TCS: `Plugins/TireflyCombatSystem/openspec/project.md:152-156, 225`
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/runtime-network-identity/spec.md`
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/debug-console-surface/spec.md`