# buff-merge-runtime Specification

## Purpose
TBD - created by archiving change optimize-buff-merge-runtime. Update Purpose after archive.
## Requirements
### Requirement: 槽位级 Buff Merge Group 运行时模型
TCS SHALL 按 `FTcsStateSlot` 维护持久化的 Buff merge group 运行时状态，而不是在每次 merge 处理时都从头重建所有 Buff group。

#### Scenario: 槽位成员关系通过 group 运行时状态表达
- **WHEN** Buff 实例进入或离开某个 slot
- **THEN** slot 应当更新受影响 `StateDefId` 对应的 Buff merge group 运行时状态
- **AND** 后续 merge 处理应当可以直接处理该 group，而不需要先把整个 slot 里的 Buff 重新分组一遍

#### Scenario: 强制重建会重新同步槽位本地运行时
- **WHEN** 槽位本地 Buff merge 运行时被标记为需要重建
- **THEN** 运行时应当在处理脏 group 之前，根据当前 slot 状态重建 group 成员关系
- **AND** 这条重建路径应当继续兼容共享 State 移除生命周期

### Requirement: 显式的 Buff Merge 脏状态跟踪
TCS SHALL 通过脏 group 记账与脏原因标记显式追踪 Buff merge 失效。

#### Scenario: 空闲 merge 处理不再触发重新分组
- **WHEN** 某个 slot 没有脏 Buff merge group，且也不需要重建
- **THEN** `ProcessBuffMerging()` 应当直接返回，而不是再去重分组整个 slot

#### Scenario: 一个 group 可以累积多个脏原因
- **WHEN** 同一个 Buff merge group 在被处理前因为多个原因变得失效
- **THEN** 运行时应当保留这些失效原因，直到该 group 被处理
- **AND** 后来的原因不应悄悄覆盖之前的原因

### Requirement: Merger 依赖声明
TCS SHALL 允许每个 Buff merger 声明哪些运行时输入会影响其正确性。

#### Scenario: 内建 merger 声明精确依赖
- **WHEN** 运行时解析到 `UseNewest`、`UseOldest`、`NoMerge`、`StackByInstigator` 这类内建 merger
- **THEN** merger 应当暴露依赖标记，说明哪些脏原因会要求它重新处理

#### Scenario: 保守默认值保证自定义 merger 的正确性
- **WHEN** 某个自定义 merger 或 Blueprint merger 没有覆盖依赖声明 API
- **THEN** 运行时应当回退到一个保守的依赖集合
- **AND** 应当优先保证正确性，而不是追求最大化优化收益

### Requirement: 脏原因只重新处理相关 Group
TCS SHALL 只在脏原因与当前 merger 的依赖标记相交，或强制重建明确要求时，才重新处理脏 Buff merge group。

#### Scenario: 不相关的脏原因不应强制重新 merge
- **WHEN** 某个 group 变脏的原因并不在当前 merger 的依赖集合内
- **THEN** 当前处理轮次中，运行时应当跳过对该 merger 的重新执行

#### Scenario: 相关脏原因会触发重新 merge
- **WHEN** 某个 group 因当前 merger 依赖的原因而变脏
- **THEN** 运行时应当在当前处理轮次中对该 group 重新执行 merger

### Requirement: 现有 Merge 输出协议保持稳定
TCS SHALL 在优化运行时编排时保持当前 merger 的输入/输出契约不变。

#### Scenario: 被合并淘汰的 Buff 仍然走共享移除链路离场
- **WHEN** 某次 merge 处理判定一个或多个 Buff 实例被 merged out
- **THEN** 运行时仍应当通过 `MergedOutBuffs` 暴露这些实例
- **AND** 它们的移除仍应继续走共享的 `UTcsStateComponent` 移除路径，而不是引入旁路通道

### Requirement: 运行时敏感的 Buff Merge 失效
TCS SHALL 支持来自成员关系变化之外的、对运行时敏感的 Buff merge 失效。

#### Scenario: 对 stack 敏感的 merger 会响应运行时 stack 变化
- **WHEN** 某个 Buff merge group 中的 merger 依赖运行时 stack 值
- **THEN** 相关 Buff 实例的 stack 变化应当将该 group 标记为脏，以便重新处理

#### Scenario: 对 stage 敏感的 merger 会响应执行阶段变化
- **WHEN** 某个 Buff merge group 中的 merger 依赖执行阶段
- **THEN** 相关阶段变化应当将该 group 标记为脏，以便重新处理

#### Scenario: 对 gate 敏感的 merger 会响应槽位 gate 变化
- **WHEN** 某个 Buff merge group 中的 merger 依赖 slot gate 状态
- **THEN** 相关 slot gate 变化应当将该 group 标记为脏，以便重新处理

