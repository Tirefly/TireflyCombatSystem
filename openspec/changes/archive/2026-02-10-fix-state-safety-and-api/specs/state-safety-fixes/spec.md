# state-safety-fixes Specification

## Purpose
修复 State 模块中的安全性问题，包括空指针解引用风险和条件验证逻辑不严格的问题。

## ADDED Requirements

### Requirement: SetDurationRemaining 空指针安全检查

系统 MUST 在 `UTcsStateInstance::SetDurationRemaining()` 中检查 `OwnerStateCmp` 的有效性，当其无效时直接返回，避免后续解引用空指针导致崩溃。

#### Scenario: OwnerStateCmp 无效时安全返回

**Given** StateInstance 的 `OwnerStateCmp` 为无效（nullptr 或已销毁）
**When** 调用 `SetDurationRemaining(float InDurationRemaining)`
**Then** 函数记录错误日志并直接返回，不尝试解引用 `OwnerStateCmp`

#### Scenario: OwnerStateCmp 有效时正常执行

**Given** StateInstance 的 `OwnerStateCmp` 有效
**When** 调用 `SetDurationRemaining(float InDurationRemaining)`
**Then** 函数正常调用 `OwnerStateCmp->SetStateRemainingDuration(this, InDurationRemaining)`

---

### Requirement: CheckStateApplyConditions 严格验证条件有效性

系统 MUST 在 `UTcsStateManagerSubsystem::CheckStateApplyConditions()` 中严格验证激活条件的有效性，当条件配置无效时直接返回 `false`，阻止状态应用。

#### Scenario: 条件配置无效时阻止状态应用

**Given** StateDefinition 包含一个或多个激活条件
**And** 其中至少一个条件的 `IsValid()` 返回 `false`（例如 `ConditionClass` 为空或 `Payload` 无效）
**When** 调用 `CheckStateApplyConditions(StateInstance)`
**Then** 函数记录错误日志并返回 `false`，阻止状态应用

#### Scenario: 所有条件配置有效时正常检查

**Given** StateDefinition 包含一个或多个激活条件
**And** 所有条件的 `IsValid()` 返回 `true`
**When** 调用 `CheckStateApplyConditions(StateInstance)`
**Then** 函数正常检查每个条件，根据条件检查结果返回 `true` 或 `false`

#### Scenario: 无激活条件时返回 true

**Given** StateDefinition 不包含任何激活条件（`ActiveConditions` 为空）
**When** 调用 `CheckStateApplyConditions(StateInstance)`
**Then** 函数返回 `true`

---

## MODIFIED Requirements

无

---

## REMOVED Requirements

无
