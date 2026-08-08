# state-parameter-management Specification

## Purpose
TBD - created by archiving change add-stateparam-instance-operand-binding. Update Purpose after archive.
## Requirements
### Requirement: StateParamInstance 运行时实例化

`FTcsNumericStateParamInstance` SHALL 新增 `ModifierInstances` (TArray<FStateParamModifierInstance>) 和 `DeriveModifiedValue()` 方法。

#### Scenario: NumericValue 不被 Modifier 改写
- **WHEN** DeriveModifiedValue 被调用
- **THEN** NumericValue MUST 保持求值器产出的原始值不变

#### Scenario: DeriveModifiedValue 沿链求值
- **WHEN** 有多个 bActive==true 的 ModifierInstance
- **THEN** 按 Priority 降序依次调用 Evaluator->Evaluate()，返回最终值

### Requirement: Snapshot 求值策略

`Evaluate()` SHALL 根据 bIsSnapshot 决定求值行为：
- bIsSnapshot == true：首次求值后 bHasEvaluated 为 true，后续调用直接返回缓存
- bIsSnapshot == false：每次调用都重新求值

#### Scenario: Snapshot 参数只求值一次
- **WHEN** bIsSnapshot=true，首次调用 Evaluate
- **THEN** 求值器执行，Value 更新，bHasEvaluated=true
- **WHEN** 再次调用 Evaluate
- **THEN** 直接返回，求值器不再执行

#### Scenario: 非 Snapshot 参数每次求值
- **WHEN** bIsSnapshot=false，每次调用 Evaluate
- **THEN** 每次都重新调求值器，Value 更新

### Requirement: GameplayTag 统一标识

`TcsStateDefinition::Parameters` SHALL 使用 FGameplayTag 作为 Key。原有的 `TagParameters` 字段 SHALL 删除。

#### Scenario: 参数 Key 为 GameplayTag
- **WHEN** 配置 `StateParam.Attack.Power` 参数
- **THEN** Parameters Map 中以该 GameplayTag 为 Key 存储

### Requirement: 六容器替换

`UTcsStateInstance` 上统一的 `StateParamInstances` TMap SHALL 拆为 `NumericParamInstances` / `BoolParamInstances` / `VectorParamInstances` 三个独立容器。`UTcsSkillEntry` SHALL 同构。

#### Scenario: Populate 时分桶
- **WHEN** `PopulateStateParamInstances` 遍历 Def->Parameters
- **THEN** Numeric/Bool/Vector 分别写入对应容器

#### Scenario: SkillInstance 指向 Entry 容器
- **WHEN** `UTcsSkillInstance` 调用 `GetNumericParamInstances()`
- **THEN** MUST 返回 `SkillEntry->NumericParamInstances`

### Requirement: virtual PopulateStateParamInstances

`UTcsStateInstance` SHALL 提供 virtual 方法 PopulateStateParamInstances，替代旧的 UTcsStateComponent::EvaluateAndApplyStateParameters。

#### Scenario: 基类 populate
- **WHEN** 普通 State/Buff 类型调用
- **THEN** 从 StateDef.Parameters 遍历创建并求值 StateParamInstance，填充到 this->StateParamInstances

#### Scenario: SkillInstance 跳过
- **WHEN** UTcsSkillInstance 调用
- **THEN** 空实现（实例由 Entry 持有）

### Requirement: virtual GetStateParamInstance

`GetStateParamInstance` SHALL 拆为 `GetNumericParamInstance` / `GetBoolParamInstance` / `GetVectorParamInstance` 三个 virtual 方法。`GetStateParamInstances` SHALL 同步拆分为对应完整表访问器。

#### Scenario: 基类返回本地容器
- **WHEN** 普通 StateInstance 上调用 `GetBoolParamInstance(Tag)`
- **THEN** MUST 返回 `this->BoolParamInstances.Find(Tag)`

