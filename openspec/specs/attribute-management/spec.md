# attribute-management Specification

## Purpose
定义 `UTcsAttributeComponent` 的 Actor 本地 Attribute 业务边界：属性 CRUD、Modifier 管线、SourceHandle 本地索引、重算与夹值。`AttributeDef` / `AttributeModifierDef` 与 tag 解析归口到 `UTcsDefinitionManagerSubsystem`；Attribute / Modifier 进程级 ID 由 Component 静态工厂分配。
## Requirements
### Requirement: Attribute Component 拥有 Actor 本地 Attribute 业务逻辑

`UTcsAttributeComponent` SHALL 成为 Actor 本地 attribute 业务逻辑的唯一归属，包括 attribute CRUD、BaseValue 写入、由 BaseValue / modifier 聚合推导 CurrentValue、SourceHandle 到 modifier 的本地索引、重算、夹值以及范围约束。该 change 归档后，`UTcsAttributeManagerSubsystem` MUST NOT 再保留任何 Actor 本地业务实现。

#### Scenario: 子类扩展夹值策略

- **WHEN** 开发者从 `UTcsAttributeComponent` 派生 `UMyCustomAttributeComponent` 并覆写 `ClampAttributeValueInRange`
- **THEN** 该覆写 MUST 在所有相关的数值路径上被调用，包括 AddAttribute 的占位值约束、`SetAttributeBaseValue`、modifier 驱动的重算与范围传播路径，并且不允许被 Subsystem 绕过

#### Scenario: Subsystem 不再接受 Actor 本地调用

- **WHEN** 迁移完成后（Phase G 已归档）
- **THEN** `UTcsAttributeManagerSubsystem` MUST NOT 声明或实现 `AddAttribute` / `AddAttributes` / `AddAttributeByTag` / `SetAttributeBaseValue` / `RemoveAttribute` / `CreateAttributeModifier` / `CreateAttributeModifierWithBindings` / `ApplyModifier` / `ApplyModifierWithSourceHandle` / `RemoveModifier` / `RemoveModifiersBySourceHandle` / `GetModifiersBySourceHandle` / `HandleModifierUpdated` / `GetAttributeComponent`

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
- **WHEN** `UTcsAttributeComponent::AddAttribute(Name)` executes
- **THEN** `UTcsAttributeDefinition` MUST 通过统一的运行时 Definition 加载归口获取
- **AND** AddAttribute MUST NOT 接受 InitValue 或通过 component 本地 map / `UTcsAttributeManagerSubsystem::GetAttributeDefinition(Name)` 获取定义

### Requirement: Attribute 夹值绑定到单一 Component 作用域

Attribute 夹值 SHALL 只在单个 `UTcsAttributeComponent` 作用域内工作。`FTcsAttributeRange` 的 `MinValueAttribute` / `MaxValueAttribute`（均为 `FName`）MUST 解析到同一个 component 实例上的 attributes。`UTcsAttributeClampStrategy` 子类收到的 `FTcsAttributeClampContextBase` 也只能绑定到所属 component。

#### Scenario: 同 Component 内解析最小值/最大值

- **WHEN** `EnforceAttributeRangeConstraints` processes an attribute whose `FTcsAttributeRange.MinValueAttribute` is `HealthFloor`
- **THEN** 解析过程 MUST 在 `this` component 上查找 `HealthFloor`；MUST NOT 去查找其他 Actor 的 component

#### Scenario: 不支持跨 Actor 引用

- **WHEN** 设计者试图在 `FTcsAttributeRange` 中通过名称引用另一个 Actor 的 attribute
- **THEN** 该行为属于未定义且不受支持；解析会回落到本地 component，不会形成跨 component 依赖

