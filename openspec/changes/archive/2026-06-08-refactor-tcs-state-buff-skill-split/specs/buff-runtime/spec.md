## ADDED Requirements

### Requirement: Buff 应拥有时效语义
TCS SHALL 将持续时间视为 Buff 自有的数据与运行时行为，而不是通用 `State Core` 字段。

#### Scenario: Buff 的持续时间应配置在 Buff 自有类型上
- **WHEN** 设计者为一个 Buff 配置有限持续时间
- **THEN** 持续时间相关字段应声明在 `UTcsBuffDefinition` 或 Buff 自有策略/片段上
- **AND** 抽象的 `UTcsStateDefinition` 不应继续承载 `DurationType` 或 `Duration`

#### Scenario: 有时效的 Buff 应由 Buff 运行时驱动其耗尽行为
- **WHEN** 一个有限时长的 Buff 到期
- **THEN** Buff 自有运行时状态应决定其何时耗尽以及如何处理耗尽后的策略
- **AND** `State Core` 只应提供共享的移除和生命周期宿主入口

### Requirement: Buff 应拥有叠层与合并语义
TCS SHALL 把叠层与合并行为保留在 Buff 自有运行时模型中，而不是塞进通用 State 级 API。

#### Scenario: Buff 的叠层变化应使用 Buff 自有语义
- **WHEN** 同一个 Buff 的多次应用发生交互
- **THEN** 叠层数、叠层耗尽以及合并策略都应由 Buff 自有运行时规则解析
- **AND** 通用 `State Core` 契约不应假设所有运行时状态都支持叠层

#### Scenario: Buff 的合并规则不应停留在共享基类定义上
- **WHEN** 一个 Buff 定义声明了重复应用如何合并
- **THEN** 这类合并配置应存在于 Buff 自有定义数据上
- **AND** 共享基类定义应继续保持不承载合并专属配置

### Requirement: Buff 应通过共享宿主扩展 State
TCS SHALL 保持 `UTcsStateComponent` 为权威共享 State 宿主，同时允许 Buff 专属运行时能力存在于 Buff 自有 helper 和协作型 Buff 组件中。

#### Scenario: Buff 专属运行时应与共享宿主协作
- **WHEN** Buff 运行时增加 Period、Refresh、Expire 或 Dispel 策略
- **THEN** 这些策略应由 Buff 自有处理器/helper、Buff 自有运行时类型，或一个协作型 `UTcsBuffComponent` 实现
- **AND** `UTcsStateComponent` 仍应保持对共享 apply/remove/lifecycle 入口的权威性
- **AND** `UTcsBuffComponent` 不应以一套平行的 apply/remove/lifecycle 框架取代共享 State 宿主