# state-runtime-core Specification

## Purpose
TBD - created by archiving change refactor-tcs-state-buff-skill-split. Update Purpose after archive.
## Requirements
### Requirement: 抽象的 State Definition 基类
TCS SHALL 将 `UTcsStateDefinition` 建模为一个抽象共享契约，并且它只包含每一种运行时状态类型都需要的数据。

#### Scenario: 裸 StateDefinition 不再被视为最终具体运行时定义类型
- **WHEN** 在代码中建模共享 `State Core` 定义数据时
- **THEN** 裸 `UTcsStateDefinition` SHALL 被视为抽象共享契约，而不是最终的具体运行时定义类型
- **AND** 可供 authoring 的具体定义 SHALL 由共享基类之外的派生类型提供

#### Scenario: 共享 state 基类排除子类专属字段
- **WHEN** 在 `UTcsStateDefinition` 上声明共享 `State Core` 数据时
- **THEN** `DurationType`、`Duration`、`MaxStackCount`、`MergerType` 等 Buff-only 字段 SHALL NOT 存在于抽象基类上
- **AND** learned/cooldown/snapshot 等 Skill-only 字段 SHALL NOT 存在于抽象基类上

### Requirement: 由具体类型替代通用 StateType 分类
TCS SHALL 通过具体定义类型表达运行时定义语义，而不是依赖共享的 `ETcsStateType` / `StateType` 分类字段。

#### Scenario: 共享基类不依赖通用状态类型枚举
- **WHEN** 对 `State Core` definitions 进行重构时
- **THEN** 共享定义基类 SHALL NOT 依赖 `ETcsStateType` 或 `StateType` 来区分 Buff、Skill 或其他派生语义
- **AND** 具体派生定义类型 SHALL 自己负责表达其语义角色

### Requirement: 具体的 State 宿主组件
TCS SHALL 保持 `UTcsStateComponent` 作为面向 Actor 的具体宿主，用于应用、移除、查询、分槽以及驱动运行时状态实例。

#### Scenario: Actor 保持单一的具体状态宿主
- **WHEN** 一个实体参与 TCS 状态运行时
- **THEN** 该实体 SHALL 继续暴露一个具体的 `UTcsStateComponent`
- **AND** Buff 与 Skill 的运行时集成都 SHALL 通过这个宿主完成，而不是要求平行宿主组件

#### Scenario: State Core 保留共享编排职责
- **WHEN** 一个 Buff 运行时状态或由 Skill 触发的运行时状态进入激活态
- **THEN** 共享的 slot 分配、生命周期编排与 StateTree 驱动 SHALL 继续由 `State Core` 提供

### Requirement: 当前阶段的 Skill 初步规模允许进入作者工作流
TCS SHALL 继续将 `UTcsSkillDefinition` 与 `UTcsSkillModifierDefinition` 视为仍在演进中的能力，但当前插件进度已经允许它们进入编辑器 authoring、Definition 管理与 DataTable 同步工作流。

#### Scenario: Skill Def 可以参与编辑器 authoring 与同步能力
- **WHEN** 在编辑器中为 TCS 定义资产建立 authoring 或同步工作流时
- **THEN** `UTcsSkillDefinition` 与 `UTcsSkillModifierDefinition` SHALL 被视为可纳入的具体 DefinitionAsset 类型
- **AND** 不应再因为历史阶段约束而将其排除在该类工作流之外

#### Scenario: 现有 Skill 骨架保持兼容
- **WHEN** `State Core`、Buff 与 Skill 的共享边界继续演进时
- **THEN** 现有 Skill 模块骨架及其当前调用路径 SHALL 保持可编译兼容
- **AND** Skill 能参与 editor authoring / sync 工作流这一事实，不等于其运行时分层已经被冻结为长期最终模型

### Requirement: 共享参数策略默认不视为 Skill 专属
TCS SHALL 将快照这类参数求值时机策略视为共享参数系统关注点，除非已被证明只属于 Skill。

#### Scenario: 支持快照的参数继续可供 Buff 使用
- **WHEN** 一个 Buff 需要在激活时快照与实时重算参数之间做选择
- **THEN** 参数系统 SHALL 能表达这种选择，而不要求该能力必须存在于 Skill-only 模块中

#### Scenario: 共享参数策略不会被错误标记为 Skill-only
- **WHEN** 在这次重构中审视共享参数元数据时
- **THEN** 与 snapshot 相关的参数语义 SHALL NOT 被默认归类为 Skill-only
- **AND** 任何暗示其归属为 Skill-only 的现有实现注释或结构，都 SHALL 被视为待修正对象

### Requirement: 本次重构期间 StateTree Schema 暴露面保持可显式调整
TCS SHALL 将当前 `UTcsStateInstance` 到 `StateTreeSchema` 的暴露面视为暂定设计，直到最小共享运行时上下文集合被定义清楚。

#### Scenario: 实验性上下文暴露不会被冻结成正式契约
- **WHEN** 当前 `UTcsStateInstance` 字段通过 `StateTreeSchema` 暴露时
- **THEN** 这类暴露 SHALL NOT 自动被视为已经定稿的长期契约
- **AND** 在正式固化之前，本次重构 SHALL 被允许收窄或重塑共享上下文暴露面

