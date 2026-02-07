# Spec: 属性 Modifier 严格输入校验

## 概述

本规范定义创建属性 Modifier 时的严格输入校验规则,确保所有非法输入都被拒绝并输出清晰的错误日志,遵循 Fail Fast 原则。

## ADDED Requirements

### Requirement: CreateAttributeModifier MUST 校验 SourceName

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::CreateAttributeModifier` MUST reject calls with `SourceName == NAME_None`. `UTcsAttributeManagerSubsystem::CreateAttributeModifier` 必须拒绝 `SourceName == NAME_None` 的调用。

#### Scenario: 拒绝空 SourceName

**Given** 调用 `CreateAttributeModifier` 函数

**When** 参数 `SourceName` 为 `NAME_None`

**Then** 函数必须返回 `false`

**And** 必须输出 Error 级别的日志,包含 `"SourceName is NAME_None"` 信息

**And** 不应创建任何 Modifier 实例

---

### Requirement: CreateAttributeModifier MUST 校验 Actor 有效性

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::CreateAttributeModifier` MUST reject invalid `Instigator` or `Target` Actor. `UTcsAttributeManagerSubsystem::CreateAttributeModifier` 必须拒绝无效的 `Instigator` 或 `Target` Actor。

#### Scenario: 拒绝无效 Instigator

**Given** 调用 `CreateAttributeModifier` 函数

**When** 参数 `Instigator` 为 `nullptr` 或无效(IsValid 返回 false)

**Then** 函数必须返回 `false`

**And** 必须输出 Error 级别的日志,包含 `"Instigator is invalid"` 信息

**And** 不应创建任何 Modifier 实例

#### Scenario: 拒绝无效 Target

**Given** 调用 `CreateAttributeModifier` 函数

**When** 参数 `Target` 为 `nullptr` 或无效(IsValid 返回 false)

**Then** 函数必须返回 `false`

**And** 必须输出 Error 级别的日志,包含 `"Target is invalid"` 信息

**And** 不应创建任何 Modifier 实例

---

### Requirement: CreateAttributeModifier MUST 校验战斗实体接口

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::CreateAttributeModifier` MUST ensure both `Instigator` and `Target` implement `ITcsEntityInterface` interface. `UTcsAttributeManagerSubsystem::CreateAttributeModifier` 必须确保 `Instigator` 和 `Target` 都实现了 `ITcsEntityInterface` 接口。

#### Scenario: 拒绝未实现接口的 Instigator

**Given** 调用 `CreateAttributeModifier` 函数

**And** 参数 `Instigator` 是有效的 Actor

**When** `Instigator` 未实现 `ITcsEntityInterface` 接口

**Then** 函数必须返回 `false`

**And** 必须输出 Error 级别的日志,包含 `"Instigator does not implement ITcsEntityInterface"` 信息

**And** 不应创建任何 Modifier 实例

#### Scenario: 拒绝未实现接口的 Target

**Given** 调用 `CreateAttributeModifier` 函数

**And** 参数 `Target` 是有效的 Actor

**When** `Target` 未实现 `ITcsEntityInterface` 接口

**Then** 函数必须返回 `false`

**And** 必须输出 Error 级别的日志,包含 `"Target does not implement ITcsEntityInterface"` 信息

**And** 不应创建任何 Modifier 实例

---

### Requirement: CreateAttributeModifierWithOperands MUST 应用相同校验

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands` MUST apply the same input validation rules as `CreateAttributeModifier`. `UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands` 必须应用与 `CreateAttributeModifier` 相同的输入校验规则。

#### Scenario: 应用所有校验规则

**Given** 调用 `CreateAttributeModifierWithOperands` 函数

**When** 任何输入参数不满足校验规则

**Then** 函数必须返回 `false`

**And** 必须输出相应的 Error 级别日志

**And** 不应创建任何 Modifier 实例

**And** 校验规则必须与 `CreateAttributeModifier` 完全一致:
- SourceName 不能为 NAME_None
- Instigator 必须有效
- Target 必须有效
- Instigator 必须实现 ITcsEntityInterface
- Target 必须实现 ITcsEntityInterface

---

### Requirement: 校验失败时 MUST 遵循统一错误处理策略

**优先级**: P1 (重要)

**描述**:
All validation failures MUST follow unified error handling strategy. 所有校验失败都必须遵循统一的错误处理策略。

#### Scenario: 统一的错误处理

**Given** 任何 CreateAttributeModifier 相关函数

**When** 输入校验失败

**Then** 必须满足以下条件:
- 返回值为 `false`
- 输出 Error 级别日志(使用 `LogTcsAttribute` 日志通道)
- 日志包含函数名称和具体的失败原因
- 不创建任何 Modifier 实例
- 不修改任何组件状态
- 不触发任何事件

---

## 相关规范

- `source-handle-core` - SourceHandle 核心定义
- `source-handle-attribute-integration` - SourceHandle 与属性系统集成

## 实施注意事项

1. **Fail Fast**: 在函数开头进行所有校验,尽早返回
2. **清晰日志**: 错误日志必须包含足够的上下文信息,便于调试
3. **一致性**: 所有创建 Modifier 的函数必须应用相同的校验规则
4. **测试覆盖**: 必须为每种校验失败场景编写单元测试
