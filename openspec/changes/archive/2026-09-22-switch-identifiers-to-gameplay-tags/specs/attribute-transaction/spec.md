## MODIFIED Requirements

> **术语订正（2026-10-14）**：本文原用「单位 / `Unit`」，该词已退役，现统一为「**实体 / `Entity`**」（见 `DEC-2026-10-14-entity-vocabulary`）。**正文为当时原话，保留不改。**

### Requirement: 未提交候选值预览

`PeekPending(Unit, FGameplayTag)` MUST 返回**不落账**的候选值预览（D2-5，供表现层预览；**2026-09-22 改造：属性参数从 `FTcsAttributeName` 改为 `FGameplayTag`**）：

- 批进行中：返回该属性在当前批内**候选集**上算出的值（含尚未提交的变更）；
- 无进行中的批：返回与 `EvaluateCurrent` 相同的当前值；
- `PeekPending` MUST NOT 写回、MUST NOT 标脏、MUST NOT 广播（只读预览）。

#### Scenario: 批内预览未提交值

- **WHEN** 开批 → 挂一个把 100 改成 130 的修正器 → `PeekPending`
- **THEN** 返回 130，而 `EvaluateCurrent` 仍返回 100（事务期读旧值）；提交后两者一致
