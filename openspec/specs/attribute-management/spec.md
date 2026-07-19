# attribute-management Specification

## Purpose
定义 `UTcsAttributeComponent` 的 Actor 本地 Attribute 业务边界：属性 CRUD、Modifier 管线、SourceHandle 本地索引、重算与夹值。`AttributeDef` / `AttributeModifierDef` 与 tag 解析归口到 `UTcsDefinitionManagerSubsystem`；Attribute / Modifier 进程级 ID 由 Component 静态工厂分配。
## Requirements
### Requirement: Attribute Component 拥有 Actor 本地 Attribute 业务逻辑

`UTcsAttributeComponent` SHALL 成为 Actor 本地 attribute 业务逻辑的唯一归属，包括 attribute CRUD、modifier 的创建/应用/移除/更新管线、SourceHandle 到 modifier 的本地索引、重算、夹值以及范围约束。该 change 归档后，`UTcsAttributeManagerSubsystem` MUST NOT 再保留任何 Actor 本地业务实现。

#### Scenario: 子类扩展夹值策略

- **WHEN** 开发者从 `UTcsAttributeComponent` 派生 `UMyCustomAttributeComponent` 并覆写 `ClampAttributeValueInRange`
- **THEN** 该覆写 MUST 在所有相关的数值写入路径上被调用，包括 `SetAttributeBaseValue` / `SetAttributeCurrentValue` / modifier 驱动的重算路径，并且不允许被 Subsystem 绕过

#### Scenario: Subsystem 不再接受 Actor 本地调用

- **WHEN** 迁移完成后（Phase G 已归档）
- **THEN** `UTcsAttributeManagerSubsystem` MUST NOT 声明或实现 `AddAttribute` / `AddAttributes` / `AddAttributeByTag` / `SetAttributeBaseValue` / `SetAttributeCurrentValue` / `ResetAttribute` / `RemoveAttribute` / `CreateAttributeModifier` / `CreateAttributeModifierWithBindings` / `ApplyModifier` / `ApplyModifierWithSourceHandle` / `RemoveModifier` / `RemoveModifiersBySourceHandle` / `GetModifiersBySourceHandle` / `HandleModifierUpdated` / `GetAttributeComponent`

### Requirement: Modifier 管线保持行为不变量

迁移到 `UTcsAttributeComponent` 的 modifier 创建/应用/移除/更新管线 SHALL 保持所有现有行为不变量：`BatchId` / `ApplyTimestamp` / `UpdateTimestamp` 的写入顺序、`SourceHandleIdToModifierInstIds` 与 `ModifierInstIdToIndex` 的维护、事件广播顺序，以及最终对 `EnforceAttributeRangeConstraints()` 的收尾调用。

#### Scenario: ApplyModifier 按正确顺序写入时间戳

- **WHEN** `UTcsAttributeComponent::ApplyModifier` processes a batch of modifiers
- **THEN** 每个 modifier 的 `BatchId` MUST 先于 `ApplyTimestamp` 赋值，`ApplyTimestamp` MUST 早于任何 merge 或重算，而 `UpdateTimestamp` MUST 只出现在后续更新路径中

#### Scenario: SourceHandle 索引在移除后保持一致

- **WHEN** `RemoveModifiersBySourceHandle(SourceHandle)` processes modifiers
- **THEN** 每一个被移除的 modifier MUST 同时从 `SourceHandleIdToModifierInstIds[SourceHandle.Id]` 与 `ModifierInstIdToIndex` 中清除；不得留下任何陈旧索引项

#### Scenario: 范围约束是最后一步

- **WHEN** 任意 modifier 变更路径（apply / remove / update）完成时
- **THEN** `EnforceAttributeRangeConstraints()` MUST 作为终止调用执行，并且发生在所有事件广播之后

### Requirement: Modifier 创建通过 Manager 获取全局 ID

`UTcsAttributeComponent` 中凡是需要进程级唯一 ID 的方法 SHALL 通过自身静态工厂分配 `AttributeInstId`、`ModifierInstId` 与 `ModifierChangeBatchId`。已删除的 `UTcsAttributeManagerSubsystem` MUST NOT 继续承担 ID 分配职责，Component 实例之间也 MUST NOT 各自维护独立计数器。

#### Scenario: CreateAttributeModifier 从 Component 静态工厂分配 ID
- **WHEN** `UTcsAttributeComponent::CreateAttributeModifier` 创建新的 `FTcsAttributeModifierInstance`
- **THEN** 其实例 ID MUST 来自 `UTcsAttributeComponent` 的静态分配入口
- **AND** 该入口 MUST 在当前进程内保持全局唯一

#### Scenario: 已删除 AttributeManager 不再分配 ID
- **WHEN** 在 TCS runtime 源码中搜索 `ResolveAttributeManager`、`UTcsAttributeManagerSubsystem` 或旧 Manager ID 计数器
- **THEN** 搜索结果 MUST 不包含 Attribute / Modifier ID 分配实现

### Requirement: Attribute 定义通过 Manager 解析

