## MODIFIED Requirements
### Requirement: State Manager Subsystem 只保留全局职责

迁移完成后，`UTcsStateManagerSubsystem` SHALL 只暴露全局 state instance ID 工厂，以及跨 Actor 门面。Definition cache/load、Definition 查询与运行时 Definition source cache 归口 SHALL 不再留在 `UTcsStateManagerSubsystem` 中。

#### Scenario: Definition 查询不再由 StateManager 提供
- **WHEN** 任意调用方需要通过 `FName` 或 `FGameplayTag` 解析具体 State-like Definition 或 `UTcsStateSlotDefinition` 时
- **THEN** 该查询 MUST 通过统一的运行时 Definition 加载归口完成
- **AND** `UTcsStateManagerSubsystem` MUST NOT 再暴露 `GetStateDefinition` / `GetStateDefinitionByTag` / `GetStateSlotDefinition` / `GetStateSlotDefinitionByTag` / `GetAllStateDefNames`

#### Scenario: StateDefId 查询语义只允许留在 State 模块内部
- **WHEN** 系统仍保留 `StateDefId` 相关查询或标识语义
- **THEN** 它 MUST 只服务于 `State` 模块内部单个 state definition 在 `StateComponent` 上的运行生命周期
- **AND** Buff 相关 public API MUST NOT 再对外暴露 `StateDefId`

#### Scenario: 全局 ID 工厂仍留在 StateManager 上
- **WHEN** `UTcsStateComponent` 需要一个新的全局唯一 `StateInstanceId`
- **THEN** 它 MUST 继续调用 `ResolveStateManager()->AllocateStateInstanceId()`；ID 计数器 MUST NOT 被迁移到 Component 作用域

#### Scenario: Buff apply 主路径按 DefId 驱动
- **WHEN** 调用方要施加一个 Buff，且手里只有 `BuffDefId`
- **THEN** 主执行路径 MUST 允许直接按 DefId 解析并继续 apply
- **AND** 不得要求调用方先显式持有已加载的 `UTcsBuffDefinition*`

#### Scenario: Buff Definition 解析失败时不得部分 apply
- **WHEN** Buff apply 主路径按 `BuffDefId` 解析 Definition 失败
- **THEN** 该次 apply MUST 明确失败
- **AND** 系统 MUST NOT 创建部分初始化的 runtime state、占位 buff 或半完成的 apply 副作用

#### Scenario: Buff 对象型逻辑若残留只能作为内部辅助实现
- **WHEN** 迁移阶段内部仍存在对象型 Buff apply 转发逻辑
- **THEN** 这些逻辑 MUST NOT 继续作为 public API 暴露
- **AND** 它们 MUST 只服务于内部参数转换与 DefId 主路径收口
