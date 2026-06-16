## ADDED Requirements

### Requirement: Operand 动态绑定

`FTcsAttributeModifierInstance` SHALL 支持 `OperandBindings` 字段，声明 Operand → StateParam 的运行时绑定。

绑定 SHALL 使用 `FTcsStateParamBinding` 结构体：
- `OperandName`: FName（ModifierDef 内部约定的工具变量名）
- `StateParamTag`: FGameplayTag（绑定的 StateParam 标识）

#### Scenario: 绑定配置
- **WHEN** StateTree Task 配置 Modifier "AttrMod_Damage_Physical" 的 Magnitude 操作数绑定到 StateParam.Attack.Power
- **THEN** OperandBindings 中包含一条 {OperandName="Magnitude", StateParamTag=StateParam.Attack.Power}

### Requirement: RecalculateAttributeCurrentValues 中刷新 Operand

`RecalculateAttributeCurrentValues` SHALL 在执行每个 Modifier 之前，遍历其 `OperandBindings`，从对应的 StateParamInstance 拉取最新值并写入 `Operands`。

刷新采用"拉取"模式：StateParam 值变化只更新 Instance 内部状态，不主动触发下游；`RecalculateAttributeCurrentValues` 是唯一的刷新入口。

#### Scenario: 非 Snapshot Operand 刷新
- **WHEN** State Level 变化导致 StateLevelArray Param 值改变，随后 ApplyAttributeModifier 触发 RecalculateAttributeCurrentValues
- **THEN** 执行该 Modifier 前，Operands["Magnitude"] 被刷新为新值

#### Scenario: Snapshot Operand 不刷新
- **WHEN** bIsSnapshot=true 的 StateParam 已被求值过，触发 RecalculateAttributeCurrentValues
- **THEN** Evaluate 直接返回缓存，Operand 值不变

### Requirement: ResolveStateInstanceFromModifier

系统 SHALL 提供从 `FTcsAttributeModifierInstance` 追溯到其所属 `UTcsStateInstance` 的能力。

#### Scenario: 通过 SourceHandle 追溯
- **WHEN** Modifier 持有有效的 SourceHandle
- **THEN** ResolveStateInstanceFromModifier 返回正确的 UTcsStateInstance
