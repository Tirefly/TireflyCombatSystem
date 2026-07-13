## ADDED Requirements
### Requirement: 运行时实例身份不得再被建模为单一 InstanceId

TCS SHALL 不再把单一的 `InstanceId` 视为所有运行时对象、所有网络阶段与所有验证场景下的统一身份。系统 MUST 在设计层至少区分条目级稳定身份（如 `DefId`）、未来预测阶段身份（`PredictionKey`，仅作为显式进入本地预测 / 网络同步实现后的约束）与实例级 authority 最终身份；若未来确有本地 bookkeeping 需要，才 MAY 额外引入局部运行时辅助键。当前执行范围 MUST NOT 因此新增 `PredictionKey` 类型、字段、函数参数、RPC 或网络同步链路。

#### Scenario: 条目对象不强行发明实例级身份
- **WHEN** 运行时对象本质上是长期条目或持久持有项（例如 `UTcsSkillEntry`）
- **THEN** 它 SHOULD 继续只以 `DefId` 表达稳定身份
- **AND** 系统 MUST NOT 无理由为其补一套额外的实例级 `AuthorityId`

#### Scenario: 单实例 Attribute 不需要额外实例级身份
- **WHEN** 一个 Owner 下对同一个 `AttributeDefId` 只持有一个 `AttributeInstance`
- **THEN** 该实例的稳定身份 SHOULD 继续由 `Owner + AttributeDefId` 表达
- **AND** 系统 MUST NOT 额外强制引入 `AttrInstId` 作为网络或 authority 主身份

### Requirement: State-like 运行时实例继续保留实例级 authority 身份

对同 `DefId` 可并存多个运行时实例，且未来可能需要跨端重关联、验证或纠错的对象，系统 SHALL 保留实例级 authority 身份，并继续沿用符合语义的现有字段命名。

#### Scenario: StateInstance 需要实例级 authority 身份
- **WHEN** 系统创建 `UTcsStateInstance`
- **THEN** 它 MUST 具备实例级 authority 身份（如 `StateInstId`）
- **AND** 该身份 MUST 在 authority instance 真正创建且其他有效性验证已通过后立即分配

#### Scenario: BuffInstance 与 SkillInstance 继承 StateInstance 身份模型
- **WHEN** 系统创建 `UTcsBuffInstance` 或 `UTcsSkillInstance`
- **THEN** 它 MUST 继承 `UTcsStateInstance` 的实例级 authority identity 模型
- **AND** 系统 MUST NOT 再为这两类对象平行发明另一套独立实例身份体系

#### Scenario: SourceHandle 不替代 State-like 实例身份
- **WHEN** 系统验证 `UTcsStateInstance` / `UTcsBuffInstance` / `UTcsSkillInstance` 的 authority 身份与语义上下文
- **THEN** `StateInstId` MUST 继续承担该实例的实例级 authority 身份
- **AND** `FTcsSourceHandle` MUST 只被视为 authority 有效性验证与因果链语义上下文之一，而不是 `StateInstId` 的替代物

### Requirement: Modifier 实例现阶段不进入预测流程

`AttributeModifierInstance` 与 `SkillModifierInstance` 在当前版本 SHALL 不进入本地预测流程，但仍可保留实例级 authority 身份以支撑多实例并存与 authority 唯一验证。

#### Scenario: AttributeModifierInstance 只保留实例级 authority 身份
- **WHEN** 系统创建 `AttributeModifierInstance`
- **THEN** 它 MAY 保留实例级 authority 身份（如 `AttrModInstId`）
- **AND** 当前版本 MUST NOT 为其引入 `PredictionKey`

#### Scenario: SkillModifierInstance 只保留实例级 authority 身份
- **WHEN** 系统创建 `SkillModifierInstance`
- **THEN** 它 MAY 保留实例级 authority 身份（如 `SkillModInstId`）
- **AND** 当前版本 MUST NOT 为其引入 `PredictionKey`

#### Scenario: SourceHandle 不替代 Modifier 实例身份
- **WHEN** 系统验证 `AttributeModifierInstance` 或 `SkillModifierInstance` 的 authority 身份与语义上下文
- **THEN** `AttrModInstId` / `SkillModInstId` MUST 继续承担各自实例的实例级 authority 身份
- **AND** `FTcsSourceHandle` MUST 只被视为 authority 有效性验证与语义上下文之一，而不是这些实例级 ID 的替代物

### Requirement: SourceHandle 是 AuthorityOnly 的因果链句柄

`FTcsSourceHandle` SHALL 作为 TCS 事件因果链的唯一权威存储结构。`FTcsSourceHandle::Id` MUST 是 **AuthorityOnly** 字段；客户端本地预测阶段不得生成最终 authority `SourceHandle`。

