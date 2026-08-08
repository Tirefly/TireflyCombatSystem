## ADDED Requirements

### Requirement: Ongoing 依赖键与自动收集

系统 SHALL 为可观察依赖定义 `DependencyKey`，至少覆盖：目标 `UTcsAttributeComponent` 内 Attribute 的 CurrentValue，以及目标本地 BuffInstance 上 Numeric StateParam 的 effective 值。Ongoing 父实例重算时，Evaluator MUST 仅通过只读 Context / Snapshot 读取这些值，并 MUST 自动将读取到的 `DependencyKey` 记录到该父实例的依赖集合。系统 MUST NOT 要求 Custom Evaluator 另行维护唯一依赖声明列表作为正确性前提。依赖记录精度 MUST 固定到父 Ongoing `ModifierInstId`，MUST NOT 以单条 Operation 作为独立 Dirty 粒度。

#### Scenario: 读取 Attribute Current 时登记依赖
- **WHEN** 某 Ongoing 父实例重算时 Evaluator 经 Context 读取目标 Component 上 `AbilityPower` 的 CurrentValue
- **THEN** 该父实例的依赖记录 MUST 包含对应 Attribute Current 的 DependencyKey
- **AND** 反向依赖索引 MUST 能从该 Key 找回该 `ModifierInstId`

#### Scenario: 读取 StateParam effective 时登记依赖
- **WHEN** StateParam Operand Evaluator 读取本地 Buff 上某 Numeric Param 的 `GetModifiedValue()`
- **THEN** 该父实例的依赖记录 MUST 包含对应 StateParam effective 的 DependencyKey

#### Scenario: 多 Operation 共享父实例依赖粒度
- **WHEN** 同一父实例含多条 Operation 且各自读取不同依赖
- **THEN** 任一依赖变化 MUST 使整个父实例 Dirty
- **AND** Flush MUST 原子重算该父实例全部 Operation

### Requirement: 依赖变化标脏与 Revision

当已登记依赖的生产者成功提交**真实值变化**时，系统 MUST 递增该依赖的 `DependencyRevision`，并 MUST 将反向索引中的相关父 Ongoing 实例加入 Dirty 集合。MarkDirty MUST NOT 在依赖生产者路径内同步递归执行完整 Ongoing 重算图。

#### Scenario: Attribute Current 变化只标脏
- **WHEN** 目标 Component 上某 Attribute 的 CurrentValue 因合法路径提交而变化
- **AND** 存在 Ongoing 父实例依赖该 CurrentValue
- **THEN** 系统 MUST 将这些父实例标 Dirty
- **AND** MUST NOT 在 CurrentValue 赋值回调内同步完成全部 Dirty 父实例的重算（除非当前正处于受控路径安全点 Flush）

#### Scenario: 无真实变化不标脏
- **WHEN** 生产者提交的值与上次已提交值近乎相等（无业务可见变化）
- **THEN** 系统 MUST NOT 无故递增 Revision 或扩大 Dirty 集合

### Requirement: Flush 策略 C（路径末同步 + 帧末兜底）

`UTcsAttributeComponent` SHALL 提供统一的 Dirty Ongoing Flush 入口。Flush MUST：合并 Dirty、按需扩展依赖闭包、使用自排除 Snapshot、按稳定序/拓扑序重算受影响父实例、执行 Clamp 与范围传播，并只广播最终稳定态事件。

调度 MUST 遵守：
1. **受控路径安全点**（含 `ApplyAttributeModifier`、`SetAttributeBaseValue`、事务 Commit、显式全量/局部 Recalculate 等）：若 Dirty 非空，MUST 在该路径返回前同步 Flush。
2. **跨调用栈 Dirty**（例如 StateParam 失效通知）：MUST 合并到帧末或等价延迟任务再 Flush。
3. MUST NOT 在 Attribute / StateParam 的 setter 或对外广播回调中同步递归进入完整依赖图重算。