### Requirement: Attribute Manager Subsystem 只保留全局职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。attribute 与 modifier 的 tag 解析、Definition 查询与相关运行时索引 SHALL 收敛到 `UTcsDefinitionManagerSubsystem`；Attribute / Modifier 的运行时实例 ID 工厂 SHALL 下沉到 `UTcsAttributeComponent` 内部静态工厂；SourceHandle 创建 SHALL 统一委托共享的 `FTcsSourceHandleFactory`。

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

#### Scenario: SourceHandle 创建委托共享静态工厂
- **WHEN** State、Buff、Skill、Attribute 或后续伤害运行时需要创建新的有效 `FTcsSourceHandle`
- **THEN** C++ 权威创建入口 MUST 是 `FTcsSourceHandleFactory`
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `CreateSourceHandle` 入口
- **AND** `UTcsAttributeComponent` 与 `UTcsDefinitionManagerSubsystem` MUST NOT 成为 SourceHandle 分配器

### Requirement: 通过 Virtual 明确 Public Component API 的扩展点

`UTcsAttributeComponent` 的 public API SHALL 将以下方法标记为 `virtual`，以明确扩展点：`AddAttribute`、`SetAttributeBaseValue`、`RemoveAttribute`、`CreateAttributeModifier`、`CreateAttributeModifierWithBindings`、`ApplyModifier`、`RemoveModifier`、`HandleModifierUpdated`、`RemoveModifiersBySourceHandle`。非扩展型包装器（`AddAttributes`、`AddAttributeByTag`、`ApplyModifierWithSourceHandle`、`GetModifiersBySourceHandle`）SHALL 保持 non-virtual。`ResetAttribute` 与 `SetAttributeCurrentValue` MUST NOT 作为 public API 或扩展点存在。

#### Scenario: 扩展点可覆写

- **WHEN** 某个子类为了加入遥测而覆写 `RemoveModifiersBySourceHandle`
- **THEN** 该覆写 MUST 通过基类分派，在 `UTcsStateComponent::FinalizeStateRemoval.Step 5` 中被调用

#### Scenario: 包装器不可覆写

- **WHEN** 某个子类尝试对 `ApplyModifierWithSourceHandle` 做 `virtual override`
- **THEN** 编译器 MUST 因基类方法为 non-virtual 而拒绝该覆写；子类若要扩展，必须通过新增方法叠加实现

### Requirement: Attribute 创建不持有业务数值基线

`FTcsAttributeInstance` SHALL 只保存 `BaseValue` 与 `CurrentValue` 作为数值状态。系统 MUST NOT 保存 `InitValue`、`InitialValue`、`InitialBaseValue` 或其他创建时数值基线。`AddAttribute` 与 `AddAttributeByTag` MUST NOT 接受数值参数；它们只负责解析 Definition、创建 / 注册 AttributeInstance、使用内部零值占位参与统一 Range Clamp，以及建立范围依赖。

#### Scenario: AddAttribute 不接受初始值但仍解析定义
- **WHEN** 调用方执行 `AddAttribute(AttributeDefId)`
- **THEN** `UTcsAttributeDefinition` MUST 通过 `UTcsDefinitionManagerSubsystem` 的统一运行时加载归口解析
- **AND** 调用方 MUST NOT 提供 InitValue 或等价数值参数
- **AND** AttributeInstance MUST NOT 保存任何可恢复初始值字段

#### Scenario: 业务层在创建后明确写入 BaseValue
- **WHEN** 角色创建、读档、等级变化或资源恢复需要设置 Attribute 数值
- **THEN** 业务层 MUST 在 Attribute 创建后通过 `SetAttributeBaseValue` 写入计算得到的 BaseValue
- **AND** 系统 MUST NOT 从 AddAttribute 的历史参数、AttributeInstance 初始字段或 Reset 语义推断该数值

#### Scenario: 创建占位值仍受范围约束
- **WHEN** 新 Attribute 的内部零值不在其有效 AttributeRange 内
- **THEN** 系统 MUST 通过该 Attribute 的统一 ClampStrategy 对 BaseValue 与 CurrentValue 应用范围约束
- **AND** 约束后的值 MUST NOT 被记录为业务初始基线

