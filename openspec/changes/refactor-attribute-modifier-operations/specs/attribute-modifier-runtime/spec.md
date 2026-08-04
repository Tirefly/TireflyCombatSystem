## Purpose

定义 AttributeModifier 的 Operation 运行时契约：`OperandPayload -> OperandEvaluator -> EvaluatedOperand -> Operator` 模型、唯一入口 `ApplyAttributeModifier(Request, OutResult)`、Instant BaseValue 原子结算、Ongoing CurrentValue 聚合、Snapshot 自排除、StateInstance 宿主边界，以及 Operator/Merger 兼容规则。

## REMOVED Requirements

### Requirement: Operand 动态绑定
**Reason**: OperandBindings / `FTcsStateParamBinding` 被 `FTcsStateParamOperandPayload` + Numeric Evaluator 取代。
**Migration**: 在 Operation 的 OperandPayload 中声明 StateParamTag 与来源类型；Evaluator 从 EvaluatorContext 读取 effective `GetModifiedValue()`。

### Requirement: RecalculateAttributeCurrentValues 中刷新 Operand
**Reason**: 旧“先刷新 Operands Map 再 Execution”模型被 Snapshot + Evaluator 按 Operation 求值取代。
**Migration**: Ongoing 受控重算入口构建排除自身后的 Snapshot，再按 OperationId 稳定排序求值；动态依赖自动标脏留给后续 change。

### Requirement: ResolveStateParamInstances
**Reason**: 旧容器定位 helper 绑定到 OperandBindings 刷新路径。
**Migration**: EvaluatorContext 直接携带可选 SourceStateInstance / SourceSkillEntry；StateParam Evaluator 从 Context 解析 Numeric Param。

### Requirement: CreateAttributeModifierWithBindings
**Reason**: 创建与 Apply 分离的旧入口删除；Bindings 模型删除。
**Migration**: 使用 `ApplyAttributeModifier(Request, OutResult)`，Request 携带 ApplicationMode、SourceHandle 与可选 OperationOverrides。

### Requirement: 删除 CreateAttributeModifierWithOperands
**Reason**: 该“删除旧 API”要求本身已成为历史；本 change 删除全部旧 Create/Apply 入口族。
**Migration**: 统一迁移到 `ApplyAttributeModifier`。

### Requirement: AttributeModifierInstance 新增引用字段
**Reason**: SourceStateInstance / SourceSkillEntry 不再作为 ModifierInstance 上为 OperandBindings 服务的持久定位字段契约。
**Migration**: 它们只作为每轮 EvaluatorContext 的可选输入；由 StateInstance / Skill 宿主在 Apply 时提供，不通过 SourceHandle 反查。

### Requirement: AttributeModifier 已解析 Operand 的统一访问口径
**Reason**: `FTcsAttributeModifierInstance::Operands` 不再是权威已解析值存储。
**Migration**: 业务与调试读取 ApplicationResult、Ongoing 已应用 Operation 记录或最终 Attribute CurrentValue；不再依赖 Operands Map。

## ADDED Requirements

### Requirement: AttributeOperation 模型与 Operation Map

AttributeModifier Definition SHALL 使用 `TMap<FName, FTcsAttributeOperationSpec>` 表达多 Operation，Key 为稳定 `OperationId`。每条 Operation MUST 声明 `TargetAttributeId`、Operator（内建或 Custom）、OperandEvaluator 与 OperandPayload。AttributeModifier 的 Evaluator 与 Operand MUST 固定为 Numeric / `float`。`FInstancedStruct` MUST 只承载 OperandPayload。Definition MUST NOT 暴露 `ModifierMode`、单目标 `AttributeId`、旧 `Operands` Map 或 `ModifierType`。

#### Scenario: Definition 持有多个稳定 Operation
- **WHEN** 设计者在 AttributeModifierDef 中创建 `op_health` 与 `op_mana` 两条 Operation
- **THEN** 它们 MUST 以 `OperationId` 为 Key 保存在 Operation Map 中
- **AND** 各自可指向不同 `TargetAttributeId`
- **AND** Def 级 MUST NOT 再出现单一 `AttributeId` 或 `ModifierMode`

