# attribute-direct-manipulation-api Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
### Requirement: 直接设置Base值API

 MUST 提供直接设置属性Base值的API。

#### Scenario: 设置Base值

**Given**:
- Actor有属性 `Health`
- `Health.BaseValue = 100`

**When**: 调用 `SetAttributeBaseValue(Actor, "Health", 80)`

**Then**:
- `Health.BaseValue` 被设置为80
- 执行clamp逻辑（如果有范围约束）
- 触发 `OnAttributeBaseValueChanged` 事件
- Current值重新计算（应用修改器）

#### Scenario: 设置Base值并禁用事件

**Given**: Actor有属性 `Health`

**When**: 调用 `SetAttributeBaseValue(Actor, "Health", 80, false)`

**Then**:
- `Health.BaseValue` 被设置为80
- 执行clamp逻辑
- 不触发事件（用于批量操作）

#### Scenario: 设置不存在的属性

**Given**: Actor没有属性 `Mana`

**When**: 调用 `SetAttributeBaseValue(Actor, "Mana", 50)`

**Then**:
- 返回false
- 记录错误日志
- 不修改任何属性

### Requirement: 直接设置Current值API

 MUST 提供直接设置属性Current值的API。

#### Scenario: 设置Current值

**Given**:
- Actor有属性 `Health`
- `Health.CurrentValue = 100`

**When**: 调用 `SetAttributeCurrentValue(Actor, "Health", 80)`

**Then**:
- `Health.CurrentValue` 被设置为80
- 执行clamp逻辑
- 触发 `OnAttributeValueChanged` 事件
- Base值不受影响

#### Scenario: 设置Current值超出范围

**Given**:
- Actor有属性 `Health`
- `Health` 范围为 `[0, 100]`

**When**: 调用 `SetAttributeCurrentValue(Actor, "Health", 150)`

**Then**:
- `Health.CurrentValue` 被clamp到100
- 触发 `OnAttributeValueChanged` 事件
- 可能触发 `OnAttributeReachedBoundary` 事件

### Requirement: 重置属性API

 MUST 提供重置属性到初始值的API。

#### Scenario: 重置属性

**Given**:
- Actor有属性 `Health`
- `Health` 定义的初始值为100
- `Health.BaseValue = 80`
- `Health.CurrentValue = 60`

**When**: 调用 `ResetAttribute(Actor, "Health")`

**Then**:
- `Health.BaseValue` 被重置为100
- `Health.CurrentValue` 被重置为100
- 移除所有应用到该属性的修改器
- 触发相应的变更事件

#### Scenario: 重置不存在的属性

**Given**: Actor没有属性 `Mana`

**When**: 调用 `ResetAttribute(Actor, "Mana")`

**Then**:
- 返回false
- 记录错误日志

### Requirement: 移除属性API

 MUST 提供移除属性的API。

#### Scenario: 移除属性

**Given**:
- Actor有属性 `Health`
- `Health` 有关联的修改器

**When**: 调用 `RemoveAttribute(Actor, "Health")`

**Then**:
- 从AttributeComponent移除 `Health` 属性
- 移除所有应用到该属性的修改器
- 触发属性移除事件（如果有）
- 清理相关索引和缓存

#### Scenario: 移除不存在的属性

**Given**: Actor没有属性 `Mana`

**When**: 调用 `RemoveAttribute(Actor, "Mana")`

**Then**:
- 返回false
- 记录警告日志
- 不执行任何操作

### Requirement: API参数验证

所有直接操作API MUST 进行严格的参数验证。

#### Scenario: 验证Actor有效性

**Given**: Actor为nullptr

**When**: 调用任何直接操作API

**Then**:
- 返回false
- 记录错误日志
- 不执行任何操作

#### Scenario: 验证AttributeComponent存在

**Given**: Actor没有AttributeComponent

**When**: 调用任何直接操作API

**Then**:
- 返回false
- 记录错误日志
- 不执行任何操作

#### Scenario: 验证属性名称有效

**Given**: 属性名称为NAME_None

**When**: 调用任何直接操作API

**Then**:
- 返回false
- 记录错误日志
- 不执行任何操作

### Requirement: 事件触发一致性

直接操作API MUST 保证事件触发的一致性。

#### Scenario: Base值变更事件

**Given**: 调用 `SetAttributeBaseValue` 且值发生变化

**When**: 操作完成

**Then**:
-  MUST 触发 `OnAttributeBaseValueChanged` 事件
- 事件payload包含旧值和新值
- 事件payload包含属性名称

#### Scenario: Current值变更事件

**Given**: 调用 `SetAttributeCurrentValue` 且值发生变化

**When**: 操作完成

**Then**:
-  MUST 触发 `OnAttributeValueChanged` 事件
- 事件payload包含旧值和新值
- 事件payload包含属性名称

#### Scenario: 边界事件

**Given**: 设置值导致触及边界

**When**: 操作完成

**Then**:
-  MUST 触发 `OnAttributeReachedBoundary` 事件
- 事件包含边界类型（最大值或最小值）
- 事件包含边界值

### Requirement: Clamp逻辑集成

所有直接操作API MUST 内部调用clamp逻辑。

#### Scenario: 设置Base值时clamp

**Given**:
- 属性 `Health` 范围为 `[0, 100]`

**When**: 调用 `SetAttributeBaseValue(Actor, "Health", 150)`

**Then**:
- 值被clamp到100
- 使用两阶段clamp逻辑（如果适用）
- 触发相应事件

#### Scenario: 设置Current值时clamp

**Given**:
- 属性 `Health` 范围为 `[0, MaxHealth]`
- `MaxHealth.CurrentValue = 80`

**When**: 调用 `SetAttributeCurrentValue(Actor, "Health", 100)`

**Then**:
- 值被clamp到80
- 使用WorkingCurrent解析动态范围
- 触发相应事件

