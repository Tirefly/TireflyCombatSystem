# 规范增量 —— attribute-management

> **权威执行依据**：本 spec delta 用于定义契约与不变量。具体迁移步骤、文件行号与签名替换规则，以 `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/02_PhaseC_Attribute业务迁移.md` 为准。

## ADDED Requirements

### Requirement: Attribute Component 拥有 Actor 本地 Attribute 业务逻辑

`UTcsAttributeComponent` SHALL 成为 Actor 本地 attribute 业务逻辑的唯一归属，包括 attribute CRUD、modifier 的创建/应用/移除/更新管线、SourceHandle 到 modifier 的本地索引、重算、夹值以及范围约束。该 change 归档后，`UTcsAttributeManagerSubsystem` MUST NOT 再保留任何 Actor 本地业务实现。

#### Scenario: 子类扩展夹值策略

- **WHEN** 开发者从 `UTcsAttributeComponent` 派生 `UMyCustomAttributeComponent` 并覆写 `ClampAttributeValueInRange`
- **THEN** 该覆写 MUST 在所有相关的数值写入路径上被调用，包括 `SetAttributeBaseValue` / `SetAttributeCurrentValue` / modifier 驱动的重算路径，并且不允许被 Subsystem 绕过

#### Scenario: Subsystem 不再接受 Actor 本地调用

- **WHEN** 迁移完成后（Phase G 已归档）
- **THEN** `UTcsAttributeManagerSubsystem` MUST NOT 声明或实现 `AddAttribute` / `AddAttributes` / `AddAttributeByTag` / `SetAttributeBaseValue` / `SetAttributeCurrentValue` / `ResetAttribute` / `RemoveAttribute` / `CreateAttributeModifier` / `CreateAttributeModifierWithOperands` / `ApplyModifier` / `ApplyModifierWithSourceHandle` / `RemoveModifier` / `RemoveModifiersBySourceHandle` / `GetModifiersBySourceHandle` / `HandleModifierUpdated` / `GetAttributeComponent`



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

`UTcsAttributeComponent` 中凡是需要全局唯一 ID 的方法，都 SHALL 通过 `ResolveAttributeManager()->AllocateAttributeInstanceId()` / `AllocateModifierInstanceId()` / `AllocateModifierChangeBatchId()` 获取。Component 实例 MUST NOT 在本地自行递增 ID 计数器。

#### Scenario: CreateAttributeModifier 从全局分配 ID

- **WHEN** `UTcsAttributeComponent::CreateAttributeModifier` produces a new `FTcsAttributeModifierInstance`
- **THEN** 它的 `InstanceId` MUST 来自 `ResolveAttributeManager()->AllocateModifierInstanceId()`，并由该接口递增 Subsystem 级别的 `GlobalAttributeModifierInstanceIdMgr`



### Requirement: Attribute 定义通过 Manager 解析

attribute 与 modifier 的定义查询 SHALL 统一经过 `ResolveAttributeManager()->GetAttributeDefinition(...)` / `GetModifierDefinition(...)`。Components MUST NOT 在本地缓存或复制 definition registry。

#### Scenario: AddAttribute 从 Manager 解析定义

- **WHEN** `UTcsAttributeComponent::AddAttribute(Name, InitValue)` executes
- **THEN** `UTcsAttributeDefinition` MUST 通过 `ResolveAttributeManager()->GetAttributeDefinition(Name)` 获取，而不是通过 component 本地 map 获取



### Requirement: Attribute 夹值绑定到单一 Component 作用域

Attribute 夹值 SHALL 只在单个 `UTcsAttributeComponent` 作用域内工作。`FTcsAttributeRange` 的 `MinValueAttribute` / `MaxValueAttribute`（均为 `FName`）MUST 解析到同一个 component 实例上的 attributes。`UTcsAttributeClampStrategy` 子类收到的 `FTcsAttributeClampContextBase` 也只能绑定到所属 component。

#### Scenario: 同 Component 内解析最小值/最大值

