## ADDED Requirements
### Requirement: DefAsset 策略字段默认值与镜像同步
TCS 编辑器 authoring 集成 SHALL 为受支持 DefAsset 中声明为策略模式的 `UClass` 字段提供稳定的 concrete 默认类；并在 DataTable ↔ DefAsset 双向同步中把这些默认类显式镜像到对应 RowStruct。DataTableRow 自身 SHALL NOT 复制 DefAsset 级别的有效性勘误职责。

#### Scenario: AttributeDef 使用线性 Clamp 作为默认策略
- **WHEN** 开发者新建或同步得到一个 `UTcsAttributeDefinition`
- **AND** `ClampStrategyClass` 为空
- **THEN** authoring 或同步流程 SHALL 将其归一化为 `UTcsAttrClampStrategy_Linear`
- **AND** 后续回写的 `FTcsAttributeDefRow.ClampStrategyClass` SHALL 显式写入同一 concrete 类

#### Scenario: AttributeModifierDef、BuffDef 与 StateSlotDef 使用 concrete 默认策略
- **WHEN** 开发者新建或同步得到 `UTcsAttributeModifierDefinition`、`UTcsBuffDefinition` 或 `UTcsStateSlotDefinition`
- **AND** 其可默认化策略字段为空
- **THEN** `MergerType` / `SamePriorityPolicy` SHALL 分别归一化为 `UTcsAttrModMerger_NoMerge`、`UTcsBuffMerger_NoMerge`、`UTcsStateSamePriorityPolicy_UseNewest`
- **AND** 对应 RowStruct 在回写时 SHALL 显式写入这些 concrete 默认类

#### Scenario: StateParameter 与 Skill cooldown 使用共享 constant evaluator 默认值
- **WHEN** 开发者在 `UTcsStateDefinition::Parameters` 或 `UTcsSkillDefinition::CooldownParam` 中 authoring 一个 `FTcsStateParameter`
- **AND** 当前 `ParameterType` 对应的 evaluator 类为空
- **THEN** 系统 SHALL 为 Numeric / Bool / Vector 分别归一化到各自 shared constant evaluator
- **AND** 对应 RowStruct 的嵌套 `FTcsStateParameter` 数据 SHALL 镜像同一 concrete evaluator 类

#### Scenario: SkillModifierDef 使用类型匹配的 concrete 默认执行器
- **WHEN** 开发者新建或同步得到一个 `UTcsSkillModifierDefinition`
- **AND** `TargetParamType` 已确定，但对应类型的 evaluator 类为空
- **THEN** Numeric / Bool / Vector SHALL 分别归一化到 `UTcsSkillModExec_Addition`、`UTcsSkillModExec_SetBool`、`UTcsSkillModExec_SetVector`
- **AND** 对应 `FTcsSkillModifierDefRow` SHALL 显式镜像相同的 concrete 执行器类

#### Scenario: DataTableRow 不承担 DefAsset 级勘误
- **WHEN** 某个受支持 RowStruct 中的默认化策略字段为空或缺失
- **THEN** Row → Asset 同步流程 MAY 归一化补值
- **BUT** RowStruct 本身 SHALL NOT 提供 `IsDataValid`、错误日志或编辑器通知级别的勘误入口

### Requirement: DefAsset 策略字段有效性勘误
TCS 编辑器 authoring 集成 SHALL 在 DefAsset 级别校验受支持策略字段的有效性，并在字段缺失、落到抽象类或与当前参数类型不匹配时提供明确错误提示。

#### Scenario: 缺失或抽象策略类触发勘误
- **WHEN** 某个 DefAsset 的必需策略字段为空，或被设置为抽象类
- **THEN** 编辑器校验 SHALL 报告错误
- **AND** 报错信息 SHALL 明确指出字段名、资产类型与期望的 concrete authoring 方向

#### Scenario: SkillModifier evaluator 与目标参数类型不匹配
- **WHEN** `UTcsSkillModifierDefinition` 的 `TargetParamType` 为 Bool 或 Vector
- **AND** 开发者填入了非对应类型的 evaluator 字段，或遗漏了对应字段
- **THEN** 编辑器校验 SHALL 报告错误
- **AND** 只有与 `TargetParamType` 匹配的 evaluator 字段才应被视为有效 authoring 输入

#### Scenario: 非默认项不被强行补值
- **WHEN** 开发者 authoring `UTcsAttributeModifierDefinition::ModifierType` 或 `ActiveConditions`
- **THEN** 编辑器 SHALL NOT 因为这些字段为空而自动注入默认值
- **AND** 只有明确纳入默认值范围的策略字段才参与自动归一化

#### Scenario: StateParameter 默认值共享不替代 DefAsset 本地校验
- **WHEN** `UTcsStateDefinition::Parameters` 或 `UTcsSkillDefinition::CooldownParam` 依赖 shared `FTcsStateParameter` 默认 evaluator 规则
- **THEN** 默认值归一化 MAY 复用 shared `FTcsStateParameter` 的公共逻辑
- **AND** 参数是否合法 SHALL 继续由拥有该字段的 DefAsset 在自身 `IsDataValid()` 中判定
- **AND** 系统 SHALL NOT 通过 `Instance` 层导出通用 DefAsset 校验入口