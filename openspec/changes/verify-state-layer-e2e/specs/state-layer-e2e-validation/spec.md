## ADDED Requirements

### Requirement: 状态层端到端竖切矩阵

项目 MUST 提供一条可在 PIE 中运行的宿主验收装置命令 `Tcs.Test.State.Run`，在**同一个状态实例**上
依次穿过 R5 全部六个能力面：内容资产驱动的定义解析、参数快照冻结、真资产修正器物化、内联触发行起行为链、
周期回调、到期回收与属性复原。该竖切 MUST 由内容资产（buff 资产 + 修正器模板资产 + 链资产）
驱动，MUST NOT 仅以运行期手搓定义充当唯一路径。装置 MUST 为每个环节输出可检索的通过/失败行，
并在常规命令中保持零非预期红字。

#### Scenario: 内容资产驱动的施加与快照冻结

- **WHEN** 在 PIE 中运行 `Tcs.Test.State.Run`
- **THEN** 内容 buff 资产的 `DefTag` 经定义库解析成功、`ApplyState` 返回 `EAR_Applied`、
  实例在册且 `CascadeAnchor` 有效，快照按 `Overrides` 与等级源分别取得预期值

#### Scenario: 真资产修正器经 LoadSynchronous 物化

- **WHEN** buff 的 `ModifierRows` 以 `TSoftObjectPtr<UTcsAttrModDef>` 引用一个真资产
- **THEN** 该资产被 `LoadSynchronous()` 取得并物化为账本修正器槽位，目标属性读数按预期变化，
  且这一路径有独立的通过/失败行（不得由瞬态模板的读数代替）

#### Scenario: 内联触发行起行为链并改属性

- **WHEN** buff 自身的内联触发行在 `TcsEvent.State.Applied` 上触发行为链
- **THEN** 行为链执行、目标属性被写入、链步骤的目标由链上 `SelectTargets` 解析
  （内容步骤 MUST NOT 依赖资产里写死的运行时实体句柄）

#### Scenario: 到期回收与级联退订

- **WHEN** 有限时值到期
- **THEN** 实例被回收、`Expired` 事件计一次、该实例的内联触发行按锚点全部退订
  （效果门面触发行计数回到施加前的读数）、修正器被剥离且属性复原到施加前的读数

#### Scenario: 可复现性由两轮逐字比对证明

- **WHEN** 在同编辑器进程内第二轮重跑 `Tcs.Test.State.Run`
- **THEN** 第二轮的全部读数摘要与第一轮逐字相同；仅"两轮皆通过"不足以证明可复现性

### Requirement: 状态层拒绝面独立取证

项目 MUST 提供独立命令 `Tcs.Test.State.Reject`，集中承载状态层的拒绝面，使常规命令得以保持零非预期红字。
拒绝面 MUST 至少覆盖：未登记的定义 `DefTag`、无效目标句柄、已回收的悬空句柄、定义库未就绪、
在无限时值定义上调时长操作。每条拒绝 MUST 断言"被拒 + 留痕级别与实测一致"，MUST NOT 把 `Log`
级留痕当作 `Warning` 级断言。

#### Scenario: 未登记定义与无效目标

- **WHEN** 以未登记的 `DefTag` 或无效目标句柄调用 `ApplyState`
- **THEN** 返回 `EAR_Rejected`、不产生实例、不广播状态事件，并留下 Warning 级记录

#### Scenario: 悬空句柄访问

- **WHEN** 对已回收实例的句柄调用查询、移除与时长操作
- **THEN** 各入口返回空值或 `false`，不触发 `ensure`，留痕级别与实现一致

#### Scenario: 无限时值定义的时长操作被拒

- **WHEN** 对 `EDP_Infinite` 定义上调 `ExtendDuration` 或 `SetRemaining`
- **THEN** 操作被拒且实例的时长字段不被修改，拒绝有独立留痕

### Requirement: 状态层证据的双面读数与边界登记纪律

证据文档 MUST 同时给出**属性面**读数（`EvaluateCurrent` / `PeekPending` 与账本修正器槽位数）与
**事件面**读数（`FTcsAttributeChangedEvent` 与六个状态事件广播计数），MUST NOT 以单面读数
推断另一面的结论。证据文档 MUST 记录装置输出冻结快照的 SHA-256 与关键日志行引用，
MUST NOT 引用持续增长的实时日志行号。

#### Scenario: 双面读数各自独立

- **WHEN** 证据文档记录竖切的每个环节
- **THEN** 属性面的通过 MUST 由属性读数支持，事件面的通过 MUST 由事件计数支持，
  不得由另一面的读数代替

#### Scenario: 引用冻结快照而非实时日志

- **WHEN** 证据文档引用日志行号或摘要
- **THEN** 引用的对象是一个已固定并计算 SHA-256 的快照文件，行号在该快照内可复现

### Requirement: 实测与未覆盖的边界清单

证据文档 MUST 附一份边界清单，逐条标注该边界是**已实测**还是**未覆盖**，
MUST NOT 以推断冒充实测。清单 MUST 至少覆盖：单机/单世界/单 PIE 进程的前提；
等级参数源仅验了哪些类型（未验的类型须显式列出）；关系表字段零消费者（转 R5.5-e）；
`Cues` / `EventPayloadFilter` / `InterruptPriority` 的占位状态；跨 PIE 世界反初始化的残留检查结论；
以及内容资产未引用切片侧类型这一纪律所带来的覆盖缺口及其理由。

#### Scenario: 未覆盖项不得隐去

- **WHEN** 某能力面在本轮未被内容资产驱动，或某参数源类型未被覆盖
- **THEN** 清单中该项标注为"未覆盖"并给出理由与承接轮次，MUST NOT 记为通过

#### Scenario: 跨 PIE 残留检查

- **WHEN** 停止 PIE 并重新进入
- **THEN** 总状态实例数与效果门面触发行计数均归零，残留检查结论记入边界清单

#### Scenario: 内容资产的类型稳定性纪律

- **WHEN** 内容资产需要引用一个类型
- **THEN** 该类型的生命周期 MUST 长于内容；切片侧类型 MUST NOT 被内容资产引用，
  由此产生的覆盖缺口 MUST 在边界清单中登记理由
