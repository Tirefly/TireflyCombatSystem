## MODIFIED Requirements

### Requirement: 打断

`TcsSkill` MUST 提供打断入口 `CancelCast(FTcsSkillInstanceHandle Run, <打断原因>)`，其结算 MUST 先过查询契约、MUST NOT 无条件打断：

- 结算序：① 经 `CanBeCanceled()` 判可打断性 → ② 不可打断 ⇒ **拒绝**（返回 false + 具名 `Log`，**运行态原样继续**、MUST NOT 发任何事件、MUST NOT 动账本）→ ③ 可打断 ⇒ 走**四路共用的终结例程**并记原因为"打断"（终结例程与回收面住 `skill-cast-runtime` 能力的「施法运行态与实例池」需求——**本能力只定义"什么时候允许打断 + 记什么原因"，不重复定义"终结时摘什么"**）；
- 打断 MUST 发 `TcsEvent.Cast.Interrupted`、**MUST NOT** 发 `TcsEvent.Cast.Completed`——"被打断"与"跑完了"是两件事，**MUST NOT** 用完成事件冒充打断（那会让配了 `OnCastCompleted` 起链的技能在被打断时真的打出主链）；
- **止于未来不追溯**（M4 约定）：打断 MUST NOT 回退**已发生**的副作用（已挂的账本条目按来源回收是**生命周期终结**，不是"回滚历史"）；
- **来源优先级仲裁本轮如实登记为未落面**：设计原词是"来源优先级 vs `CanBeCanceled()`"，而**全仓无任何类型声明"打断来源的优先级"**（技能关系字段 `Priority` 的语义属 `R5.5-e` 关系字段检查器，尚未定稿）⇒ 本轮**只落可打断性裁决**，**MUST NOT** 为它发明一个零消费者的优先级形参（"零消费者不预建"）。

**★ 2026-10-16 `R5.5-e1` 改名（本需求已现况化）**：`IsInterruptibleNow()` → **`CanBeCanceled()`**、`bInterruptible` → **`bCancellable`**（时段字段），与 `ECFR_Canceled` / `bCancellableDefault` / `ECT_OnCastCanceled` **同族同批**。**判据**：「打断」是手段、「取消」是结果——关系表族加入后，取消可由 `TagsToCancel` 直接产生（`EBRC_Cancelled` 首次有内建产生者），**不经过「打断」这个手段** ⇒ 方法名 MUST 描述**结果**，否则关系表取消路径读不通。`TcsEvent.Cast.Interrupted` 事件词**本轮不改**（大概率退役）。

#### Scenario: 可打断段被打断成功

- **WHEN** 一个当前时段 `bCancellable = true` 的技能在飞时调用 `CancelCast`
- **THEN** 返回成功；`TcsEvent.Cast.Interrupted` **恰好 1 次**、`TcsEvent.Cast.Completed` **零次**；该 run 不再是飞状态

#### Scenario: 不可打断段被打断被拒（阴性对照）

- **WHEN** 同上但当前时段 `bCancellable = false`（或默认档开关为 `false`）
- **THEN** 返回**失败**；`TcsEvent.Cast.Interrupted` **零次**、`TcsEvent.Cast.Completed` **零次**；**且旧 run 原样继续**——判据 MUST 含"运行态仍可解析 + 在飞读数不变"（只验"没有事件"会让"静默把运行态弄坏了"的实现全绿）
