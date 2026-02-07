# Spec: 属性添加防覆盖

## 概述

本规范定义属性添加操作的防覆盖行为,确保 `AddAttribute` 系列函数不会静默覆盖已存在的属性,并提供明确的返回值语义。

## ADDED Requirements

### Requirement: AddAttribute 不覆盖已存在属性

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::AddAttribute` MUST reject adding when attribute already exists, output warning log, and return directly without overwriting existing attribute value. `UTcsAttributeManagerSubsystem::AddAttribute` 在属性已存在时必须拒绝添加,输出 warning 日志,并直接返回,不覆盖已存在的属性值。

#### Scenario: 拒绝覆盖已存在属性

**Given** 战斗实体 `CombatEntity` 已经拥有属性 `"HP"`,其 `BaseValue` 为 `100.0`,`CurrentValue` 为 `80.0`

**When** 调用 `AddAttribute(CombatEntity, "HP", 200.0)`

**Then** 函数必须直接返回,不修改任何状态

**And** 必须输出 Warning 级别的日志,包含以下信息:
- 属性名称 `"HP"`
- 战斗实体名称
- 提示信息 `"already exists"` 和 `"skipping"`

**And** 属性 `"HP"` 的 `BaseValue` 仍为 `100.0`

**And** 属性 `"HP"` 的 `CurrentValue` 仍为 `80.0`

**And** 不应触发任何属性变化事件

#### Scenario: 首次添加属性正常工作

**Given** 战斗实体 `CombatEntity` 不拥有属性 `"HP"`

**When** 调用 `AddAttribute(CombatEntity, "HP", 100.0)`

**Then** 函数必须成功添加属性

**And** 属性 `"HP"` 的 `BaseValue` 为 `100.0`

**And** 属性 `"HP"` 的 `CurrentValue` 为 `100.0` (经过 Clamp)

**And** 不应输出 warning 日志

---

### Requirement: AddAttributes 批量防覆盖

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::AddAttributes` MUST apply the same no-overwrite logic for each attribute. `UTcsAttributeManagerSubsystem::AddAttributes` 必须对每个属性应用相同的防覆盖逻辑。

#### Scenario: 批量添加时跳过已存在属性

**Given** 战斗实体 `CombatEntity` 已经拥有属性 `"HP"`

**And** 不拥有属性 `"MP"` 和 `"Stamina"`

**When** 调用 `AddAttributes(CombatEntity, ["HP", "MP", "Stamina"])`

**Then** 必须跳过 `"HP"`,输出 warning 日志

**And** 必须成功添加 `"MP"` 和 `"Stamina"`

**And** 最终 `CombatEntity` 拥有三个属性: `"HP"`, `"MP"`, `"Stamina"`

**And** `"HP"` 的值不应被修改

---

### Requirement: AddAttributeByTag 返回值语义明确

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeManagerSubsystem::AddAttributeByTag` return value MUST express "whether actually added successfully" instead of just "whether Tag resolved successfully". `UTcsAttributeManagerSubsystem::AddAttributeByTag` 的返回值必须表达"是否真的添加成功",而不仅仅是"Tag 是否解析成功"。

#### Scenario: Tag 无效时返回 false

**Given** GameplayTag `AttributeTag` 无效或未在映射中注册

**When** 调用 `AddAttributeByTag(CombatEntity, AttributeTag, 100.0)`

**Then** 函数必须返回 `false`

**And** 不应添加任何属性

**And** 应输出相应的错误或警告日志

#### Scenario: 属性已存在时返回 false

**Given** GameplayTag `AttributeTag` 有效,解析为属性名称 `"HP"`

**And** 战斗实体 `CombatEntity` 已经拥有属性 `"HP"`

**When** 调用 `AddAttributeByTag(CombatEntity, AttributeTag, 100.0)`

**Then** 函数必须返回 `false`

**And** 必须输出 Warning 级别的日志

**And** 不应修改属性 `"HP"` 的值

#### Scenario: 成功添加时返回 true

**Given** GameplayTag `AttributeTag` 有效,解析为属性名称 `"HP"`

**And** 战斗实体 `CombatEntity` 不拥有属性 `"HP"`

**When** 调用 `AddAttributeByTag(CombatEntity, AttributeTag, 100.0)`

**Then** 函数必须返回 `true`

**And** 必须成功添加属性 `"HP"`

**And** 不应输出 warning 日志

---

### Requirement: 防覆盖日志格式统一

**优先级**: P1 (重要)

**描述**:
All no-overwrite related logs MUST follow unified format for easier debugging and issue tracking. 所有防覆盖相关的日志必须遵循统一的格式,便于调试和问题追踪。

#### Scenario: 统一的日志格式

**Given** 任何 AddAttribute 相关函数检测到属性已存在

**When** 输出 warning 日志

**Then** 日志必须包含以下信息:
- 函数名称(如 `[AddAttribute]`, `[AddAttributeByTag]`)
- 属性名称
- 战斗实体名称或标识
- 明确的提示信息(如 `"already exists, skipping"`)

**And** 日志级别必须为 Warning

**And** 日志通道必须为 `LogTcsAttribute`

---

## 相关规范

- `attribute-tags` - 属性 Tag 系统

## 实施注意事项

1. **向后兼容性**: 此变更会改变现有行为,可能暴露重复添加的问题
2. **调用方修复**: 需要检查所有调用方,确保没有依赖覆盖行为
3. **测试覆盖**: 必须测试首次添加和重复添加两种场景
4. **文档更新**: 需要更新 API 文档,说明新的行为
