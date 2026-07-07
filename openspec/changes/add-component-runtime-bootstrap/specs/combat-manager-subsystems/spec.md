## ADDED Requirements

### Requirement: ManagerSubsystem 必须暴露显式 runtime-ready 契约
TCS SHALL 要求 `UTcsAttributeManagerSubsystem` 与 `UTcsStateManagerSubsystem` 暴露显式的 runtime-ready 契约，使组件能够区分“拿到了 subsystem 指针”和“该 subsystem 已可被业务依赖”这两个状态。

#### Scenario: 组件不能仅凭拿到指针就认定 subsystem ready
- **WHEN** 一个组件成功拿到了 `UTcsAttributeManagerSubsystem*` 或 `UTcsStateManagerSubsystem*`
- **THEN** 它 MUST NOT 仅凭该指针非空就把自身标记为 runtime-ready
- **AND** 必须通过 subsystem 的显式 ready 契约判断前置条件是否真正满足

#### Scenario: subsystem ready 先于组件 runtime-ready 判定
- **WHEN** bootstrap subsystem 评估某个组件能否进入 ready
- **THEN** 它 MUST 先检查所需 subsystem 的 runtime-ready 状态
- **AND** 只有在这些全局前置条件满足后，才允许继续评估组件级依赖

### Requirement: Runtime bootstrap 必须是 GameInstanceSubsystem 并显式声明依赖
TCS SHALL 将新的运行时初始化编排器建模为 `UGameInstanceSubsystem`，并使用 `InitializeDependency` 对现有 manager subsystem 建立正式初始化依赖。

#### Scenario: bootstrap subsystem 依赖 Attribute 与 State manager subsystem
- **WHEN** `UTcsRuntimeBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)` executes
- **THEN** 它 MUST 通过 `Collection.InitializeDependency<UTcsAttributeManagerSubsystem>()` 与 `Collection.InitializeDependency<UTcsStateManagerSubsystem>()` 显式声明依赖

#### Scenario: bootstrap subsystem 不承接 Actor 本地业务逻辑
- **WHEN** 审查 bootstrap subsystem 的职责边界时
- **THEN** 它 MAY 持有实体注册表、依赖评估状态与诊断输出
- **AND** MUST NOT 承接 Attribute / State / Buff / Skill 的 Actor 本地业务实现
