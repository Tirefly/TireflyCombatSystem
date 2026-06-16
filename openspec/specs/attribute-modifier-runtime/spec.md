# attribute-modifier-runtime Specification

## Purpose
TBD - created by archiving change add-stateparam-instance-operand-binding. Update Purpose after archive.
## Requirements
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

### Requirement: ResolveStateParamInstances

`ResolveStateParamInstances` SHALL 更名为 `ResolveNumericParamInstances`，返回类型从 `TMap<FGameplayTag, FTcsStateParamInstance>*` 改为 `TMap<FGameplayTag, FTcsNumericStateParamInstance>*`。

#### Scenario: 仅返回 Numeric 容器
- **WHEN** 调用 ResolveNumericParamInstances
- **THEN** 返回的 Map 中仅包含 Numeric 类型的 StateParam 实例

#### Scenario: Operand 刷新无需类型判空
- **WHEN** RecalculateAttributeCurrentValues 通过 ResolveNumericParamInstances 获取 ParamInstance
- **THEN** MUST NOT 再检查 CachedEvaluator 是否为 null

### Requirement: CreateAttributeModifierWithBindings

`UTcsAttributeComponent` SHALL 提供 `CreateAttributeModifierWithBindings` 方法，接收 `ModifierId`、`Instigator`、`OperandBindings`。

该方法 SHALL：
1. 从 DefAsset 复制默认 Operands
2. 将 OperandBindings 写入实例
3. 首值使用 DefAsset 默认值；首次 `RecalculateAttributeCurrentValues` 时从对应的 StateParamInstance 拉取并覆盖 Operand
4. 后续 `RecalculateAttributeCurrentValues` 自动刷新 Operand

#### Scenario: 绑定的 Operand 创建时使用 DefAsset 默认值，首次 Recalculate 后拉取
- **WHEN** ModifierDef 的 Magnitude 默认值为 1.0，Bindings 绑定 Magnitude 到 StateParam.Attack.Power（当前值 15.5）
- **THEN** 创建后 Operands["Magnitude"] = 1.0（DefAsset 默认值）
- **AND** 首次 RecalculateAttributeCurrentValues 后 Operands["Magnitude"] = 15.5

#### Scenario: 无绑定的 Operand 使用 DefAsset 默认值
- **WHEN** ModifierDef 有 Scale=1.0 但 Bindings 中无 Scale 绑定
- **THEN** 创建的实例 Operands["Scale"] = 1.0

### Requirement: 删除 CreateAttributeModifierWithOperands

`CreateAttributeModifierWithOperands` SHALL 从 `UTcsAttributeComponent` 和 `UTcsAttributeManagerSubsystem` 中删除。

#### Scenario: 旧 API 不可用
- **WHEN** 编译引用了 `CreateAttributeModifierWithOperands` 的代码
- **THEN** 编译失败，需迁移到 `CreateAttributeModifierWithBindings`

### Requirement: AttributeModifierInstance 新增引用字段

`FTcsAttributeModifierInstance` SHALL 新增 `SourceStateInstance` 和 `SourceSkillEntry` 两个 TWeakObjectPtr 字段，用于在 Operand 刷新时直接定位 NumericParamInstances 的所在容器，不依赖 SourceHandle 回溯。

#### Scenario: SourceStateInstance 直接定位
- **WHEN** Modifier 直接关联一个活跃的 StateInstance
- **THEN** ResolveNumericParamInstances 通过 SourceStateInstance 直接返回其 NumericParamInstances

#### Scenario: SourceSkillEntry 覆盖 AOE 场景
- **WHEN** Modifier 由 AOE 领域创建，源技能已结束但 SkillEntry 仍存活
- **THEN** ResolveNumericParamInstances 通过 SourceSkillEntry 返回 NumericParamInstances

