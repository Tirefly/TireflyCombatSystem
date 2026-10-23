# attribute-transaction Specification

## Purpose
定义属性变更的**批与提交**语义：嵌套变更批与唯一提交点、未提交候选值的预览查询，以及按来源句柄的级联摘除。**本能力不提供事务回滚**（历史能力名保留，语义以正文为准）。
## Requirements
### Requirement: 变更批与提交

管线 MUST 提供事务式写入（D2-5）：`BeginBatch(Entity)` / `Commit(Entity)`，同一批内完成的全部变更（挂修正器 / 摘修正器 / 改基础值 / **属性增删**）MUST 汇总为**一次重算 + 每属性最多一次广播**：

- **批内**：变更只标脏、只累加候选集（求值 → 折叠 → 值域收口全在内存候选值上做），不写回、不广播；
- **`Commit` 尾**：对全部脏属性逐个重算（含依赖顺延）并按 epsilon 比较后广播——**单帧多次变更只算一次**；
- 未显式开批时，各写入口 MUST 各自视为一个**隐式批**（行为与"开批-操作-提交"一致）；
- 批 MUST 支持嵌套计数（内层提交不触发 flush，最外层提交才触发）；
- **每实体事务状态与依赖边住 `FTcsAttributeStore`**（批深度 `BatchDepth` / 依赖边 `Dependents`）——与属性实例、冻结暂存区同生命周期，`UnregisterEntity` / `Deinitialize` 一并释放（无跨实体残留）；
- 写操作类 MUST 覆盖：挂修正器 / 按来源摘除 / **改基值**（`SetBaseValue(Entity, Attribute, NewBaseValue)`）/ 属性增删（`AddAttribute` / `RemoveAttribute`）。

#### Scenario: 批内多次改同属性只广播一次

- **WHEN** 一个批内对同一属性挂两个修正器（100 → 120 → 130），然后 `Commit`
- **THEN** 该属性只重算一次、只广播一次（`OldValue=100`、`NewValue=130`）

#### Scenario: 未开批即隐式批

- **WHEN** 单次调用挂一个修正器（不开批）
- **THEN** 行为与"开批 → 挂 → 提交"一致（立即生效并广播一次）

### Requirement: 唯一提交点与「提交才重算」——**不提供回滚**

批 MUST 只有**一个重算与广播的提交点**（`Commit` 的最外层）：批内的修正器挂载、基值写入、属性增删**当场落账**，只把 `Recalculate` 与 `Broadcast` 推迟到 `Commit`；`Commit` 失败的属性**不写回缓存值**（保持提交前那一份）：

- 写回与广播 MUST 限于本次批标脏的属性（不做全量重写）；
- 批内 MUST NOT 广播任何属性变更事件（订阅者看不到中间态，这是批存在的**主要理由**）；
- **批 MUST NOT 被当作事务使用**：它**不提供候选集、也不提供回滚**——`ApplyModifier` / `SetBaseValue` 在调用当刻就改了 `AttrModInstances` 与基值。需要「全有全无」的调用方 MUST 自己在批外先判后做（先校验全部前置条件，再统一施加）。

> **订正（2026-10-17，`ARCH-04` D-1 / `G-10`）**：本条原名「唯一提交点与**失败零写入**」，正文写「提交失败 MUST 零写入——不得出现『改了一半』的中间态」「提交失败时 MUST 保持批前状态（脏标记与缓存值回到批前语义）」。**代码从来没有实现过这半**——批只推迟 `Recalculate` 与 `Broadcast`，修正器在 `ApplyModifier` 当场 `AttrModInstances.Add`（`TcsAttributePipeline.cpp`），基值在 `SetBaseValue` 当场写（`TcsAttributePipeline_Cascade.cpp`），全模块 `Rollback` / `回滚` **零命中**。
> **为什么不改代码而改规格**：① **所有真实调用方都没有依赖原子性**——`TcsSkill` 明文写「MUST NOT 把批当作事务/回滚来依赖」（`TcsCostOps.cpp`）、`TcsBuff` 只借批做「摘旧与挂新之间无中间态」（这正是「批内不广播」给的）、`TcsDamage` 单属性写、`TcsEffect` 单条写 ⇒ **修代码是给零消费者付代价**；② 引入候选集会**破坏 `PeekPending` 的既有语义**（它今天读的就是已落账的实例 + 未提交的重算）；③ 能力名 `attribute-transaction` 的误导性**由本条的名称订正吸收**——「唯一提交点」是真、「事务」是历史命名。
> ⚠ **这条是真·`G-10`（规格更严、代码更松）的标本**：规格写的是 MUST，代码全没做，而**两侧都没错**——错的是当年把「延迟重算 + 合并广播」这半边命名为「事务」，于是另半边被当成"还差一点"。⇒ 判据：**凡规格写「失败零写入」而代码无候选集，先问"调用方有没有真的依赖它"，再决定改哪边**。

#### Scenario: 批内不广播中间态

- **WHEN** 一个批内对同一属性连续挂两个修正器（100 → 120 → 130），还没 `Commit`
- **THEN** 订阅者**一次事件都没收到**（批内可见的只有「脏标记已置」），`EvaluateCurrent` 仍返回 100

#### Scenario: 提交只重算一次

- **WHEN** 上述批 `Commit`
- **THEN** 该属性只重算一次、只广播一次（`OldValue=100`、`NewValue=130`）

#### Scenario: 成环被拒时只有那一条不生效，合法的那条仍在

- **WHEN** 一个批内先挂一个合法修正器、再挂一个会触发成环拒绝的修正器，然后提交
- **THEN** 成环那一条被 ensure 拒绝且**不登记依赖边**，而**先挂的合法那条照常生效**——批**不会**因此"整个作废"；这正是「批不是事务」的可观测面

#### Scenario: 需要全有全无的调用方先判后做

- **WHEN** 调用方要先确认"两条修正器都能挂上"再决定是否施加
- **THEN** 它 MUST 在**开批之前**自行校验全部前置条件（如目标属性存在、来源有效），再统一施加；MUST NOT 指望 `Commit` 回滚已落账的条目

### Requirement: 未提交候选值预览

`PeekPending(Entity, FGameplayTag)` MUST 返回**不落账**的候选值预览（D2-5，供表现层预览；**2026-09-22 改造：属性参数从 `FTcsAttributeName` 改为 `FGameplayTag`**）：

- 批进行中：返回该属性在当前批内**候选集**上算出的值（含尚未提交的变更）；
- 无进行中的批：返回与 `EvaluateCurrent` 相同的当前值；
- `PeekPending` MUST NOT 写回、MUST NOT 标脏、MUST NOT 广播（只读预览）。

#### Scenario: 批内预览未提交值

- **WHEN** 开批 → 挂一个把 100 改成 130 的修正器 → `PeekPending`
- **THEN** 返回 130，而 `EvaluateCurrent` 仍返回 100（事务期读旧值）；提交后两者一致

### Requirement: 按来源级联摘除

管线 MUST 提供 `RemoveBySource(Entity, FTcsSourceHandle)`（D2-2：modifier 生死完全绑定来源句柄，M2 无计时器）：

- 从该实体**全部属性**的修正器槽位中摘除 Source 匹配的条目 → 受影响属性标脏 → 重算 → 按变更规则广播；
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