attribute 与 modifier 的定义查询 SHALL 统一经过 `UTcsDefinitionManagerSubsystem` 的类型化 Definition 查询入口。Components MUST NOT 在本地缓存或复制 definition registry，也 MUST NOT 继续通过 `UTcsAttributeManagerSubsystem` 解析 `AttributeDef` / `AttributeModifierDef`。

#### Scenario: AddAttribute 从 DefinitionManager 解析定义
- **WHEN** `UTcsAttributeComponent::AddAttribute(Name, InitValue)` executes
- **THEN** `UTcsAttributeDefinition` MUST 通过统一的运行时 Definition 加载归口获取
- **AND** 它 MUST NOT 通过 component 本地 map 或 `UTcsAttributeManagerSubsystem::GetAttributeDefinition(Name)` 获取

### Requirement: Attribute 夹值绑定到单一 Component 作用域

Attribute 夹值 SHALL 只在单个 `UTcsAttributeComponent` 作用域内工作。`FTcsAttributeRange` 的 `MinValueAttribute` / `MaxValueAttribute`（均为 `FName`）MUST 解析到同一个 component 实例上的 attributes。`UTcsAttributeClampStrategy` 子类收到的 `FTcsAttributeClampContextBase` 也只能绑定到所属 component。

#### Scenario: 同 Component 内解析最小值/最大值

- **WHEN** `EnforceAttributeRangeConstraints` processes an attribute whose `FTcsAttributeRange.MinValueAttribute` is `HealthFloor`
- **THEN** 解析过程 MUST 在 `this` component 上查找 `HealthFloor`；MUST NOT 去查找其他 Actor 的 component

#### Scenario: 不支持跨 Actor 引用

- **WHEN** 设计者试图在 `FTcsAttributeRange` 中通过名称引用另一个 Actor 的 attribute
- **THEN** 该行为属于未定义且不受支持；解析会回落到本地 component，不会形成跨 component 依赖

### Requirement: Attribute Manager Subsystem 只保留全局职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。attribute 与 modifier 的 tag 解析、Definition 查询与相关运行时索引 SHALL 收敛到 `UTcsDefinitionManagerSubsystem`；Attribute / Modifier 的运行时实例 ID 工厂 SHALL 下沉到 `UTcsAttributeComponent` 内部静态工厂。

#### Scenario: Tag 到 AttributeDefId 的解析集中在 DefinitionManager
- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 attribute 名称时
- **THEN** MUST 使用 `UTcsDefinitionManagerSubsystem` 提供的 Attribute tag 查询或解析接口
- **AND** `UTcsAttributeComponent` MUST NOT 在本地缓存或复制全局 tag 映射

#### Scenario: Attribute 与 Modifier 的全局 ID 工厂下沉到 Component
- **WHEN** `UTcsAttributeComponent` 需要分配新的 `AttributeInstId`、`ModifierInstId` 或 `ModifierChangeBatchId`
- **THEN** 它 MUST 通过自身持有的静态工厂完成分配
- **AND** 分配出的 ID MUST 在当前进程内保持全局唯一

#### Scenario: AttributeManager 不再提供 Definition 查询
- **WHEN** 调用方需要解析 `AttributeDef` 或 `AttributeModifierDef`
- **THEN** MUST 使用统一的运行时 Definition 加载归口
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 继续暴露 `GetAttributeDefinition` / `GetModifierDefinition`

#### Scenario: SourceHandle 工厂不再挂在 AttributeManager 上
- **WHEN** State 运行时创建新的 `FTcsSourceHandle`
- **THEN** 该工厂 MUST 位于更贴近 State 生命周期的实现侧
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `CreateSourceHandle` 入口

### Requirement: 通过 Virtual 明确 Public Component API 的扩展点

`UTcsAttributeComponent` 的 public API SHALL 将以下方法标记为 `virtual`，以明确扩展点：`AddAttribute`、`SetAttributeBaseValue`、`SetAttributeCurrentValue`、`ResetAttribute`、`RemoveAttribute`、`CreateAttributeModifier`、`CreateAttributeModifierWithBindings`、`ApplyModifier`、`RemoveModifier`、`HandleModifierUpdated`、`RemoveModifiersBySourceHandle`。非扩展型包装器（`AddAttributes`、`AddAttributeByTag`、`ApplyModifierWithSourceHandle`、`GetModifiersBySourceHandle`）SHALL 保持非 virtual。

#### Scenario: 扩展点可覆写

- **WHEN** 某个子类为了加入遥测而覆写 `RemoveModifiersBySourceHandle`
- **THEN** 该覆写 MUST 通过基类分派，在 `UTcsStateComponent::FinalizeStateRemoval.Step 5` 中被调用

#### Scenario: 包装器不可覆写

- **WHEN** 某个子类尝试对 `ApplyModifierWithSourceHandle` 做 `virtual override`
- **THEN** 编译器 MUST 因基类方法为 non-virtual 而拒绝该覆写；子类若要扩展，必须通过新增方法叠加实现

