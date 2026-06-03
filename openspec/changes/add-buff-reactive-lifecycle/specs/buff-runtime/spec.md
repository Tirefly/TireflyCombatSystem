## ADDED Requirements

### Requirement: 可叠层 Buff 的增量反应策略应显式建模
TCS SHALL 仅为可叠层 Buff 暴露 Buff 自有的生命周期增量反应配置。

#### Scenario: 单层 Buff 隐藏并忽略增量反应策略
- **WHEN** 一个 `UTcsBuffDefinition` 的 `MaxStackCount <= 1`
- **THEN** 叠层相关的增量反应字段不应作为有效配置项呈现
- **AND** 即使旧数据仍保留了这些值，运行时也应忽略它们

#### Scenario: 可叠层 Buff 可以声明叠层上涨后的时长刷新行为
- **WHEN** 一个可叠层 Buff 获得新层数，且其 `OnStackIncrease.DurationPolicy` 为 `RefreshRemainingToTotal`
- **THEN** Buff 运行时应将剩余持续时间刷新为该 Buff 的总持续时间

### Requirement: 持续时间耗尽响应应在 Buff 定义层显式建模
TCS SHALL 允许具备有限持续时间且可叠层的 Buff 在 Buff 定义层声明持续时间耗尽后的处理方式；而有限持续时间的单层 Buff 则继续沿用默认的整 Buff 移除语义，无需显式配置 `ExpirationPolicy`。

#### Scenario: 有限时长的可叠层 Buff 可以在耗尽时整 Buff 移除
- **WHEN** 一个有限时长的可叠层 Buff 达到持续时间终点，且 `ExpirationPolicy` 为 `ClearEntireBuff`
- **THEN** Buff 运行时应通过共享移除链请求移除整个 Buff

#### Scenario: 有限时长的单层 Buff 使用默认的整 Buff 移除
- **WHEN** 一个有限时长 Buff 的 `MaxStackCount <= 1` 且到达持续时间终点
- **THEN** Buff 运行时应通过共享移除链移除整个 Buff
- **AND** 该 Buff 不应需要显式配置 `ExpirationPolicy` 才能表达这一行为

#### Scenario: 多层 Buff 可以在耗尽时只掉一层
- **WHEN** 一个有限时长 Buff 有多于一层叠层，且 `ExpirationPolicy` 为 `RemoveSingleStack`
- **THEN** Buff 运行时应减少一层叠层，而不是直接移除整个 Buff
- **AND** 当最后一层最终仍因持续时间耗尽离场时，这条路径不应错误地把最终离场原因报告为 `StackDepleted`

#### Scenario: 多层 Buff 可以在耗尽时掉一层并刷新持续时间
- **WHEN** 一个有限时长 Buff 有多于一层叠层，且 `ExpirationPolicy` 为 `RemoveSingleStackAndRefreshDuration`
- **THEN** Buff 运行时应减少一层叠层
- **AND** 应刷新仍然存活的 Buff 实例的剩余持续时间

#### Scenario: 无限时长 Buff 隐藏并忽略持续时间耗尽响应配置
- **WHEN** 一个 Buff 不使用有限持续时间
- **THEN** `OnDurationExpired` 相关配置应被隐藏或视为不适用
- **AND** 运行时不应尝试对该 Buff 求值持续时间耗尽策略

### Requirement: Buff 运行时应通过统一入口执行共享增量反应语义
TCS SHALL 通过 Buff 自有的统一运行时入口执行叠层上涨和持续时间耗尽反应，而不是把这些副作用硬编码进各个 merger，同时保持 `UTcsStateComponent` 为权威共享宿主。

#### Scenario: Buff 增量反应处理与共享宿主协作
- **WHEN** Buff 运行时处理叠层增长或持续时间耗尽反应时
- **THEN** 这些反应可以收口到 Buff 自有运行时 helper 或 `UTcsBuffComponent`
- **AND** `UTcsStateComponent` 仍应保持对共享 apply/remove/lifecycle 入口的权威性

#### Scenario: 写回叠层数后再处理叠层反应
- **WHEN** Buff 运行时修改某个 Buff 的叠层数量
- **THEN** 最终写回的叠层数应成为后续增量反应处理的依据
- **AND** 共享的叠层上涨反应应通过 Buff 自有的统一路径执行，而不是写死在各个 merger 中

#### Scenario: 先解析持续时间耗尽策略，再决定移除还是保留 Buff
- **WHEN** 一个有限时长 Buff 的剩余持续时间归零
- **THEN** Buff 运行时应先解析该 Buff 的持续时间耗尽策略
- **AND** 只有在策略要求整 Buff 移除或没有任何叠层存活时，才应移除整个 Buff

### Requirement: Period 驱动应归属 BuffStateTree
TCS SHALL 把 Buff 的 Period 驱动保留在 BuffStateTree 内，而不是把它建模成通用 Buff 增量反应配置或组件侧调度状态。

#### Scenario: 周期型 Buff 可以使用可复用的 StateTree 周期驱动任务
- **WHEN** 一个 BuffStateTree 使用 `FTcsSTTask_BuffPeriodDriver`
- **THEN** 该任务应从本地 override 或 Buff 定义上的默认 `Period` 值解析当前周期
- **AND** 应在时间推进时发出中性的 `Event.Buff.PeriodTick` 事件

#### Scenario: 当 BuffStateTree 不再 Tick 时，Period 也应停止推进
- **WHEN** 一个 BuffStateTree 处于暂停、挂起或其他不再 ticking 的状态
- **THEN** 该 Period 驱动任务应停止为该 Buff 累积已过去时间

### Requirement: 特殊 Period 反应应保留为具体 Buff 的树内逻辑
TCS SHALL 要求与叠层敏感或具体 Buff 绑定的 Period 反应由具体 BuffStateTree 逻辑实现，而不是提升为通用 Buff 定义策略。

#### Scenario: 叠层触发的额外一次 Period 执行应保留在具体 Buff 的树内逻辑中
- **WHEN** 某个具体 Buff 希望在叠层增长时立刻额外触发一次 Period 效果
- **THEN** 该行为应由该 Buff 专属的 StateTree 节点、任务或状态流实现
- **AND** 共享的 Buff 定义配置面不应为此新增一个通用 `PeriodPolicy`

#### Scenario: 叠层触发的周期重置应保留在具体 Buff 的树内逻辑中
- **WHEN** 某个具体 Buff 希望让叠层变化重置其 Period 相位
- **THEN** 该行为应由该 Buff 专属的 StateTree 逻辑实现
- **AND** 共享 Buff 运行时不应为了支持它而要求一个组件侧 `PeriodTracker`