#### Scenario: Apply 路径末尾 Flush
- **WHEN** Instant 或 Ongoing Application 提交导致 Dirty 非空
- **THEN** `ApplyAttributeModifier` 返回前 MUST 已完成必要 Flush
- **AND** 调用方读到的 CurrentValue MUST 为收敛后的稳定态

#### Scenario: 同帧多次依赖变化合并
- **WHEN** 同一帧内同一父实例的多个依赖先后变化
- **THEN** 系统 MUST 将多次 MarkDirty 合并
- **AND** Flush MUST 对该父实例只执行一次完整原子重算（或等价一次提交）

#### Scenario: 跨路径 Dirty 帧末收敛
- **WHEN** StateParam effective 变化仅 MarkDirty 且当前调用栈不在 Attribute 受控路径内
- **THEN** 系统 MUST 在帧末或等价延迟点 Flush
- **AND** MUST NOT 要求调用方再手动调用全量 Recalculate 才能收敛自动观察的依赖

### Requirement: 循环依赖检测

系统 MUST 检测不同父 Ongoing 实例之间因自动观察依赖形成的有向环。Initial Apply 的完整候选图形成循环时，本次 Application MUST 零提交。已注册父实例在后续 Attribute 事务中形成循环时，系统 MUST 记录包含 `ModifierInstId`、相关 `OperationId` 与依赖链的 Error，MUST 临时跳过最小循环 SCC 的全部父实例，并 MUST 从其余有效贡献重建 Current。系统 MUST NOT 使用“最多 N 次迭代”作为默认收敛策略。

#### Scenario: 已注册双父实例环临时停止贡献
- **WHEN** 已注册父实例 A 依赖 Attribute X 写出 Y，已注册父实例 B 依赖 Y 写出 X，形成环
- **THEN** Flush MUST 检测最小循环 SCC 并记录 Error
- **AND** A 与 B 的候选 AppliedOperations MUST 清空且旧贡献 MUST 立即停止
- **AND** SCC 下游父实例 MUST 基于剩余有效 Current 继续重算

#### Scenario: Initial Apply 引入循环时零提交
- **WHEN** 新 Ongoing Application 候选与已注册父实例形成循环
- **THEN** 本次 Application MUST 失败并零提交
- **AND** 新父实例 MUST NOT 保存为空贡献注册项
- **AND** 已注册父实例与 Current MUST 保持 Application 前状态

#### Scenario: Source 清理新暴露的循环临时停止贡献
- **WHEN** SourceHandle 清理已使待移除父实例失效
- **AND** 剩余父实例形成新的循环 SCC，无法产生不含已移除来源的稳定 Current
- **THEN** 系统 MUST 清空该 SCC 全部父实例的候选 AppliedOperations
- **AND** MUST 使用其余有效父实例完成 Source 清理事务
- **AND** State removal MUST 继续执行后续步骤

### Requirement: 未观察依赖的显式重算请求

对首版不自动观察的依赖（跨 Actor Attribute、非目标 Component、装备或其他业务 UObject 字段），系统 SHALL 提供显式 API（如 `RequestOngoingModifierRecalculation(SourceHandle)`）：将匹配 `SourceHandle` 的 Ongoing 父实例加入 Dirty，仍由 Flush 策略执行实际重算。该 API MUST NOT 在调用栈内强制同步完成跨 Component 全局图遍历。

#### Scenario: 显式按 SourceHandle 标脏
- **WHEN** 业务在未观察依赖变化后调用显式重算请求并传入有效 SourceHandle
- **THEN** 匹配该 SourceHandle 的 Ongoing 父实例 MUST 进入 Dirty 集合
- **AND** 实际重算 MUST 仍由后续 Flush 完成

#### Scenario: 显式请求触发空贡献父实例重算
- **WHEN** 业务传入某已注册父实例的有效 SourceHandle
- **AND** 该父实例的 AppliedOperations 当前为空
- **THEN** 后续 Flush MUST 作为 Attribute 事务重新尝试全部空贡献父实例
- **AND** 重算成功前 MUST NOT 恢复其旧数值贡献