#### Scenario: SkillInstance 覆写指向 Entry
- **WHEN** SkillInstance 上调用 `GetBoolParamInstance(Tag)`
- **THEN** MUST 返回 `SkillEntry->BoolParamInstances.Find(Tag)`

### Requirement: Shared StateParam constant evaluator 可直接 authoring
TCS SHALL 将 Numeric / Bool / Vector 三类 shared `StateParam` evaluator 统一收敛为可直接 authoring 的 concrete constant evaluator，并保持三者对 typed payload 的默认解析语义一致。

#### Scenario: Bool 与 Vector evaluator 可作为 concrete 默认类被选择
- **WHEN** 开发者 authoring 一个 Bool 或 Vector 类型的 `FTcsStateParameter`
- **THEN** `UTcsStateBoolParamEvaluator` 与 `UTcsStateVectorParamEvaluator` SHALL 可作为可选的 concrete evaluator 类出现
- **AND** 它们 SHALL 不再因为抽象标记而失去默认值候选资格

#### Scenario: Shared evaluator 继续解析各自的 constant payload
- **WHEN** Numeric / Bool / Vector shared evaluator 在没有自定义子类覆写的情况下执行默认逻辑
- **THEN** 它们 SHALL 分别从各自 typed constant payload 中读取值
- **AND** 这种默认行为 SHALL 继续作为对应参数类型的稳定基线语义

#### Scenario: Shared StateParameter 只共享默认值规则
- **WHEN** DefAsset authoring 需要为 `FTcsStateParameter` 的 evaluator 字段补齐默认值
- **THEN** 系统 SHALL 允许复用 shared `FTcsStateParameter` 的默认值归一化逻辑
- **AND** DefAsset 级别的参数合法性校验 SHALL 留在具体 DefAsset 自身
- **AND** shared `FTcsStateParameter` SHALL NOT 成为通用 DefAsset 校验入口

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

### Requirement: StateParam effective 变化可通知依赖失效且禁止同步 Attribute 重算

Numeric StateParam 的 effective 值在成功提交且发生真实业务可见变化时，若存在登记了对应 DependencyKey 的 Ongoing 依赖，系统 MUST 向目标 `UTcsAttributeComponent` 发布依赖失效通知（递增 DependencyRevision 并 MarkDirty 相关 Ongoing）。StateParam 值变化 MUST NOT 同步调用 AttributeComponent 的完整 Ongoing 重算或 `RecalculateAttributeCurrentValues` 全图入口。实际 Attribute 重算 MUST 仅由 AttributeComponent 在受控安全点或帧末 Flush 中执行。

#### Scenario: effective 变化触发标脏而非同步重算
- **WHEN** 本地 Buff 上某 Numeric StateParam 的 effective 从 10 变为 15
- **AND** 目标 AttributeComponent 上存在依赖该 Param 的 Ongoing 父实例
- **THEN** 系统 MUST 使这些父实例进入 Dirty 集合（或等价失效队列）
- **AND** StateParam 提交路径 MUST NOT 在返回前同步完成 Attribute 的完整依赖图重算

#### Scenario: 无有效变化不通知
- **WHEN** StateParam effective 写入结果与上次已提交 effective 无真实变化
- **THEN** 系统 MUST NOT 产生多余的依赖失效通知

### Requirement: 跨模块消费仍读 effective

在引入依赖失效通知后，参数条件、Attribute Operand Evaluator 以及其他跨模块 StateParam 消费路径 MUST 继续复用 host 的 effective 读取口径（无参 `GetModifiedValue()` / `Get*ParamByTag`），MUST NOT 退回仅读 base。

#### Scenario: Attribute Evaluator 仍读 effective
- **WHEN** Attribute StateParam Operand Evaluator 求值
- **THEN** 它 MUST 使用 effective `GetModifiedValue()`
- **AND** MUST NOT 仅使用 `GetBaseValue()`

