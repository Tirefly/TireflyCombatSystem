## MODIFIED Requirements

### Requirement: 移除与到期时按来源级联摘除

`RemoveState` / 到期路径 MUST 在进入 `Expiring` 迁移**之前**按该实例的**级联锚点**（`FTcsStateInstance::CascadeAnchor`——撤销语境下的"来源"自 2026-10-05 起特指该锚点，口径见 `state-instance-lifecycle` 的「池化状态实例记录」）做级联摘除（`RemoveBySource`），并 MUST 在同一个属性变更批内提交——`TcsEvent.State.Expired` / `TcsEvent.State.Removed` 的订阅者在回调里读该属性能读到**已归位**的数值。`UnregisterUnit` MUST 复用同一条移除链。

撤销顺序 MUST 为：**按锚点摘除修正器（批内提交）→ 退订该实例的内联触发行 → 撤时间条目 → `Expiring` → 广播 → 归还槽位**。

**摘除 MUST 排在"取实例指针"之前**：摘除的批提交会重算 + 广播（属性变更事件），订阅者可在其中重入状态操作（移除本实例 / 注销单位）⇒ 摘除之后 MUST **重新定位桶与实例**，重入造成的失效由"桶不存在 / 实例查不到"两道守卫接住（MUST NOT 在摘除前后跨用同一个实例指针）。

摘除条数 MUST 由属性账本返回；返回 0（无匹配）MUST 是正常路径（锚点可能只挂过已摘除的条目），不 ensure。

#### Scenario: 驱散后条目消失且属性归位

- **WHEN** 移除一个已挂载修正器的状态实例
- **THEN** 账本条目消失、属性当前值回到施加前，且 `TcsEvent.State.Removed` 的订阅者在回调里读到的已是归位值

#### Scenario: 到期路径同款

- **WHEN** 让时钟推进跨过到期时刻使实例到期
- **THEN** 账本条目同样被摘除、属性归位（`Expired` 与显式移除走同一条摘除链）

#### Scenario: 单位注销清理该单位全部条目

- **WHEN** 注销一个持有多个带修正器状态实例的单位
- **THEN** 该单位属性账本上这些实例锚点的条目全部消失（逐条走同一条移除链）

### Requirement: 修正器来源句柄的身份一致性

物化的每一条账本条目 MUST 以该状态**实例的级联锚点**（`FTcsStateInstance::CascadeAnchor`）作为 `FTcsAttrModInstance::Source`；刷新 MUST NOT 更换它。由此 MUST 成立：**同一个状态实例 = 同一个锚点** ⇒ 一次按锚点摘除清掉该实例的全部修正器条目（内联触发行退订复用同一锚点，一次摘净两族条目）。

**MUST NOT 用"施加方来源句柄"（`FTcsStateInstance::Source`）作物化锚点**：同一来源可施加多个不同定义，按它摘除会误摘别的实例的条目。

#### Scenario: 同实例多条修正器共用来源

- **WHEN** 一个状态的 `ModifierRows` 物化出两条作用于不同属性的条目
- **THEN** 两条的 `Source` 相等（都等于该实例的级联锚点），且按该锚点一次摘除即两条全清

#### Scenario: 同一来源施加的两个实例各挂各的条目

- **WHEN** 以同一声明来源施加两个不同 `DefTag` 的状态（各自物化出账本条目），随后移除其中一个
- **THEN** 只有被移除者的条目消失，另一实例的条目仍在账本上（锚点不同）