#### Scenario: SourceHandle 只在 authority 上最终生成
- **WHEN** 运行时对象进入 authority 最终创建路径
- **THEN** 系统 MUST 在 authority 侧生成最终 `FTcsSourceHandle`
- **AND** 客户端预测阶段 MUST NOT 自行生成最终 authority `SourceHandle`

#### Scenario: SourceHandle 用于因果链与 authority 归因
- **WHEN** 系统验证一个 `FTcsSourceHandle`
- **THEN** 有效性 MUST 只以 `SourceHandle.Id` 为主键判断
- **AND** `CausalityChain`、`Instigator`、`SourceTags` MUST 被视为语义载荷而不是身份主键

### Requirement: 未来本地预测流程优先参考 GAS 的根请求预测模型

当前版本虽然不强制立即实现本地预测与网络同步代码，但后续方案 SHALL 以 GAS 风格的“根请求携带 `PredictionKey`”流程为设计基准，且 MUST NOT 为实例 reconcile 额外引入专门 RPC。在用户明确批准进入该实现前，本 requirement 仅约束未来设计方向，MUST NOT 被解释为当前代码任务或第 8 / 9 阶段前置条件。

#### Scenario: 当前阶段不添加 PredictionKey 代码
- **WHEN** 当前 change 仍处于第六阶段设计收敛或第八 / 第九阶段 DefId 主路径实现范围内
- **THEN** 系统 MUST NOT 新增 `PredictionKey` 类型、字段、函数参数、RPC 或复制字段
- **AND** 本地预测与网络同步相关内容 MUST 只作为未来显式进入第七阶段后的实现约束

#### Scenario: 未来预测阶段以 PredictionKey 作为主身份键
- **WHEN** 用户已明确批准进入本地预测实现且客户端本地预测创建运行时对象
- **THEN** 系统 MUST 使用根请求携带的 `PredictionKey` 作为预测阶段主身份键
- **AND** 系统 MUST NOT 默认再为每个预测实例发明一个全局 `LocalInstanceId`

#### Scenario: 当前代码范围默认不引入 PredictionKey
- **WHEN** 当前版本处理运行时实例身份设计且用户尚未明确批准进入本地预测实现
- **THEN** 系统 SHOULD 默认不为其引入 `PredictionKey`
- **AND** 只有后续真正纳入本地预测主路径的对象，才需要进入 `PredictionKey` 语义模型
- **AND** 当前已知只有未来 `SkillInstance` 预测路径可能进入该模型

#### Scenario: 未来单个 PredictionKey 不会产生多个同类预测实例
- **WHEN** 用户已明确批准进入本地预测实现且系统处理同一个根请求的本地预测实例创建
- **THEN** 同一个 `PredictionKey` MUST NOT 产生多个同类预测实例
- **AND** 因此当前版本不需要为预测阶段额外引入 prediction-scope 局部序号来区分同类实例

#### Scenario: 未来 PredictionKey 到实例级 authority 身份的确认映射
- **WHEN** 用户已明确批准进入本地预测 / 网络同步实现，且 authority 接受根请求并成功创建最终实例
- **THEN** authority MUST 在实例创建且其他有效性验证已通过后立即分配对应的实例级 authority 身份
- **AND** 客户端 MUST 通过既有确认/复制链路完成 `PredictionKey -> 实例级 authority 身份` 的 reconcile，而不是通过专门的实例级 RPC 完成

### Requirement: 第五阶段的 Component static ID 工厂只作为过渡实现

在本次 change 的第五阶段，出于先清理空壳 manager 的需要，`StateInstanceId`、`ModifierInstId`、`ModifierChangeBatchId` 与 `SourceHandle.Id` 的部分分配逻辑 MAY 临时下沉到 `Component static`。该做法 SHALL 被明确标注为过渡方案，而非最终身份架构。

#### Scenario: 过渡期 static 工厂不等于最终身份系统
- **WHEN** 代码在第五阶段通过 `Component static` 自增计数器分配相关运行时 ID
- **THEN** 这些计数器 MUST 只被解释为过渡期实现
- **AND** 它们 MUST NOT 被当作 authority 身份系统已经收敛完成的事实

### Requirement: 当前版本先固化设计，不强制立即落地预测与网络同步实现

当前版本 SHALL 先把上述身份语义与预测/authority 兼容约束写硬到 spec。后续是否实际落地本地预测与网络同步实现，由用户明确批准后的后续阶段单独推进。

#### Scenario: 设计先于实现落地
- **WHEN** 当前版本尚未把本地预测或网络同步纳入必须交付范围
- **THEN** 系统 MAY 只先完成身份语义、预测兼容性与 authority 约束的规格收敛
- **AND** 不得因为当前尚未实现预测代码，就回退到继续混用单一 `InstanceId` 语义
- **AND** 不得在未获用户明确批准进入预测/同步实现前新增 `PredictionKey` 相关代码
