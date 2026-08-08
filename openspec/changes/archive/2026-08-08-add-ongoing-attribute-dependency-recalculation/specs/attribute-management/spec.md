## MODIFIED Requirements

### Requirement: Modifier 管线保持行为不变量

`UTcsAttributeComponent` 的 AttributeModifier 管线 SHALL 遵守 Instant / Ongoing 分层不变量：所有有效 Application 必须携带有效 SourceHandle；Instant 原子写 BaseValue 且不进入 Ongoing 存储；Ongoing 由父实例索引维护；`SourceHandleIdToModifierInstIds` 与 `ModifierInstIdToIndex` 在移除后保持一致；Attribute Change 事件只在 Clamp 与范围传播完成后报告最终稳定态；受控路径中的范围收敛通过事务内候选 `ClampCandidateAttributeValues`（或等价 candidate clamp fixpoint）完成，并在路径安全点 Flush 后发布最终稳定态。在启用依赖链惰性重算后，受控路径（Apply / BaseValue 写入 / Commit / 显式 Recalculate）若产生 Dirty Ongoing，MUST 在该路径安全点 Flush，使返回后的 CurrentValue 为最终稳定态；跨路径 Dirty MUST 延迟到帧末或等价点 Flush，且 MUST NOT 在 setter 回调内同步递归全图重算。

#### Scenario: Instant Application 原子写 BaseValue
- **WHEN** `UTcsAttributeComponent::ApplyAttributeModifier` 以 `ApplicationMode = Instant` 处理请求
- **THEN** 系统 MUST 在全部 Operation 求值成功后原子提交 BaseValue 变更
- **AND** MUST NOT 分配 Ongoing 存储项或依赖 `ModifierChangeBatchId`

#### Scenario: SourceHandle 索引在移除后保持一致
- **WHEN** `RemoveOngoingModifiersBySourceHandle(SourceHandle)` 处理 Ongoing 父实例
- **THEN** 每一个被移除的父实例 MUST 同时从 `SourceHandleIdToModifierInstIds[SourceHandle.Id]` 与 `ModifierInstIdToIndex` 中清除；不得留下任何陈旧索引项

#### Scenario: 范围约束后才报告最终稳定态
- **WHEN** 任意 Instant 或 Ongoing 变更路径完成时
- **THEN** 事务内候选 Clamp / 范围收敛 MUST 在最终 Attribute Change 事件之前完成
- **AND** 对外事件 MUST 只包含传播后的最终稳定值

#### Scenario: 受控路径安全点 Flush Dirty Ongoing
- **WHEN** SetAttributeBaseValue 或 ApplyAttributeModifier 提交后 Dirty Ongoing 集合非空
- **THEN** 该 API 返回前 MUST 完成必要的 Dirty Flush
- **AND** 调用方随后读取的 CurrentValue MUST 反映依赖收敛后的结果

## ADDED Requirements

### Requirement: AttributeComponent 拥有 Ongoing Dirty Flush 调度

`UTcsAttributeComponent` SHALL 作为 Actor 本地 Ongoing Dirty 的调度权威：维护 Dirty 集合、执行 Flush、并与既有 `RecalculateAttributeCurrentValues` / Application 事务衔接。系统 MUST NOT 将完整依赖图重算职责下放到 StateParam、Buff 或外部业务 setter。

#### Scenario: Flush 仅由 AttributeComponent 执行重算
- **WHEN** StateParam 发出依赖失效通知
- **THEN** StateParam 路径 MUST 只导致 MarkDirty（或请求帧末 Flush）
- **AND** 实际 Ongoing 重算 MUST 在 AttributeComponent 的 Flush 中执行

#### Scenario: 单一注册表与临时无贡献状态由 AttributeComponent 管理
- **WHEN** 已提交 Ongoing 在后续重算中失败并临时停止贡献
- **THEN** `UTcsAttributeComponent` MUST 成为注册表 AppliedOperations 更新、后续事务重试与最终 Current 提交的唯一权威
- **AND** StateParam、Buff 或外部业务 setter MUST NOT 直接恢复父实例贡献或写入 Current
