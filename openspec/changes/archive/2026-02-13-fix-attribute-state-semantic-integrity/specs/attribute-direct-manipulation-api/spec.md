# Spec: 属性直接操作API

## ADDED Requirements

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

## Implementation Notes

### API签名

```cpp
// UTcsAttributeManagerSubsystem

/**
 * 直接设置属性的Base值
 * @param CombatEntity 战斗实体
 * @param AttributeName 属性名称
 * @param NewValue 新的Base值
 * @param bTriggerEvents 是否触发事件（默认true）
 * @return 是否成功设置
 */
UFUNCTION(BlueprintCallable, Category = "Attribute")
bool SetAttributeBaseValue(
    AActor* CombatEntity,
    FName AttributeName,
    float NewValue,
    bool bTriggerEvents = true);

/**
 * 直接设置属性的Current值
 * @param CombatEntity 战斗实体
 * @param AttributeName 属性名称
 * @param NewValue 新的Current值
 * @param bTriggerEvents 是否触发事件（默认true）
 * @return 是否成功设置
 */
UFUNCTION(BlueprintCallable, Category = "Attribute")
bool SetAttributeCurrentValue(
    AActor* CombatEntity,
    FName AttributeName,
    float NewValue,
    bool bTriggerEvents = true);

/**
 * 重置属性到定义的初始值
 * @param CombatEntity 战斗实体
 * @param AttributeName 属性名称
 * @return 是否成功重置
 */
UFUNCTION(BlueprintCallable, Category = "Attribute")
bool ResetAttribute(
    AActor* CombatEntity,
    FName AttributeName);

/**
 * 移除属性
 * @param CombatEntity 战斗实体
 * @param AttributeName 属性名称
 * @return 是否成功移除
 */
UFUNCTION(BlueprintCallable, Category = "Attribute")
bool RemoveAttribute(
    AActor* CombatEntity,
    FName AttributeName);
```

### 使用场景

1. **调试工具**: 编辑器工具直接修改属性值进行测试
2. **属性重置**: 角色死亡后重置所有属性
3. **属性校正**: 修复因bug导致的属性异常
4. **批量操作**: 禁用事件进行批量属性修改，最后统一触发事件

### 注意事项

- 这些API绕过了修改器系统，应谨慎使用
- 建议在文档中说明使用场景和注意事项
- 不应在常规游戏逻辑中频繁使用，优先使用修改器

## Related Specs

- `attribute-dynamic-range-two-phase-clamp`: 两阶段clamp逻辑
- `attribute-boundary-events`: 边界事件
