# attribute-boundary-events Specification

## Purpose
修复 Attribute Range Enforcement 缺少边界事件的问题，确保在动态 Range 变化导致值被 Clamp 到边界时也触发 `OnAttributeReachedBoundary` 事件。

## ADDED Requirements

### Requirement: Range Enforcement 触发边界事件

系统 MUST 在 `EnforceAttributeRangeConstraints()` 中检测值是否被 Clamp 到边界，如果是则触发 `OnAttributeReachedBoundary` 事件。

#### Scenario: BaseValue 被 Clamp 到最大边界时触发事件

**Given** Attribute 的 BaseValue 为 100
**And** Attribute 的 MaxValue 从 100 降低到 80
**When** `EnforceAttributeRangeConstraints()` 执行
**Then** BaseValue 被 Clamp 到 80
**And** 触发 `OnAttributeReachedBoundary` 事件，参数为：
  - `AttributeName`: 属性名称
  - `bIsMaxBoundary`: `true`
  - `OldValue`: 100
  - `NewValue`: 80
  - `BoundaryValue`: 80

#### Scenario: BaseValue 被 Clamp 到最小边界时触发事件

**Given** Attribute 的 BaseValue 为 10
**And** Attribute 的 MinValue 从 0 提高到 20
**When** `EnforceAttributeRangeConstraints()` 执行
**Then** BaseValue 被 Clamp 到 20
**And** 触发 `OnAttributeReachedBoundary` 事件，参数为：
  - `AttributeName`: 属性名称
  - `bIsMaxBoundary`: `false`
  - `OldValue`: 10
  - `NewValue`: 20
  - `BoundaryValue`: 20

#### Scenario: CurrentValue 被 Clamp 到最大边界时触发事件

**Given** Attribute 的 CurrentValue 为 100
**And** Attribute 的 MaxValue 从 100 降低到 80
**When** `EnforceAttributeRangeConstraints()` 执行
**Then** CurrentValue 被 Clamp 到 80
**And** 触发 `OnAttributeReachedBoundary` 事件，参数为：
  - `AttributeName`: 属性名称
  - `bIsMaxBoundary`: `true`
  - `OldValue`: 100
  - `NewValue`: 80
  - `BoundaryValue`: 80

#### Scenario: CurrentValue 被 Clamp 到最小边界时触发事件

**Given** Attribute 的 CurrentValue 为 10
**And** Attribute 的 MinValue 从 0 提高到 20
**When** `EnforceAttributeRangeConstraints()` 执行
**Then** CurrentValue 被 Clamp 到 20
**And** 触发 `OnAttributeReachedBoundary` 事件，参数为：
  - `AttributeName`: 属性名称
  - `bIsMaxBoundary`: `false`
  - `OldValue`: 10
  - `NewValue`: 20
  - `BoundaryValue`: 20

#### Scenario: 值未达到边界时不触发边界事件

**Given** Attribute 的 CurrentValue 为 50
**And** Attribute 的 MaxValue 从 100 降低到 80
**When** `EnforceAttributeRangeConstraints()` 执行
**Then** CurrentValue 保持为 50（未被 Clamp）
**And** 不触发 `OnAttributeReachedBoundary` 事件

---

### Requirement: 边界事件与 Value Changed 事件的触发顺序

系统 MUST 确保边界事件在 Value Changed 事件之后触发，保持与现有逻辑的一致性。

#### Scenario: 边界事件在 Value Changed 事件之后触发

**Given** Attribute 的 CurrentValue 为 100
**And** Attribute 的 MaxValue 从 100 降低到 80
**When** `EnforceAttributeRangeConstraints()` 执行
**Then** 先触发 `OnAttributeValueChanged` 事件（OldValue: 100, NewValue: 80）
**And** 后触发 `OnAttributeReachedBoundary` 事件（bIsMaxBoundary: true, BoundaryValue: 80）

---

### Requirement: 边界检测使用浮点数容差比较

系统 MUST 使用 `FMath::IsNearlyEqual()` 进行边界检测，以处理浮点数精度问题。

#### Scenario: 使用浮点数容差比较检测边界

**Given** Attribute 的 CurrentValue 为 79.9999999
**And** Attribute 的 MaxValue 为 80.0
**When** `EnforceAttributeRangeConstraints()` 执行边界检测
**Then** 使用 `FMath::IsNearlyEqual(CurrentValue, MaxValue)` 判断是否达到边界
**And** 如果在容差范围内相等，则认为达到边界并触发事件

---

### Requirement: 边界事件与现有逻辑一致

系统 MUST 确保 `EnforceAttributeRangeConstraints()` 中的边界事件触发逻辑与 `RecalculateAttributeBaseValues()` 和 `RecalculateAttributeCurrentValues()` 中的逻辑一致。

#### Scenario: 边界检测逻辑与 RecalculateAttributeBaseValues 一致

**Given** `RecalculateAttributeBaseValues()` 在 BaseValue 达到边界时触发边界事件
**When** `EnforceAttributeRangeConstraints()` 中 BaseValue 达到边界
**Then** 使用相同的边界检测逻辑（`FMath::IsNearlyEqual()`）
**And** 触发相同的边界事件（`BroadcastAttributeReachedBoundaryEvent`）
**And** 事件参数格式一致

#### Scenario: 边界检测逻辑与 RecalculateAttributeCurrentValues 一致

**Given** `RecalculateAttributeCurrentValues()` 在 CurrentValue 达到边界时触发边界事件
**When** `EnforceAttributeRangeConstraints()` 中 CurrentValue 达到边界
**Then** 使用相同的边界检测逻辑（`FMath::IsNearlyEqual()`）
**And** 触发相同的边界事件（`BroadcastAttributeReachedBoundaryEvent`）
**And** 事件参数格式一致

---

## Implementation Notes

- 修改 `TcsAttributeManagerSubsystem.cpp` 中的 `EnforceAttributeRangeConstraints()` 函数
- 在提交 BaseValue 和 CurrentValue 时，检测新值是否达到边界
- 需要获取属性的 Range 信息（Min/Max）来进行边界检测
- 使用 `FMath::IsNearlyEqual()` 进行浮点数比较
- 边界事件应该在 Value Changed 事件之后触发
- 参考 `RecalculateAttributeBaseValues()` (line 877) 和 `RecalculateAttributeCurrentValues()` (line 982) 中的实现

## Related Capabilities

无
