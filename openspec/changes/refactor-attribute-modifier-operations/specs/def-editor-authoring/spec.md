## MODIFIED Requirements

### Requirement: Row Struct 直接赋值映射

除标识字段外，Row Struct 与 DefAsset 的 UPROPERTY SHALL 保持类型和名称 1:1 一致。映射通过每种类型各一对编译期静态 `SyncRowToAsset` / `SyncAssetToRow` 函数直接赋值，不使用运行时反射。

#### Scenario: `RowName` 承载 DefId
- **WHEN** 同步系统处理任意一行 DataTable 与其对应 DefAsset
- **THEN** `RowName` SHALL 作为唯一主键并承载 DefId
- **AND** Row Struct 内 SHALL NOT 再重复声明 `DefId` / `AttributeDefId` / `ModifierId` / `StateSlotDefId` 等同义标识字段

#### Scenario: 直接赋值编译期校验完整性
- **WHEN** DefAsset 新增或删除一个 UPROPERTY 字段
- **THEN** 对应的 Sync 函数编译报错，强制同步更新 Row Struct

#### Scenario: 嵌套结构体在 DataTable 中自动展开
- **WHEN** Row 包含 `FTcsStateParameter CooldownParam`
- **THEN** DataTable 编辑器自动展开为子列 `CooldownParam.ParameterType`、`CooldownParam.NumericParamEvaluator` 等

#### Scenario: FInstancedStruct 和 FGameplayTagContainer 完整映射
- **WHEN** Row Struct 包含 `FInstancedStruct` 或 `FGameplayTagContainer` 类型的 UPROPERTY
- **THEN** 直接赋值 MUST 完整深拷贝到 DefAsset 属性
- **AND** DataTable 编辑器 MUST 能直接编辑这些类型的值

#### Scenario: AttributeModifier Row 与新 Definition 非标识字段保持 1:1
- **WHEN** 同步 `UTcsAttributeModifierDefinition` 与 `FTcsAttributeModifierDefRow`
- **THEN** Priority、Merger、Operation Map 以及每条 Operation 的 TargetAttributeId、Operator、Evaluator、OperandPayload、Custom Operator 等非标识字段 MUST 名称与类型 1:1
- **AND** Row / Asset MUST NOT 再包含旧 `AttributeId`、`ModifierMode`、`Operands` 或 `ModifierType`

#### Scenario: Operation Map 中嵌套 OperandPayload 可编辑并完整深拷贝
- **WHEN** AttributeModifier Operation 使用 `FInstancedStruct` 承载 OperandPayload
- **THEN** DataTable 与 DefAsset 编辑器 MUST 能编辑该 Payload
- **AND** 双向同步 MUST 完整深拷贝 Payload 内容，不得丢失嵌套字段

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
- **AND** AttributeModifierDef MUST NOT 再因已删除的 `ModifierType` 或其他非默认项被自动补值

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
TCS 编辑器 authoring 集成 SHALL 在 DefAsset 级别校验受支持策略字段的有效性，并在字段缺失、落到抽象类、与当前参数类型不匹配，或违反 Operator / Merger 兼容规则时提供明确错误提示。

#### Scenario: 缺失或抽象策略类触发勘误
- **WHEN** 某个 DefAsset 的必需策略字段为空，或被设置为抽象类
- **THEN** 编辑器校验 SHALL 报告错误
- **AND** 报错信息 SHALL 明确指出字段名、资产类型与期望的 concrete authoring 方向

#### Scenario: SkillModifier evaluator 与目标参数类型不匹配
- **WHEN** `UTcsSkillModifierDefinition` 的 `TargetParamType` 为 Bool 或 Vector
- **AND** 开发者填入了非对应类型的 evaluator 字段，或遗漏了对应字段
- **THEN** 编辑器校验 SHALL 报告错误
- **AND** 只有与 `TargetParamType` 匹配的 evaluator 字段才应被视为有效 authoring 输入

#### Scenario: Operation 默认 Operand 保持可编辑
- **WHEN** 开发者新建 AttributeModifier Operation
- **THEN** 系统 SHALL 默认配置 Constant Operand Evaluator 与 Constant OperandPayload
- **AND** 系统 MUST NOT 默认配置 Operator 或 Custom Operator
- **AND** 编辑器 MUST NOT 在后续编辑、同步或校验期间重置开发者选择的 Evaluator 或 OperandPayload
- **AND** Buff/State 的 `ActiveConditions` 为空时，编辑器 SHALL NOT 自动注入默认值

