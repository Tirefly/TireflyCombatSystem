## MODIFIED Requirements

### Requirement: Attribute Component 拥有 Actor 本地 Attribute 业务逻辑

`UTcsAttributeComponent` SHALL 成为 Actor 本地 attribute 业务逻辑的唯一归属，包括 attribute CRUD、BaseValue 写入、由 BaseValue / Ongoing AttributeModifier 聚合推导 CurrentValue、SourceHandle 到 Ongoing modifier 的本地索引、重算、夹值以及范围约束。`UTcsAttributeManagerSubsystem` MUST NOT 再保留任何 Actor 本地业务实现。

#### Scenario: 子类扩展夹值策略

- **WHEN** 开发者从 `UTcsAttributeComponent` 派生 `UMyCustomAttributeComponent` 并覆写 `ClampAttributeValueInRange`
- **THEN** 该覆写 MUST 在所有相关的数值路径上被调用，包括 AddAttribute 的占位值约束、`SetAttributeBaseValue`、Instant AttributeModifier 写入、Ongoing modifier 驱动的重算与范围传播路径，并且不允许被 Subsystem 绕过

#### Scenario: Subsystem 不再接受 Actor 本地调用

- **WHEN** 迁移完成后（Phase G 已归档）
- **THEN** `UTcsAttributeManagerSubsystem` MUST NOT 声明或实现 `AddAttribute` / `AddAttributes` / `AddAttributeByTag` / `SetAttributeBaseValue` / `RemoveAttribute` / `ApplyAttributeModifier` / `RemoveOngoingModifiersBySourceHandle` / `GetOngoingModifiersBySourceHandle` / `GetAttributeComponent` 或任何已删除的旧 Create/Apply Modifier API

### Requirement: Modifier 管线保持行为不变量

`UTcsAttributeComponent` 的 AttributeModifier 管线 SHALL 遵守 Instant / Ongoing 分层不变量：所有有效 Application 必须携带有效 SourceHandle；Instant 原子写 BaseValue 且不进入 Ongoing 存储；Ongoing 由父实例索引维护；`SourceHandleIdToModifierInstIds` 与 `ModifierInstIdToIndex` 在移除后保持一致；Attribute Change 事件只在 Clamp 与范围传播完成后报告最终稳定态；路径收尾调用 `EnforceAttributeRangeConstraints()`。

#### Scenario: Instant Application 原子写 BaseValue

- **WHEN** `UTcsAttributeComponent::ApplyAttributeModifier` 以 `ApplicationMode = Instant` 处理请求
- **THEN** 系统 MUST 在全部 Operation 求值成功后原子提交 BaseValue 变更
- **AND** MUST NOT 分配 Ongoing 存储项或依赖 `ModifierChangeBatchId`

#### Scenario: SourceHandle 索引在移除后保持一致

- **WHEN** `RemoveOngoingModifiersBySourceHandle(SourceHandle)` 处理 Ongoing 父实例
- **THEN** 每一个被移除的父实例 MUST 同时从 `SourceHandleIdToModifierInstIds[SourceHandle.Id]` 与 `ModifierInstIdToIndex` 中清除；不得留下任何陈旧索引项

#### Scenario: 范围约束后才报告最终稳定态

- **WHEN** 任意 Instant 或 Ongoing 变更路径完成时
- **THEN** `EnforceAttributeRangeConstraints()` MUST 在最终 Attribute Change 事件之前完成收敛
- **AND** 对外事件 MUST 只包含传播后的最终稳定值

### Requirement: Modifier 创建通过 Manager 获取全局 ID

`UTcsAttributeComponent` 中凡是需要进程级唯一 ID 的方法 SHALL 通过自身静态工厂分配 `AttributeInstId` 与 `ModifierInstId`。系统 MUST NOT 再分配或保存 `ModifierChangeBatchId`。已删除的 `UTcsAttributeManagerSubsystem` MUST NOT 继续承担 ID 分配职责，Component 实例之间也 MUST NOT 各自维护独立计数器。

#### Scenario: Ongoing 父实例从 Component 静态工厂分配 ModifierInstId
- **WHEN** `ApplyAttributeModifier` 成功创建新的 Ongoing 父实例
- **THEN** 其 `ModifierInstId` MUST 来自 `UTcsAttributeComponent` 的静态分配入口
- **AND** 该入口 MUST 在当前进程内保持全局唯一

#### Scenario: 已删除 AttributeManager 与 ChangeBatchId 不再分配
- **WHEN** 在 TCS runtime 源码中搜索 `ResolveAttributeManager`、`UTcsAttributeManagerSubsystem`、`ModifierChangeBatchId` 或 `NextModifierChangeBatchId`
- **THEN** 搜索结果 MUST 不包含 Attribute / Modifier ID 分配实现或 ChangeBatchId 计数器

### Requirement: Attribute Manager Subsystem 只保留全局职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。attribute 与 modifier 的 tag 解析、Definition 查询与相关运行时索引 SHALL 收敛到 `UTcsDefinitionManagerSubsystem`；Attribute / Ongoing Modifier 的运行时实例 ID 工厂 SHALL 下沉到 `UTcsAttributeComponent` 内部静态工厂，并只覆盖 `AttributeInstId` 与 `ModifierInstId`；SourceHandle 创建 SHALL 统一委托共享的 `FTcsSourceHandleFactory`。

