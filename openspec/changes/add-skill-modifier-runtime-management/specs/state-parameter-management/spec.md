## ADDED Requirements
### Requirement: StateParam 公开读取默认 effective

TCS SHALL 将 StateParam 的公开业务读取默认定义为 effective value。`FTcs*StateParamInstance::GetBaseValue()` SHALL 仅表示 base value；无参 `GetModifiedValue()` SHALL 基于实例自身已绑定的求值上下文，沿激活中 modifier 链返回 effective value。`UTcsStateInstance::Get*ParamByTag` SHALL 默认返回 effective，并 MUST 通过 virtual `Get*ParamInstance` 定位真实参数宿主。

#### Scenario: GetBaseValue 与 GetModifiedValue 语义分离
- **WHEN** 一个 Numeric StateParam 的 base 为 10，且存在一个激活中的 +5 SkillModifier
- **THEN** `GetBaseValue()` MUST 返回 10
- **AND** 无参 `GetModifiedValue()` MUST 返回 15

#### Scenario: Get*ParamByTag 默认返回 effective
- **WHEN** 调用 `UTcsStateInstance::GetNumericParamByTag`（或 Bool / Vector 对应 API）读取已有 modifier 的参数
- **THEN** OutValue MUST 等于无参 `GetModifiedValue()`
- **AND** MUST NOT 等于仅 base 的 `GetBaseValue()`

#### Scenario: 显式 Base API 返回未修正值
- **WHEN** 调用方使用显式 Base 读取入口（如 `GetNumericBaseParamByTag`）
- **THEN** 返回值 MUST 等于 `GetBaseValue()`
- **AND** MUST 忽略激活中的 SkillModifier 链

#### Scenario: 禁止新增第三套同义求值入口
- **WHEN** 后续新增公开 StateParam 求值 API
- **THEN** 它 MUST 复用 base / effective 两套既有语义
- **AND** MUST NOT 再引入第三种同义命名（如 `GetResolvedValue` / `GetFinalValue`）表达同一 effective 语义

#### Scenario: ParamInstance 自行持有求值上下文
- **WHEN** StateParamInstance 被普通 State 创建
- **THEN** 参数宿主 MUST 将该 State 的 Instigator 绑定到参数实例
- **WHEN** StateParamInstance 被 SkillEntry 创建
- **THEN** 参数宿主 MUST 将该 SkillEntry 和所属 SkillComponent Owner 绑定到参数实例
- **AND** 调用方 MUST NOT 在调用 `GetModifiedValue()` 时再传入或反推这些上下文

#### Scenario: SkillModifier Evaluator 只允许由 ParamInstance 调度
- **WHEN** SkillModifier Evaluator 对一个参数求值
- **THEN** 调用 MUST 发生在对应 ParamInstance 的 `GetModifiedValue()` 链内
- **AND** Attribute、Condition、Blueprint 与其他外部业务调用面 MUST NOT 直接调用 Evaluator

### Requirement: 参数条件与跨模块消费复用 effective 读取

TCS SHALL 要求参数条件、公式输入以及其他跨模块的 StateParam 消费路径复用 host 的 effective 读取口径，而不是直接读取 base 字段。

#### Scenario: 参数条件读取 effective
- **WHEN** 参数条件通过 `GetNumericParamByTag` 判定某个阈值参数
- **AND** 该参数上存在激活中的 SkillModifier
- **THEN** 条件判定 MUST 使用 effective value
- **AND** MUST NOT 使用 base `GetBaseValue()` 作为判定输入
