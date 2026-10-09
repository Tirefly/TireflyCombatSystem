## MODIFIED Requirements

### Requirement: 账本修正器与属性实例

`TcsAttribute` MUST 提供账本侧两个**纯 C++ struct**（无 `USTRUCT` 宏、不进 UHT 类型面——D2-13 账本形状只为聚合热路径服务）：

- `FTcsAttrModInstance{ Target: FGameplayTag, Op: ETcsAttributeOp, Operand: FTcsAttrModOperand, Source: FTcsSourceHandle, OverridePriority: int32, SortKey: int32 }`——`Source` 是级联撤销锚点（D2-2：来源注销 → 按 Source 全量移除）；`OverridePriority` **仅 `TAO_Override` 读**（其余带忽略），`SortKey` 始终不参与折叠（**2026-09-23 删除：原 `Tag: FName` 字段**——零消费者，见「修正器模板资产与约定列白名单」的同款说明）；
- `FTcsAttributeInstance{ Attr: FGameplayTag, BaseValue: double, CachedCurrent: double, bDirty: bool, Bounds: FTcsAttributeBounds, ValueDomain: ETcsAttributeValueDomain, OverrideTieBreak: ETcsAttrOverrideTieBreak, AttrModInstances: TArray<FTcsAttrModInstance> }`——`CachedCurrent` 是**派生缓存非权威**（聚合管线唯一生产者、惰性重算，D2-8）；`OverrideTieBreak` 由定义侧展开（热路径不回查定义）。字段 `AttrModInstances` 承载本属性的修正器条目集（**2026-10-09 由 `ModifierSlots` 改名**——见下方命名规则）。

**账本容器字段的命名规则（2026-10-09 立）**：**容器字段名 MUST 取元素类型名的复数形式**（本条 = `FTcsAttrModInstance` → `AttrModInstances`）。判据可自检：**字段名 MUST 能从元素类型名推出来**。**`Slot` 一词在本仓保留给"可按下标寻址、可复用、带代际的空位"**（如 `FreeSlots` / `PoolSlots` / `StateEventSlots`）——本容器既不可按下标寻址、也不复用、也不带代际（全仓仅有 `.Add` / `.RemoveAll` / `.Num()` / 范围遍历，`[]` / `Insert` / `RemoveAt` 零命中）⇒ **MUST NOT** 以 `Slots` 命名。**MUST NOT** 以"与旧名同族"为由复活 `Slots` 后缀：同族一致性由"同一条命名法"承载，而非由"同一个后缀"承载。

**无分组标签字段（2026-09-23 用户拍板）**：账本修正器**不携带**任何"同来源内分组标签"字段——原 `Tag: FName` 从旧 TCS 搬来且**全库零消费者**（折叠/物化/级联撤销均不读），已删除。若将来出现"同来源内分组"的真实需求，该能力 MUST 以 `FGameplayTag` 形态重新引入（与全系统标识体系一致），**MUST NOT** 复活 `FName` 版本。

#### Scenario: 修正器携带级联锚点

- **WHEN** 同一来源挂两条修正器到不同属性
- **THEN** 两条修正器的 `Source` 相等，可按该句柄一次性全量摘除（D2-2）

#### Scenario: 覆盖优先级随实例进账本

- **WHEN** 一条 `TAO_Override` 修正器以 `OverridePriority = 9` 挂到某属性
- **THEN** 账本条目保留该值，折叠按它参与强弱裁决（非覆盖带上该字段不参与任何计算）

#### Scenario: 账本修正器无分组标签字段

- **WHEN** 检查 `FTcsAttrModInstance` 的字段集
- **THEN** 不含任何分组标签字段（`Target` / `Op` / `Operand` / `Source` / `OverridePriority` / `SortKey` 六项之外无字段）

#### Scenario: 实例具备脏标记位

- **WHEN** 属性实例被新建
- **THEN** 实例带 `bDirty` 标记位且**新建即脏**——初值由聚合管线在"批外读取或提交"时结算（值域收口与既有修正器在该次结算生效）并清零脏标记（见 `attribute-store` 与 `attribute-pipeline` 能力）

#### Scenario: 账本容器字段名可从元素类型名推出

- **WHEN** 检查 `FTcsAttributeInstance` 承载 `FTcsAttrModInstance` 条目的字段名
- **THEN** 该字段名为元素类型名的复数形式（`AttrModInstances`），**不含** `Slots` 后缀；且该容器**不提供**下标寻址 / 槽位复用 / 代际接口（它只被追加与按来源全量摘除）
