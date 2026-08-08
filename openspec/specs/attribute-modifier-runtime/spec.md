# attribute-modifier-runtime Specification

## Purpose
定义 AttributeModifier 的 Operation Request 运行时契约：统一 `ApplyAttributeModifier` 入口、父 Ongoing 实例、只读求值 Snapshot、自排除重算、Operator / Merger 结算，以及 Ongoing 可观察依赖的惰性失效与安全点 Flush。
## Requirements
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

