# runtime-network-identity Specification

## Purpose

为未来 TCS 网络同步与本地预测提供运行时身份、authority 因果链和 reconcile 的设计边界；本规格不授权当前实现网络代码。

## Requirements
### Requirement: 运行时身份按职责分层

TCS SHALL 不把单一 `InstanceId` 视为定义资产、长期条目、可并存运行时实例、事件因果链和未来预测阶段的统一身份。系统 MUST 在设计层区分 `DefId`、实例级 authority 身份、`FTcsSourceHandle` 与未来 `PredictionKey` 的职责。

#### Scenario: 条目对象不强行发明实例级身份
- **WHEN** 运行时对象是长期条目或持有项，例如 `UTcsSkillEntry` 或单 Owner 下的 `AttributeInstance`
- **THEN** 系统 MUST 分别使用 `SkillDefId` 或 `Owner + AttributeDefId` 表达稳定身份
- **AND** 系统 MUST NOT 无理由为其新增独立实例级 authority ID

#### Scenario: 可并存实例保留语义化 authority 身份
- **WHEN** 系统创建可与同 DefId 对象并存的 `StateInstance`、`BuffInstance`、`SkillInstance`、`AttributeModifierInstance` 或 `SkillModifierInstance`
- **THEN** 系统 MUST 分别使用 `StateInstId`、`AttrModInstId` 或 `SkillModInstId` 表达实例级 authority 身份
- **AND** 系统 MUST NOT 仅为泛化而将这些字段重命名为 `AuthorityId`

### Requirement: SourceHandle 只承担 authority 因果链职责

`FTcsSourceHandle` SHALL 是 TCS 事件因果链的唯一 authority 存储结构。`FTcsSourceHandle::Id` MUST 是 AuthorityOnly 字段，且 `SourceHandle` MUST NOT 替代实例级 authority 身份。

#### Scenario: SourceHandle 不替代实例级 ID
- **WHEN** 系统验证一个 State-like 或 Modifier 运行时实例
- **THEN** 系统 MUST 继续使用对应的 `StateInstId`、`AttrModInstId` 或 `SkillModInstId` 识别实例
- **AND** `FTcsSourceHandle` MUST 仅作为因果归因与 authority 有效性验证上下文

#### Scenario: 预测阶段不生成最终 SourceHandle
- **WHEN** 未来本地预测路径创建临时运行时对象
- **THEN** 客户端 MUST NOT 自行生成最终 authority `FTcsSourceHandle`
- **AND** 最终 `SourceHandle` MUST 由 authority 最终创建路径生成

### Requirement: 未来预测遵循根请求 PredictionKey 模型

未来明确进入本地预测实现后，TCS SHALL 以 GAS 风格的“根请求携带 `PredictionKey`”作为预测阶段身份模型。系统 MUST NOT 为实例 reconcile 新增专门 RPC。

#### Scenario: 预测只在获批实现 change 中进入
- **WHEN** 当前 change 尚未明确批准本地预测或网络同步实现
- **THEN** 系统 MUST NOT 新增 `PredictionKey` 类型、字段、函数参数、RPC、复制字段或同步链路
- **AND** 本 capability MUST 只作为后续网络实现的设计约束

#### Scenario: PredictionKey 关联最终实例身份
- **WHEN** 后续获批的预测实现中 authority 接受根请求并成功创建最终实例
- **THEN** authority MUST 在其他有效性验证通过后立即分配对应实例级 authority 身份
- **AND** 客户端 MUST 通过既有根请求确认或复制链路将 `PredictionKey` 关联到该最终身份

#### Scenario: 一个 PredictionKey 不产生多个同类预测实例
- **WHEN** 后续获批的预测实现处理同一个根请求
- **THEN** 同一个 `PredictionKey` MUST NOT 产生多个同类预测实例
- **AND** 系统 MUST NOT 预先为此添加全局 `LocalInstanceId` 或 prediction-scope 局部序号

### Requirement: 网络身份实现必须单独获批

本 capability 只定义未来网络身份边界，不构成当前网络功能实现授权。任何本地预测、网络同步、authority allocator、复制载荷或 reconcile 代码 SHALL 由单独获批的后续 change 实现。

#### Scenario: 后续实现明确技术决策
- **WHEN** 创建网络身份相关的实现 change
- **THEN** 该 change MUST 明确 authority allocator、复制载荷、确认与拒绝、回滚和失败恢复策略
- **AND** 该 change MUST 说明对现有 `Component static` ID 工厂的迁移或保留路径