#### Scenario: Application 覆写边界
- **WHEN** 调用方通过 Request 提供 OperationOverrides
- **THEN** 系统 MUST 只允许覆写 Evaluator 与 OperandPayload
- **AND** MUST NOT 允许覆写 Operator、目标 Attribute、Priority 或 Merger

### Requirement: 唯一入口 ApplyAttributeModifier

`UTcsAttributeComponent` SHALL 提供唯一公开 Application 入口 `ApplyAttributeModifier(const FTcsAttributeModifierApplicationRequest&, FTcsAttributeModifierApplicationResult&)`。所有有效请求 MUST 携带有效 `SourceHandle`，包括 Instant。同一次调用的全部 Operation MUST 使用同一个 `ApplicationMode`。任一 Evaluator、Operator、目标或上下文失败时，系统 MUST 零提交。

#### Scenario: Instant 原子写入 BaseValue
- **WHEN** `ApplicationMode = Instant` 且全部 Operation 求值成功
- **THEN** 系统 MUST 按 `OperationId` 稳定顺序原子写入各目标 Attribute 的 BaseValue
- **AND** MUST NOT 将实例写入 Ongoing 存储
- **AND** MUST NOT 经过 Merger

#### Scenario: Ongoing 创建可撤销父实例
- **WHEN** `ApplicationMode = Ongoing` 且请求合法
- **THEN** 系统 MUST 创建或拒绝创建父 Ongoing 实例，并让其 Operation 贡献参与 CurrentValue 聚合
- **AND** 成功后可用 `RemoveOngoingModifiersBySourceHandle` 撤销

#### Scenario: 无效 SourceHandle 零修改
- **WHEN** Request 的 SourceHandle 无效
- **THEN** `ApplyAttributeModifier` MUST 返回失败
- **AND** Attribute、Ongoing 存储与事件 MUST 保持不变

#### Scenario: 任一 Operation 失败零提交
- **WHEN** 多 Operation Application 中任一 Evaluator 或 Operator 失败
- **THEN** 系统 MUST 不写入任何 BaseValue 变更
- **AND** MUST NOT 创建或更新任何 Ongoing 父实例

### Requirement: AttributeEvaluationSnapshot 与自排除

系统 SHALL 为每次 Application / Ongoing 重算构建不可变 `AttributeEvaluationSnapshot`。Snapshot MUST 只提供 TargetAttributeComponent 内 Attribute 的 BaseValue / CurrentValue，MUST NOT 隐式读取 Instigator、其他 Actor 或其他 Component。Ongoing 重算时，系统 MUST 虚拟排除当前父实例的全部旧结果并重建 Snapshot；该父实例的全部 Operation MUST 共用这份 Snapshot。Snapshot MUST NOT 持久化到 ModifierInstance。

#### Scenario: Snapshot 作用域仅限目标组件
- **WHEN** Evaluator 从 Snapshot 查询 Attribute 值
- **THEN** 它 MUST 只能读到 TargetAttributeComponent 内的 Attribute
- **AND** MUST NOT 通过 Snapshot 隐式读取 Instigator 的 Attribute

#### Scenario: Ongoing 重算排除自身旧贡献
- **WHEN** 某个 Ongoing 父实例重算其动态 Operand
- **THEN** Snapshot MUST 在排除该父实例全部旧结果后构建
- **AND** 该父实例的所有 Operation MUST 看到同一份排除后的 Snapshot

### Requirement: OperationId 稳定排序

所有影响结算结果、调试输出或未来网络确定性的 Operation 遍历 SHALL 按 `OperationId` 稳定排序。系统 MUST NOT 依赖 `TMap` 的未定义遍历顺序。

#### Scenario: 多 Operation 结算顺序稳定
- **WHEN** 同一 Application 含多个 Operation
- **THEN** 求值与提交顺序 MUST 由 `OperationId` 稳定排序决定
- **AND** 重复执行同一 Request MUST 产生相同顺序

### Requirement: ApplicationResult 逐 Operation 审计