### Requirement: 移除 Ongoing 时清理依赖索引

当 Ongoing 父实例被移除或 SourceHandle 清理成功时，系统 MUST 删除其依赖记录，并从反向索引与 Dirty 集合中移除对应 `ModifierInstId`，避免悬空引用。

#### Scenario: RemoveBySourceHandle 清理索引
- **WHEN** `RemoveOngoingModifiersBySourceHandle` 成功移除若干父实例
- **THEN** 这些实例的 DependencyKey 反向映射 MUST 被清除
- **AND** Dirty 集合 MUST NOT 再包含已移除的 ModifierInstId

#### Scenario: Source 清理覆盖空贡献父实例
- **WHEN** `RemoveOngoingModifiersBySourceHandle` 清理的 SourceHandle 拥有 AppliedOperations 为空的已注册父实例
- **THEN** 该父实例 MUST 从唯一运行时注册表、依赖索引与 Dirty 集合删除

### Requirement: 已提交 Ongoing 重算失败时临时跳过无效贡献

已成功提交的 Ongoing 父实例在后续重算中发生 Evaluator、Operator、Merger 或 Current Clamp / Range 传播失败时，系统 SHALL 记录 Error，并 SHALL 在唯一 Ongoing 注册表中保留其稳定身份、来源、Owner、Overrides 与上次成功依赖记录，同时清空本轮候选 AppliedOperations。空贡献父实例 MUST NOT 参与 Snapshot、Merger、Operator 重放或 CurrentValue 聚合。最终 CurrentValue MUST 只由 Base 与本轮有效父实例推导。系统 MUST NOT 新增 Quarantined、Disabled 或 Retry 容器。Initial Apply 失败 MUST 继续保持整次 Application 零提交，MUST NOT 保存空贡献注册项。任意后续 Attribute 事务 MUST 重新尝试全部空贡献父实例。

#### Scenario: 单一来源 Operator 失败临时跳过整个父实例
- **WHEN** 已提交父实例 B 的任一 Operation 在新 Snapshot 上返回 Operator 失败
- **AND** 该合并 Operation 可唯一追踪到 B
- **THEN** B 的全部候选 AppliedOperations MUST 作为一个父实例整体清空
- **AND** B 的上次成功结果 MUST 立即停止贡献 CurrentValue
- **AND** 其他有效父实例 MUST 继续参与本轮计算

#### Scenario: 多来源 Operator 或 Merger 失败临时跳过不可分割组
- **WHEN** 某 `ModifierDefId` 的 Merger 无法处理当前父实例组，或 Operator 失败结果由多个父实例共同合并产生
- **THEN** 该 `ModifierDefId` 组的全部父实例候选 AppliedOperations MUST 一并清空
- **AND** 系统 MUST NOT 猜测单个责任父实例

#### Scenario: 后续 Attribute 事务恢复空贡献父实例
- **WHEN** 任意后续 Attribute 事务重新尝试 AppliedOperations 为空的父实例
- **AND** 它在不包含自身旧贡献的 Snapshot 上成功重算
- **THEN** 系统 MUST 原子写回其新 AppliedOperations
- **AND** MUST 在同一事务中重算其下游闭包并只广播最终稳定事件

#### Scenario: 后续 Attribute 事务再次失败
- **WHEN** 空贡献父实例在后续 Attribute 事务中再次重算失败
- **THEN** 它的 AppliedOperations MUST 保持为空且不贡献 CurrentValue
- **AND** 系统 MUST 再次记录 Error
- **AND** 系统 MUST NOT 广播 Attribute 中间变化

#### Scenario: Base 与 Range 配置无法产生合法值
- **WHEN** 不包含任何 Modifier 贡献的 Base 候选仍无法通过 Clamp 或 Range 配置得到合法值
- **THEN** 系统 MUST 在所有构建中以 Fatal 终止
- **AND** MUST NOT 通过临时跳过任意 Modifier 掩盖该配置不变量错误