#### Scenario: StateParameter 默认值共享不替代 DefAsset 本地校验
- **WHEN** `UTcsStateDefinition::Parameters` 或 `UTcsSkillDefinition::CooldownParam` 依赖 shared `FTcsStateParameter` 默认 evaluator 规则
- **THEN** 默认值归一化 MAY 复用 shared `FTcsStateParameter` 的公共逻辑
- **AND** 参数是否合法 SHALL 继续由拥有该字段的 DefAsset 在自身 `IsDataValid()` 中判定
- **AND** 系统 SHALL NOT 通过 `Instance` 层导出通用 DefAsset 校验入口

#### Scenario: AttributeModifier Operator 与 Merger 不兼容时报错
- **WHEN** AttributeModifierDef 的 Operator / Merger 组合在 `TcsDeveloperSettings` 中为 Forbidden
- **THEN** `IsDataValid` MUST 报告 Error
- **AND** 系统 MUST NOT 自动清空用户已有 Operator 或 Merger 选择

#### Scenario: 多 Operation 使用内建选择或聚合 Merger 时报错
- **WHEN** AttributeModifierDef 的 Operation Map 含多个 Operation
- **AND** Merger 为 `UseMaximum`、`UseMinimum`、`UseAdditiveSum`、`UseNewest` 或 `UseOldest`
- **THEN** Data Validation MUST 默认报告 Error
- **AND** 若项目设置将该组合降级，则 MAY 报告强 Warning
- **AND** `NoMerge` MUST 保持合法

## ADDED Requirements

### Requirement: AttributeModifier Operation Map 编辑器创作

TCS 编辑器 authoring SHALL 支持在 AttributeModifier DefinitionAsset 与对应 DataTable 中编辑 Operation Map。每条 Operation MUST 能独立配置 `OperationId`、`TargetAttributeId`、Operator / Custom Operator、Evaluator 与 OperandPayload。旧单 Operation 字段 MUST NOT 继续作为创作入口。

#### Scenario: 在 DefAsset 中编辑多 Operation
- **WHEN** 开发者打开 `UTcsAttributeModifierDefinition`
- **THEN** 其可编辑字段 MUST 包含 Operation Map
- **AND** MUST NOT 再暴露旧 `AttributeId`、`ModifierMode`、`Operands` 或 `ModifierType` 作为有效创作路径

### Requirement: Operator 与 Merger 兼容规则驱动创作与验证

AttributeModifier 的 Operator / Merger 兼容规则 SHALL 以 `TcsDeveloperSettings` 为权威来源，采用二元 `Allowed` / `Forbidden`。DefinitionAsset 校验、必要的编辑器过滤与运行时防御 MUST 读取同一规则集。Custom Merger 类的默认兼容 Operator 列表可由类声明，项目设置只能收紧。

#### Scenario: 先设 Operator 后过滤 Merger 候选
- **WHEN** 开发者先为 Operation 选择 Operator
- **THEN** 编辑器 SHOULD 仅显示或仅允许选择 Allowed 的 Merger
- **AND** 即使没有下拉过滤，Data Validation 与运行时检查 MUST 仍阻止 Forbidden 组合

#### Scenario: 先设 Merger 后改 Operator 不自动清空
- **WHEN** 开发者先设置 Merger，再将 Operator 改为与其 Forbidden 的组合
- **THEN** 系统 MUST NOT 自动清空已有 Merger 或 Operator
- **AND** MUST 通过 `PostEditChangeProperty` / `IsDataValid` 报告 Error

### Requirement: 旧 AttributeModifier 创作数据不迁移

系统 MUST NOT 自动迁移旧 AttributeModifier DefinitionAsset、DataTable 行或 OperandBindings 资产。开发阶段相关验证资产 MUST 按新 Operation Map schema 重建。同步系统遇到无法直接赋值到新 Row / Def 字段的旧结构时 MUST 拒绝静默兼容。

#### Scenario: 旧 schema 不自动升级
- **WHEN** 仓库中仍存在旧 `AttributeId + ModifierMode + Operands + ModifierType` 形态的创作数据
- **THEN** 系统 MUST NOT 提供自动迁移工具或兼容读取路径
- **AND** 开发者 MUST 按新 Operation Map 重建该资产
