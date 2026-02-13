# Spec: 属性动态范围两阶段Clamp

## ADDED Requirements

### Requirement: 动态范围Clamp使用两阶段逻辑

属性值的动态范围约束 MUST 使用两阶段clamp逻辑，明确区分Base值和Current值的范围解析。

#### Scenario: Base值使用WorkingBase解析动态范围

**Given**:
- 属性 `Health` 的范围定义为 `[0, MaxHealth]`
- `MaxHealth` 是另一个属性
- `Health.BaseValue = 100`
- `MaxHealth.BaseValue = 80`

**When**: 对Health的Base值执行clamp

**Then**:
- 使用 `MaxHealth.WorkingBase` (80) 作为上限
- `Health.BaseValue` 被clamp到80
- 不使用 `MaxHealth.CurrentValue`

#### Scenario: Current值使用WorkingCurrent解析动态范围

**Given**:
- 属性 `Health` 的范围定义为 `[0, MaxHealth]`
- `MaxHealth` 是另一个属性
- `Health.CurrentValue = 100`
- `MaxHealth.WorkingCurrent = 120` (包含修改器效果)

**When**: 对Health的Current值执行clamp

**Then**:
- 使用 `MaxHealth.WorkingCurrent` (120) 作为上限
- `Health.CurrentValue` 被clamp到100（未超出范围）
- 不使用 `MaxHealth.BaseValue`

#### Scenario: 两阶段clamp执行顺序

**Given**: 一组属性需要应用修改器并clamp

**When**: 执行属性更新流程

**Then**:
1. 计算所有属性的WorkingBase（应用Base修改器）
2. 对每个属性的Base值执行clamp（使用WorkingBase解析范围）
3. 计算所有属性的WorkingCurrent（应用Current修改器）
4. 对每个属性的Current值执行clamp（使用WorkingCurrent解析范围）

### Requirement: 范围解析器接口

范围解析 MUST 支持指定使用Base值还是Current值。

#### Scenario: 范围解析器参数

**Given**: 动态范围需要解析

**When**: 调用范围解析函数

**Then**:
-  MUST 接受 `FAttributeValueResolver` 参数
- Resolver指定使用WorkingBase还是WorkingCurrent
- 解析结果基于指定的值类型

## ADDED Requirements

### Requirement: FAttributeValueResolver结构

 MUST 提供属性值解析器，用于指定范围解析时使用的值类型。

#### Scenario: Resolver定义

**Given**: 需要解析动态范围

**When**: 创建Resolver

**Then**:
- Resolver MUST 指定值类型（Base或Current）
- Resolver MUST 提供属性值查询接口
- Resolver MUST 处理属性不存在的情况

#### Scenario: WorkingBase Resolver

**Given**: 解析Base值的动态范围

**When**: 创建WorkingBase Resolver

**Then**:
- Resolver返回属性的WorkingBase值
- WorkingBase = BaseValue + Base修改器效果
- 不包含Current修改器效果

#### Scenario: WorkingCurrent Resolver

**Given**: 解析Current值的动态范围

**When**: 创建WorkingCurrent Resolver

**Then**:
- Resolver返回属性的WorkingCurrent值
- WorkingCurrent = BaseValue + Base修改器 + Current修改器
- 包含所有修改器效果

### Requirement: Clamp函数签名更新

`ClampAttributeValueInRange`  MUST 接受Resolver参数。

#### Scenario: Clamp函数调用

**Given**: 需要clamp属性值

**When**: 调用 `ClampAttributeValueInRange`

**Then**:
-  MUST 传入Resolver参数
- Resolver用于解析动态范围
- Clamp结果基于Resolver提供的值

## REMOVED Requirements

无移除的需求。

## Implementation Notes

### FAttributeValueResolver定义

```cpp
struct FAttributeValueResolver
{
    enum class EValueType
    {
        WorkingBase,
        WorkingCurrent
    };

    EValueType ValueType;
    const UTcsAttributeComponent* AttributeComponent;

    float GetAttributeValue(FName AttributeName) const
    {
        if (ValueType == EValueType::WorkingBase)
        {
            // 返回WorkingBase
        }
        else
        {
            // 返回WorkingCurrent
        }
    }
};
```

### 两阶段Clamp实现

```cpp
// 阶段1: Base值Clamp
FAttributeValueResolver BaseResolver(EValueType::WorkingBase, AttributeComponent);
for (auto& Attr : Attributes)
{
    ClampAttributeValueInRange(AttributeComponent, Attr.Key, Attr.Value.BaseValue, nullptr, nullptr, &BaseResolver);
}

// 阶段2: Current值Clamp
FAttributeValueResolver CurrentResolver(EValueType::WorkingCurrent, AttributeComponent);
for (auto& Attr : Attributes)
{
    ClampAttributeValueInRange(AttributeComponent, Attr.Key, Attr.Value.CurrentValue, nullptr, nullptr, &CurrentResolver);
}
```

### 性能考虑

- 两阶段clamp增加计算量，但避免语义不清
- 可以通过缓存WorkingBase和WorkingCurrent优化
- 只有使用动态范围的属性才受影响

## Related Specs

- `attribute-boundary-events`: 属性边界事件
- `attribute-dynamic-range-enforcement`: 动态范围强制执行