`FTcsAttributeModifierApplicationResult` SHALL 提供逐 Operation 的成功状态、`TargetAttributeId`、`OldValue`、`NewValue`、`SourceHandle`、`OperationId` 与失败原因。Instant 成功结算 MUST 产生 Attribute BaseValue Change 事件，但 MUST NOT 产生 AttributeModifier Added / Updated / Removed 事件。Ongoing Attribute Change Event MUST 只报告 Clamp 与范围传播完成后的最终稳定态。

#### Scenario: Instant 返回逐 Operation 结果且无 Modifier 生命周期事件
- **WHEN** Instant Application 成功修改两个目标 Attribute
- **THEN** ApplicationResult MUST 包含两条 Operation 成功记录及其 Old/New 值
- **AND** 系统 MUST 广播对应 BaseValue Change 事件
- **AND** MUST NOT 广播 Modifier Added / Updated / Removed

#### Scenario: Ongoing 事件只报告最终稳定态
- **WHEN** Ongoing Apply 或移除触发重算与范围传播
- **THEN** Attribute Change Event MUST 只包含传播收敛后的最终值
- **AND** MUST NOT 广播中间传播状态作为最终业务事件

### Requirement: Ongoing 必须经由 StateInstance 持有

所有 Ongoing AttributeModifier SHALL 由 StateInstance 持有与施加。系统 MUST NOT 允许任何来源直接向 TargetAttributeComponent 施加 Ongoing。同一 StateInstance 对同一 `ModifierDefId` 最多一次；重复 Apply MUST 硬拒绝、零修改，并在 Development / Editor 输出 Warning。不同 StateInstance 施加同一 `ModifierDefId` 允许并存，并由 Merger 处理叠加。SkillInstance MUST NOT 直接施加 Ongoing；它只能施加 Instant，或通过目标本地 StateInstance 间接产生 Ongoing。Instant MAY 直接作用于 TargetAttributeComponent，包括跨 Actor。

#### Scenario: 无 StateInstance 的 Ongoing 被拒绝
- **WHEN** 调用方尝试以 Ongoing 模式直接 Apply 到 AttributeComponent 且未提供合法 StateInstance 宿主上下文
- **THEN** 系统 MUST 拒绝该请求并保持零修改

#### Scenario: 同一 StateInstance 重复同 DefId 硬拒绝
- **WHEN** 某个 StateInstance 已持有 ModifierDefId=`X` 的 Ongoing
- **AND** 再次以同一 StateInstance 与 DefId=`X` 请求 Ongoing
- **THEN** 系统 MUST 硬拒绝、零修改
- **AND** Development / Editor MUST 输出包含 StateInstance、SourceHandle 与 ModifierDefId 的 Warning

#### Scenario: SkillInstance 不能直接 Ongoing
- **WHEN** SkillInstance 尝试直接 Apply Ongoing AttributeModifier
- **THEN** 系统 MUST 硬拒绝并零修改

#### Scenario: Instant 可直接跨 Actor
- **WHEN** 伤害或技能结算以 Instant 模式直接作用于目标 AttributeComponent
- **THEN** 系统 MUST 在 SourceHandle 有效时允许该路径
- **AND** MUST NOT 要求目标本地 StateInstance

### Requirement: StateParam Operand 使用 Payload 与 Evaluator

系统 MUST 删除 `TArray<FTcsStateParamBinding>` / OperandBindings。StateParam Operand SHALL 使用 `FTcsStateParamOperandPayload` 与专用 Numeric Evaluator：Payload 指定 StateParamTag 与来源类型，Evaluator 从 Context 读取对应 Numeric StateParam 的无参 `GetModifiedValue()`。缺来源、缺 Param 或类型不匹配时，Application MUST 原子失败。

#### Scenario: StateParam Operand 读取 effective 值
- **WHEN** Operation 的 Payload 绑定到带 SkillModifier 链的 Numeric StateParam
- **THEN** Evaluator MUST 写入 effective `GetModifiedValue()`
- **AND** MUST NOT 仅读取 base `GetBaseValue()`

