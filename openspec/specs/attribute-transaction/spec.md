# attribute-transaction Specification

## Purpose
TBD - created by archiving change add-tcsattribute-pipeline-and-transaction. Update Purpose after archive.
## Requirements
### Requirement: 变更批与提交

管线 MUST 提供事务式写入（D2-5）：`BeginBatch(Unit)` / `Commit(Unit)`，同一批内完成的全部变更（挂修正器 / 摘修正器 / 改基础值 / **属性增删**）MUST 汇总为**一次重算 + 每属性最多一次广播**：

- **批内**：变更只标脏、只累加候选集（求值 → 折叠 → 值域收口全在内存候选值上做），不写回、不广播；
- **`Commit` 尾**：对全部脏属性逐个重算（含依赖顺延）并按 epsilon 比较后广播——**单帧多次变更只算一次**；
- 未显式开批时，各写入口 MUST 各自视为一个**隐式批**（行为与"开批-操作-提交"一致）；
- 批 MUST 支持嵌套计数（内层提交不触发 flush，最外层提交才触发）；
- **每单位事务状态与依赖边住 `FTcsAttributeStore`**（批深度 `BatchDepth` / 依赖边 `Dependents`）——与属性实例、冻结暂存区同生命周期，`UnregisterUnit` / `Deinitialize` 一并释放（无跨单位残留）；
- 写操作类 MUST 覆盖：挂修正器 / 按来源摘除 / **改基值**（`SetBaseValue(Unit, Attribute, NewBaseValue)`）/ 属性增删（`AddAttribute` / `RemoveAttribute`）。

#### Scenario: 批内多次改同属性只广播一次

- **WHEN** 一个批内对同一属性挂两个修正器（100 → 120 → 130），然后 `Commit`
- **THEN** 该属性只重算一次、只广播一次（`OldValue=100`、`NewValue=130`）

#### Scenario: 未开批即隐式批

- **WHEN** 单次调用挂一个修正器（不开批）
- **THEN** 行为与"开批 → 挂 → 提交"一致（立即生效并广播一次）

### Requirement: 唯一提交点与失败零写入

事务 MUST 只有**一个提交点**：候选集全部算完后一次写回；期间任何一步失败（含环检测拒绝、非法参数）MUST 使该次提交**零写入**——不得出现"改了一半"的中间态（D2-5）：

- 写回范围 MUST 限于本次批涉及的属性（不做全量重写）；
- 提交失败时 MUST 保持批前状态（脏标记与缓存值回到批前语义）。

#### Scenario: 提交失败零写入

- **WHEN** 一个批内先挂一个合法修正器、再挂一个会触发成环拒绝的修正器，然后提交
- **THEN** 该属性值保持批前值（不出现"合法那条已生效"的半写入），并有 ensure 提示

### Requirement: 未提交候选值预览

`PeekPending(Unit, FTcsAttributeName)` MUST 返回**不落账**的候选值预览（D2-5，供表现层预览）：

- 批进行中：返回该属性在当前批内**候选集**上算出的值（含尚未提交的变更）；
- 无进行中的批：返回与 `EvaluateCurrent` 相同的当前值；
- `PeekPending` MUST NOT 写回、MUST NOT 标脏、MUST NOT 广播（只读预览）。

#### Scenario: 批内预览未提交值

- **WHEN** 开批 → 挂一个把 100 改成 130 的修正器 → `PeekPending`
- **THEN** 返回 130，而 `EvaluateCurrent` 仍返回 100（事务期读旧值）；提交后两者一致

### Requirement: 按来源级联摘除

管线 MUST 提供 `RemoveBySource(Unit, FTcsSourceHandle)`（D2-2：modifier 生死完全绑定来源句柄，M2 无计时器）：

- 从该单位**全部属性**的修正器槽位中摘除 Source 匹配的条目 → 受影响属性标脏 → 重算 → 按变更规则广播；
- **扫描面 MUST 含冻结暂存区**（`FTcsAttributeStore::FrozenAttributes`）：来源可能在"属性被冻结"期间结束——若只在实例槽位里找，其修正器会永久滞留，属性恢复时**凭空多出数值**（比丢数值更难查）。命中暂存区时 MUST 留痕（日志）；
- 未命中任何条目时 MUST 为**正常路径**（不 ensure——来源可能只挂过已撤销的修正器）。

#### Scenario: 按来源全量摘除

- **WHEN** 同一来源给两个属性各挂一条修正器，然后 `RemoveBySource`
- **THEN** 两条都被摘除、两个属性都重算并各广播一次（值回到挂之前）

#### Scenario: 摘除扫描冻结暂存区

- **WHEN** 某属性的修正器随属性被冻结进暂存区，此时该修正器的来源结束并调用 `RemoveBySource`
- **THEN** 暂存区内的该条修正器同样被摘除（并留日志）；属性后续被解冻时**不会**带回已撤销来源的数值

#### Scenario: 无匹配来源不报错

- **WHEN** 以一个从未挂过任何修正器的来源调用 `RemoveBySource`
- **THEN** 正常返回（不 ensure）

