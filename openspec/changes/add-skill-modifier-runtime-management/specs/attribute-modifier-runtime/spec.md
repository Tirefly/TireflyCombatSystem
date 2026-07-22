## MODIFIED Requirements
### Requirement: RecalculateAttributeCurrentValues 中刷新 Operand

`RecalculateAttributeCurrentValues` SHALL 在执行每个 Modifier 之前，遍历其 `OperandBindings`，从对应的 StateParamInstance 拉取最新 **effective** 值并写入 `Operands`。

刷新采用"拉取"模式：StateParam 值变化只更新 Instance 内部状态，不主动触发下游；`RecalculateAttributeCurrentValues` 是唯一的刷新入口。

对绑定到带 SkillModifier 链的 StateParam：
- 刷新结果 MUST 等于无参 `GetModifiedValue()`
- MUST NOT 仅写入 base `GetBaseValue()` / `NumericValue`

#### Scenario: 非 Snapshot Operand 刷新
- **WHEN** State Level 变化导致 StateLevelArray Param 值改变，随后 ApplyAttributeModifier 触发 RecalculateAttributeCurrentValues
- **THEN** 执行该 Modifier 前，Operands["Magnitude"] 被刷新为新值

#### Scenario: Snapshot Operand 不刷新 Evaluator 但可反映 modifier 链
- **WHEN** `bIsSnapshot=true` 的 StateParam 已被求值过，触发 RecalculateAttributeCurrentValues
- **THEN** Evaluate 直接返回缓存，base 不再重算
- **AND** 若该参数上存在激活中的 SkillModifier，写入 `Operands` 的值 MUST 仍反映 effective 结果

#### Scenario: OperandBinding 读取 SkillModifier 修正后的 StateParam
- **WHEN** 绑定源 StateParam 上存在激活中的 SkillModifier
- **AND** RecalculateAttributeCurrentValues 刷新该 OperandBinding
- **THEN** 写入 `Operands` 的值 MUST 等于该 StateParam 的无参 `GetModifiedValue()`
- **AND** MUST NOT 等于仅 base 的 `GetBaseValue()`

## ADDED Requirements
### Requirement: AttributeModifier 已解析 Operand 的统一访问口径

TCS SHALL 将 `FTcsAttributeModifierInstance::Operands` 视为“已解析 Operand 值”的权威读取位置。新增公开业务 API 若需要 AttributeModifierInstance 的解析值，MUST 读取刷新后的 `Operands`，而不是各自重新实现一遍 StateParam base 拉取。

#### Scenario: 新增公开 API 读取已解析 Operand
- **WHEN** 后续新增公开 API 需要读取某个 AttributeModifierInstance 的已解析 Operand
- **THEN** 该 API MUST 读取 `Operands`
- **AND** 调用语义 MUST 建立在 OperandBinding 已完成刷新的前提上
- **AND** MUST NOT 为业务默认路径再次直接调用 `GetBaseValue()` 旁路拉取 base

#### Scenario: 不给 AttributeModifierInstance 新增平行的参数求值 API
- **WHEN** 设计 AttributeModifierInstance 的公开读取面
- **THEN** TCS MUST NOT 再为 ModifierInstance 新增一套与 StateParam `GetModifiedValue` 平行的参数求值入口
- **AND** Attribute 自身 effective 结果仍由 Recalculate 写入属性 CurrentValue