#### Scenario: 缺失来源导致原子失败
- **WHEN** Payload 要求 SourceSkillEntry 但 Context 中该引用为空
- **THEN** 本次 Application MUST 失败并零提交
- **AND** 系统 MUST NOT 通过 SourceHandle 隐式反查业务对象

### Requirement: Ongoing Merger 处理 EvaluatedOperand

Merger SHALL 只用于 Ongoing。内建 Merger MUST 在 Operator 之前处理本轮 EvaluatedOperand，MUST NOT 以最终 Attribute 贡献或上一轮缓存贡献作为合并输入。多 Operation 使用 `NoMerge` 始终合法；内建选择 / 聚合 Merger MUST NOT 猜测多 Operation 的整组或逐 Operation 语义。

#### Scenario: UseMaximum 比较 Operand 而非最终贡献
- **WHEN** 多个 Ongoing 实例使用 `Add` Operator 与 `UseMaximum` Merger
- **THEN** Merger MUST 比较各自 EvaluatedOperand 并选择最大 Operand
- **AND** MUST NOT 比较 Operator 执行后的最终 Attribute 增量

#### Scenario: MultiplyAdditive 与 UseAdditiveSum 使用 delta 语义
- **WHEN** 设置允许 `MultiplyAdditive + UseAdditiveSum`，且 Operand 为 `0.2` 与 `0.3`
- **THEN** Merger MUST 聚合为 `0.5`
- **AND** Operator MUST 计算为 `Current * (1 + 0.5)`

### Requirement: Operator 与 Merger 兼容规则

`TcsDeveloperSettings` SHALL 作为 Operator / Merger 兼容规则的权威来源，采用二元 `Allowed` / `Forbidden` 判定，不引入 `AllowedWithWarning`。规则 MUST 同时支持内建 Operator 枚举与 Custom Operator Class。Custom Merger 类 SHALL 声明默认兼容 Operator 列表；项目设置只能收紧该列表，不能放宽到类未声明的 Operator。`Override + UseMaximum / UseMinimum / UseAdditiveSum` MUST 为 Forbidden。多 Operation 使用内建选择 / 聚合 Merger 时，Data Validation 默认 Error，设置可降级为强 Warning；`NoMerge` 始终合法。Apply / Recalculate MUST 再次执行运行时兼容检查。

#### Scenario: Forbidden 组合在运行时被拒绝
- **WHEN** Ongoing Application 的 Operator/Merger 组合在设置中为 Forbidden
- **THEN** 系统 MUST 拒绝该 Application 并零修改

#### Scenario: 多 Operation 与 UseMaximum 默认校验失败
- **WHEN** AttributeModifierDef 含多个 Operation 且 Merger 为 `UseMaximum`
- **THEN** Data Validation MUST 默认报告 Error
- **AND** 若项目设置将该组合降级，则 MAY 报告强 Warning 而非 Error
- **AND** `NoMerge` MUST 不触发该验证

### Requirement: 删除旧 AttributeModifier 创建绑定与 Target Task

系统 MUST 删除旧 AttributeModifier 创建、绑定与 Apply API：`CreateAttributeModifier`、`CreateAttributeModifierWithBindings`、`ApplyModifier`、`ApplyModifierWithSourceHandle`，以及 `TcsSTTask_ApplyAttributeModifierToTarget`。MUST NOT 保留兼容包装器。Owner 侧 StateTree Task MUST 迁移到 `ApplyAttributeModifier` Request 模型，且不向编辑器暴露来源对象字段。

#### Scenario: 旧 Create/Apply API 不可用
- **WHEN** 编译引用 `CreateAttributeModifierWithBindings` 或 `ApplyModifierWithSourceHandle` 的代码
- **THEN** 编译 MUST 失败，并要求迁移到 `ApplyAttributeModifier`

#### Scenario: Target StateTree Task 删除
- **WHEN** 搜索 TCS runtime / editor 源码中的 `TcsSTTask_ApplyAttributeModifierToTarget`
- **THEN** 搜索结果 MUST 为空
- **AND** MUST NOT 存在将其限制为 Instant 的兼容入口