- **WHEN** `EnforceAttributeRangeConstraints` processes an attribute whose `FTcsAttributeRange.MinValueAttribute` is `HealthFloor`
- **THEN** 解析过程 MUST 在 `this` component 上查找 `HealthFloor`；MUST NOT 去查找其他 Actor 的 component

#### Scenario: 不支持跨 Actor 引用

- **WHEN** 设计者试图在 `FTcsAttributeRange` 中通过名称引用另一个 Actor 的 attribute
- **THEN** 该行为属于未定义且不受支持；解析会回落到本地 component，不会形成跨 component 依赖



### Requirement: Attribute Manager Subsystem 只保留全局职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 只暴露 definition cache/load、definition 与 tag 查询、全局 ID 工厂，以及全局 `CreateSourceHandle` 工厂。

#### Scenario: Tag 到 Name 的解析仍然集中化

- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 attribute 名称时
- **THEN** MUST 使用 `UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag` / `TryGetAttributeTagByName`；不提供 per-component 的 tag 解析接口

#### Scenario: SourceHandle 工厂保持全局入口

- **WHEN** 调用方构造一个新的 `FTcsSourceHandle`
- **THEN** `UTcsAttributeManagerSubsystem::CreateSourceHandle(CausalityChain, Instigator, SourceTags)` MUST 作为唯一入口；`UTcsAttributeComponent` MUST NOT 暴露等价接口



### Requirement: 通过 Virtual 明确 Public Component API 的扩展点

`UTcsAttributeComponent` 的 public API SHALL 将以下方法标记为 `virtual`，以明确扩展点：`AddAttribute`、`SetAttributeBaseValue`、`SetAttributeCurrentValue`、`ResetAttribute`、`RemoveAttribute`、`CreateAttributeModifier`、`CreateAttributeModifierWithOperands`、`ApplyModifier`、`RemoveModifier`、`HandleModifierUpdated`、`RemoveModifiersBySourceHandle`。非扩展型包装器（`AddAttributes`、`AddAttributeByTag`、`ApplyModifierWithSourceHandle`、`GetModifiersBySourceHandle`）SHALL 保持非 virtual。

#### Scenario: 扩展点可覆写

- **WHEN** 某个子类为了加入遥测而覆写 `RemoveModifiersBySourceHandle`
- **THEN** 该覆写 MUST 通过基类分派，在 `UTcsStateComponent::FinalizeStateRemoval.Step 5` 中被调用

#### Scenario: 包装器不可覆写

- **WHEN** 某个子类尝试对 `ApplyModifierWithSourceHandle` 做 `virtual override`
- **THEN** 编译器 MUST 因基类方法为 non-virtual 而拒绝该覆写；子类若要扩展，必须通过新增方法叠加实现



### Requirement: Deprecated 兼容层必须严格作为临时过渡

迁移期间，`UTcsAttributeManagerSubsystem` MAY 为旧的 Actor 本地 API 暴露 deprecated 的薄转发包装器，并统一收纳在 `#pragma region Deprecated_MigrationOnly` 下，使用 `UFUNCTION(... meta=(DeprecatedFunction, DeprecationMessage=...))` 或 `UE_DEPRECATED` 标记。这些包装器在 change 归档前 MUST 被硬删除，同时删除所有废弃 metadata 以及只为这些包装器服务的 helper。

#### Scenario: 迁移期间包装器只是纯转发

- **WHEN** 在过渡期内调用被弃用的包装器 `UTcsAttributeManagerSubsystem::AddAttribute(CombatEntity, Name, InitValue)` 时
- **THEN** 它 MUST 只做以下事情：校验 `CombatEntity`、解析 `UTcsAttributeComponent`、调用 `Component->AddAttribute(Name, InitValue)` 并返回结果；包装器内 MUST NOT 残留任何旧业务逻辑

#### Scenario: 归档后 grep 结果为空

- **WHEN** change 归档后执行 `git grep "Deprecated_MigrationOnly" Plugins/TireflyCombatSystem/Source`
- **THEN** 该命令 MUST 不产生任何输出
