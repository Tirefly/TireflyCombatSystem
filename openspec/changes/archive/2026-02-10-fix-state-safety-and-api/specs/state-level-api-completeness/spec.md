# state-level-api-completeness Specification

## Purpose
完善状态等级 API，支持蓝图侧按等级施加状态，解决当前 API 固定使用 Level=1 的限制。

## ADDED Requirements

### Requirement: TryApplyStateToTarget 支持可选的 StateLevel 参数

系统 MUST 为 `UTcsStateManagerSubsystem::TryApplyStateToTarget()` 函数添加可选的 `StateLevel` 参数（默认值为 1），允许调用者指定状态的等级。

#### Scenario: 使用默认等级（Level=1）施加状态

**Given** 用户调用 `TryApplyStateToTarget(Target, StateDefId, Instigator)` 不指定 `StateLevel` 参数
**When** 函数内部调用 `CreateStateInstance()`
**Then** 传递的 `InLevel` 参数为 1（默认值）

#### Scenario: 使用指定等级施加状态

**Given** 用户调用 `TryApplyStateToTarget(Target, StateDefId, Instigator, 5)` 指定 `StateLevel` 为 5
**When** 函数内部调用 `CreateStateInstance()`
**Then** 传递的 `InLevel` 参数为 5

#### Scenario: 蓝图中使用指定等级施加状态

**Given** 蓝图节点 `TryApplyStateToTarget` 暴露了 `StateLevel` 参数（默认值为 1）
**When** 用户在蓝图中设置 `StateLevel` 为 10
**And** 调用该节点
**Then** 创建的 StateInstance 的等级为 10

---

### Requirement: StateLevel 参数正确传递到 CreateStateInstance

系统 MUST 确保 `TryApplyStateToTarget()` 函数将 `StateLevel` 参数正确传递给 `CreateStateInstance()` 函数。

#### Scenario: StateLevel 参数正确传递

**Given** 用户调用 `TryApplyStateToTarget(Target, StateDefId, Instigator, 7)`
**When** 函数内部调用 `CreateStateInstance(StateDefId, Target, Instigator, InLevel)`
**Then** `InLevel` 参数的值为 7

---

### Requirement: StateLevel 参数文档化

系统 MUST 在 `TryApplyStateToTarget()` 函数的注释中明确说明 `StateLevel` 参数的用途和默认值。

#### Scenario: 函数注释包含 StateLevel 参数说明

**Given** `UTcsStateManagerSubsystem::TryApplyStateToTarget()` 函数定义
**When** 查看函数注释
**Then** 注释包含 `@param StateLevel 状态等级（默认为 1）` 的说明

---

### Requirement: 向后兼容性保证

系统 MUST 确保添加 `StateLevel` 参数后，现有的调用代码无需修改即可正常工作。

#### Scenario: 现有调用代码无需修改

**Given** 现有代码调用 `TryApplyStateToTarget(Target, StateDefId, Instigator)` 不指定 `StateLevel` 参数
**When** 编译和运行代码
**Then** 代码正常工作，使用默认等级 1

---

## MODIFIED Requirements

无

---

## REMOVED Requirements

无
