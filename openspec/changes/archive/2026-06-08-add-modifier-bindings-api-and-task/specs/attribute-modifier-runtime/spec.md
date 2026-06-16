## ADDED Requirements

### Requirement: CreateAttributeModifierWithBindings

`UTcsAttributeComponent` SHALL 提供 `CreateAttributeModifierWithBindings` 方法，接收 `ModifierId`、`Instigator`、`OperandBindings`。

该方法 SHALL：
1. 从 DefAsset 复制默认 Operands
2. 对于 OperandBindings 中每条绑定，从对应的 StateParamInstance 拉取初值并覆盖 Operand
3. 将 OperandBindings 写入实例
4. 后续 `RecalculateAttributeCurrentValues` 自动刷新 Operand

#### Scenario: 有绑定的 Operand 使用 StateParam 初值
- **WHEN** ModifierDef 的 Magnitude 默认值为 1.0，Bindings 绑定 Magnitude 到 StateParam.Attack.Power（当前值 15.5）
- **THEN** 创建的实例 Operands["Magnitude"] = 15.5

#### Scenario: 无绑定的 Operand 使用 DefAsset 默认值
- **WHEN** ModifierDef 有 Scale=1.0 但 Bindings 中无 Scale 绑定
- **THEN** 创建的实例 Operands["Scale"] = 1.0

### Requirement: 删除 CreateAttributeModifierWithOperands

`CreateAttributeModifierWithOperands` SHALL 从 `UTcsAttributeComponent` 和 `UTcsAttributeManagerSubsystem` 中删除。

#### Scenario: 旧 API 不可用
- **WHEN** 编译引用了 `CreateAttributeModifierWithOperands` 的代码
- **THEN** 编译失败，需迁移到 `CreateAttributeModifierWithBindings`