### Requirement: Attribute Core 不提供 Reset 或直接 CurrentValue 写入

`UTcsAttributeComponent` MUST NOT 声明或实现 `ResetAttribute` 或 `SetAttributeCurrentValue`，也 MUST NOT 提供等价的 Blueprint、UnrealSharp 或兼容包装入口。CurrentValue SHALL 只由 BaseValue、当前 Modifier 聚合与统一 Range Clamp 推导。业务层的初始化、读档、等级变化和资源恢复 MUST 通过 `SetAttributeBaseValue` 完成；伤害、治疗和周期结算 MUST NOT 使用 Attribute Core 的直接 CurrentValue 写入作为过渡路径。

#### Scenario: Reset 由业务层显式编排
- **WHEN** 业务层需要重生、回档、等级回退或满资源恢复
- **THEN** 它 MUST 显式选择数据来源、相关 Modifier 生命周期和 BaseValue 写入顺序
- **AND** 系统 MUST NOT 删除相关 Modifier 或恢复隐藏 InitialValue 作为 Reset 副作用

#### Scenario: CurrentValue 没有直接公开写入口
- **WHEN** 审查 TCS runtime public API、Blueprint 反射面、UnrealSharp 源声明和用户脚本调用点
- **THEN** `ResetAttribute` 与 `SetAttributeCurrentValue` MUST 不存在
- **AND** 需要调整数值的调用方 MUST 使用 `SetAttributeBaseValue` 或等待后续获批的 Instant AttributeModifier 入口

#### Scenario: BaseValue 写入重建最终 CurrentValue
- **WHEN** 调用方通过 `SetAttributeBaseValue` 修改已存在 Attribute 的基础值
- **THEN** 系统 MUST 使用当前 Modifier 与统一 Range Clamp 重新建立最终 CurrentValue
- **AND** 系统 MUST NOT 通过直接覆写 CurrentValue 绕过该重建路径

### Requirement: BaseValue 与 CurrentValue 共用范围约束

对同一个 Attribute，BaseValue 与 CurrentValue SHALL 使用同一个 `FTcsAttributeRange` 和同一个 `UTcsAttributeClampStrategy`。动态 MinValueAttribute / MaxValueAttribute MUST 只解析同一 `UTcsAttributeComponent` 上被依赖 Attribute 的 CurrentValue。系统 MUST NOT 为 Base / Current 引入不同 Range、不同 ClampStrategy、ValueLayer 选择、跨 Component 查询或跨 Actor 依赖。

#### Scenario: 同一策略约束两个数值层
- **WHEN** AddAttribute、SetAttributeBaseValue、Modifier 重算或范围传播使 Attribute 的 BaseValue 或 CurrentValue 超出有效范围
- **THEN** 系统 MUST 对两个数值层使用同一个 AttributeRange 和同一个 ClampStrategy
- **AND** 所有范围传播完成后才提交最终稳定值和对应事件

#### Scenario: 动态范围只读取本地 CurrentValue
- **WHEN** Attribute 的动态 MaxValueAttribute 引用同一组件上的 `MaxHealth`
- **THEN** `MaxHealth` 的 CurrentValue MUST 作为该范围上限
- **AND** 系统 MUST NOT 改读 `MaxHealth` 的 BaseValue、其他 Component 或其他 Actor 的 Attribute

#### Scenario: 容量降低永久截断两个数值层
- **WHEN** 有效最大值从 100 降至 80，而资源 Attribute 的 BaseValue 与 CurrentValue 都为 100
- **THEN** 范围传播完成后两个值 MUST 都为 80
- **AND** 当有效最大值后续恢复至 100 时，两个值 MUST 保持 80
- **AND** 系统 MUST NOT 保存或返还任何隐藏 overflow 数值

