## MODIFIED Requirements

### Requirement: 公开参数读取默认返回 effective value

TCS SHALL 让业务可见的 Skill / State 参数读取默认返回 effective value（base + 激活中的 SkillModifier 链结果）。无参 `GetModifiedValue()` 与任何默认参数读取 API SHALL 使用 ParamInstance 自身绑定的上下文；`GetBaseValue()` 与任何显式 Base API SHALL 仅表示未经 SkillModifier 链改写的 base value。跨系统消费（含 AttributeModifier 的 StateParam OperandEvaluator、参数条件、冷却进度）MUST 复用同一 effective 读取口径，而不是各自读取 base 字段。

#### Scenario: Get*ParamByTag 默认返回 SkillModifier 修正后的值
- **WHEN** 目标 `SkillEntry` 参数上已存在激活中的 SkillModifier
- **AND** 调用方通过 `UTcsStateInstance` / `UTcsSkillInstance` 的 `Get*ParamByTag` 读取该参数
- **THEN** 返回值 MUST 等于无参 `GetModifiedValue()` 的结果
- **AND** 返回值 MUST NOT 仅等于 base `GetBaseValue()`

#### Scenario: SkillInstance 读取真实宿主参数而不是本地空容器
- **WHEN** `UTcsSkillInstance` 调用 `Get*ParamByTag`
- **THEN** 它 MUST 通过 virtual `Get*ParamInstance` 定位到 `SkillEntry` 上的参数实例
- **AND** MUST NOT 因读取本地空容器而返回缺失或 base 默认值

#### Scenario: 需要 base 时必须走显式 Base API
- **WHEN** 调用方明确只需要未经 SkillModifier 修正的基础参数值
- **THEN** 它 MUST 调用显式 Base 读取入口（如 `Get*BaseParamByTag`）
- **AND** 默认 `Get*ParamByTag` MUST 继续返回 effective

#### Scenario: 冷却进度分母使用 effective 冷却时长
- **WHEN** 技能冷却已启动，且冷却参数上存在激活中的 SkillModifier
- **THEN** `GetRemainingCooldownRatio` 使用的冷却分母 MUST 为 effective 冷却时长
- **AND** MUST NOT 继续使用 base `GetBaseValue()` 作为分母

#### Scenario: 新增公开参数读取 API 不得绕过 effective 契约
- **WHEN** 后续新增任何公开的、面向业务的 StateParam 解析值读取 API
- **THEN** 该 API MUST 默认返回 effective value
- **AND** 若接口要暴露 base，MUST 在 API 名称中明确声明 Base
- **AND** MUST NOT 直接把 `NumericValue` / `GetBaseValue()` 作为业务默认返回

## ADDED Requirements

### Requirement: SkillInstance 不得直接施加 Ongoing AttributeModifier

SkillInstance SHALL 只能直接施加 Instant AttributeModifier，或通过目标本地 StateInstance（首版以 BuffInstance 为宿主）间接产生 Ongoing AttributeModifier。直接 Ongoing 请求 MUST 硬拒绝、零修改，并在 Development / Editor 输出 Warning。

#### Scenario: Skill 直接 Ongoing 被拒绝
- **WHEN** SkillInstance 调用 `ApplyAttributeModifier` 且 `ApplicationMode = Ongoing`
- **THEN** 系统 MUST 拒绝该请求并保持零修改
- **AND** Development / Editor MUST 输出 Warning

#### Scenario: Skill 可通过目标本地 StateInstance 间接 Ongoing
- **WHEN** SkillInstance 先在目标 Actor 上创建 / 施加 StateInstance
- **AND** 该 StateInstance 再 Apply Ongoing AttributeModifier
- **THEN** 该路径 MUST 被允许
- **AND** Ongoing 生命周期 MUST 由该目标本地 StateInstance 管理
