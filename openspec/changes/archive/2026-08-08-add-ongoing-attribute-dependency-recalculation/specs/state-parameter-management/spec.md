## ADDED Requirements

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