#### Scenario: Tag 到 AttributeDefId 的解析集中在 DefinitionManager
- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 attribute 名称时
- **THEN** MUST 使用 `UTcsDefinitionManagerSubsystem` 提供的 Attribute tag 查询或解析接口
- **AND** `UTcsAttributeComponent` MUST NOT 在本地缓存或复制全局 tag 映射

#### Scenario: Attribute 与 Modifier 的全局 ID 工厂下沉到 Component
- **WHEN** `UTcsAttributeComponent` 需要分配新的 `AttributeInstId` 或 `ModifierInstId`
- **THEN** 它 MUST 通过自身持有的静态工厂完成分配
- **AND** 分配出的 ID MUST 在当前进程内保持全局唯一
- **AND** MUST NOT 再分配 `ModifierChangeBatchId`

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

`UTcsAttributeComponent` 的 public API SHALL 将以下方法标记为 `virtual`，以明确扩展点：`AddAttribute`、`SetAttributeBaseValue`、`RemoveAttribute`、`ApplyAttributeModifier`、`RemoveOngoingModifiersBySourceHandle`。非扩展型包装器（`AddAttributes`、`AddAttributeByTag`、`GetOngoingModifiersBySourceHandle`）SHALL 保持 non-virtual。旧 `CreateAttributeModifier*`、`ApplyModifier*`、`HandleModifierUpdated`、`ResetAttribute` 与 `SetAttributeCurrentValue` MUST NOT 作为 public API 或扩展点存在。

#### Scenario: 扩展点可覆写

- **WHEN** 某个子类为了加入遥测而覆写 `RemoveOngoingModifiersBySourceHandle`
- **THEN** 该覆写 MUST 通过基类分派，在 `UTcsStateComponent::FinalizeStateRemoval` 的 Ongoing 清理步骤中被调用

#### Scenario: 包装器不可覆写

- **WHEN** 某个子类尝试对 `GetOngoingModifiersBySourceHandle` 做 `virtual override`
- **THEN** 编译器 MUST 因基类方法为 non-virtual 而拒绝该覆写；子类若要扩展，必须通过新增方法叠加实现

### Requirement: Attribute Core 不提供 Reset 或直接 CurrentValue 写入

`UTcsAttributeComponent` MUST NOT 声明或实现 `ResetAttribute` 或 `SetAttributeCurrentValue`，也 MUST NOT 提供等价的 Blueprint、UnrealSharp 或兼容包装入口。CurrentValue SHALL 只由 BaseValue、当前 Ongoing AttributeModifier 聚合与统一 Range Clamp 推导。业务层的初始化、读档、等级变化和资源恢复 MUST 通过 `SetAttributeBaseValue` 完成；伤害、治疗和周期结算 MUST 使用 Instant AttributeModifier，而不是 Attribute Core 的直接 CurrentValue 写入。

#### Scenario: Reset 由业务层显式编排
- **WHEN** 业务层需要重生、回档、等级回退或满资源恢复
- **THEN** 它 MUST 显式选择数据来源、相关 Modifier 生命周期和 BaseValue 写入顺序
- **AND** 系统 MUST NOT 删除相关 Modifier 或恢复隐藏 InitialValue 作为 Reset 副作用

#### Scenario: CurrentValue 没有直接公开写入口
- **WHEN** 审查 TCS runtime public API、Blueprint 反射面、UnrealSharp 源声明和用户脚本调用点
- **THEN** `ResetAttribute` 与 `SetAttributeCurrentValue` MUST 不存在
- **AND** 需要一次性改写数值的调用方 MUST 使用 `SetAttributeBaseValue` 或 `ApplyAttributeModifier` 的 Instant 模式

#### Scenario: BaseValue 写入重建最终 CurrentValue
- **WHEN** 调用方通过 `SetAttributeBaseValue` 修改已存在 Attribute 的基础值
- **THEN** 系统 MUST 使用当前 Ongoing Modifier 与统一 Range Clamp 重新建立最终 CurrentValue
- **AND** 系统 MUST NOT 通过直接覆写 CurrentValue 绕过该重建路径

## ADDED Requirements

### Requirement: RemoveAttribute 被 Ongoing Operation 引用时硬拒绝

只要任意已保存 Ongoing Operation 将某 Attribute 作为 `TargetAttributeId`，`RemoveAttribute` MUST 硬拒绝且不修改 Attribute、Ongoing 实例记录、索引或事件。调用方 MUST 先结束相关来源并完成 Ongoing 清理，再移除该 Attribute；系统 MUST NOT 部分删除单条 Operation，也 MUST NOT 删除整个父 ModifierInstance 作为隐式补救。

#### Scenario: 仍被 Ongoing 引用的 Attribute 不能删除
- **WHEN** 某个 Ongoing 父实例的 Operation 目标为 `Health`
- **AND** 调用方执行 `RemoveAttribute("Health")`
- **THEN** 调用 MUST 失败
- **AND** `Health` Attribute、相关 Ongoing 实例与索引 MUST 保持不变
