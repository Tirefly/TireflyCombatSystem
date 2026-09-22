## MODIFIED Requirements

### Requirement: 按需重算与脏标记

管线 MUST 提供 `EvaluateCurrent(Unit, FGameplayTag)`（动词 = 求值，不是 `Resolve`——不从标识符取对象；**2026-09-22 改造：属性参数从 `FTcsAttributeName` 改为 `FGameplayTag`**）：

- 目标属性**不脏** → 直接返回 `CachedCurrent`（零重算）；
- 目标属性**脏** → 重算：收集该属性的修正器（按 `Op` 分桶）→ 求值各操作数 → 折叠 → 值域收口 → 写回 `CachedCurrent` → 清脏标记；
- **管线是 `CachedCurrent` 的唯一生产者**（定义期初值与任何外部写入都不算）；`BaseValue` 变更、修正器挂/摘、依赖属性变更、**`AddAttribute` 新建实例（含"解冻时实例已带脏标记"）** MUST 标脏；
- **新建即脏的理由（2026-09-18 增补）**：结算会被推迟到"批外读取或提交"这一刻——此时单位上的属性图通常已搭好（动态边界指向的引用属性、同期挂上的修正器都在位），**定义/添加顺序不影响结果**；若在 `AddAttribute` 内立即结算，动态边界会读到尚未添加的引用属性（求值 0）→ 把原值钳成 0 且无人再标脏（静默错值）。代价：首次结算可能广播一次（旧值 = 原始基础值）——结算前的缓存值不对外承诺；
- 属性未定义时 MUST 返回 0 且不崩溃（与读侧契约一致）。

#### Scenario: 干净属性零重算

- **WHEN** 属性刚重算完（不脏）再次 `EvaluateCurrent`
- **THEN** 返回值不变且不触发收集/折叠路径（以装置计数或缓存值观察）

#### Scenario: 脏则重算并清脏

- **WHEN** 挂上一个 `Add` 修正器（标脏）后 `EvaluateCurrent`
- **THEN** 返回新值（旧值 + 修正量）、`CachedCurrent` 更新、`bDirty` 复位
