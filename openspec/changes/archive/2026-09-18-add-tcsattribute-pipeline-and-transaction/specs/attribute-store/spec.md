## MODIFIED Requirements

### Requirement: 属性定义表与单位侧添加移除

门面 MUST 持有**属性定义表**并在内部完成定义解析——**调用面纪律**（2026-09-17 用户口径）：单位侧只按属性名操作，调用方 MUST NOT 传定义数据（定义解析是门面内部执行流程）。

- `RegisterAttributeDef(const FTcsAttributeName& Attribute, const FTcsAttributeDefTableRow& DefRow)`：宿主 / DefLibrary 加载定义资产后登记（`UTcsAttributeDef::Def` 即行、`DefId` 即属性名）；属性名由调用方显式给出（行内不带 id）。拒绝面（ensure + false）：属性名为空、同属性名重复登记（D2-1：词表重名 = 加载期错误，不得静默覆写）。
- `FindAttributeDef(FTcsAttributeName)`：查回已登记定义（未登记返回 nullptr，**正常查询路径不 ensure**）。
- `AddAttribute(Unit, FTcsAttributeName)`：**解冻优先**——若暂存区已有同名实例则整条搬回（基础值/边界/值域模式/修正器槽位原样保留，MUST 输出解冻日志）；否则按定义行新建实例（初值 `BaseValue`、`CachedCurrent = BaseValue`（占位：结算前的缓存值不对外承诺）、**`bDirty = true`（新建即脏：值域收口/既有修正器/动态边界都由管线在结算时生效）**、`ModifierSlots` 空，MUST 输出新建日志）。拒绝面（ensure + false）：单位未注册、属性名为空、该单位已持有同名属性、**定义未登记**（仅新建路径需要定义）、定义行的动态边界自引用（D2-4）。
- `RemoveAttribute(Unit, FTcsAttributeName)`：**冻结**——把整条实例（含基础值/边界/值域模式/槽位内容）从 `Attributes` 搬入暂存区 `FrozenAttributes`，MUST **不销毁、不丢弃槽位内容**，MUST 输出冻结日志（含属性名与槽位数）。拒绝面（ensure + false）：单位未注册、属性名为空、该单位未持有此属性。
- **事务纪律（2026-09-18 增补）**：`AddAttribute` / `RemoveAttribute` / `SetBaseValue` MUST 走与修正器写入**同一 store 变更路径与同一事务纪律**——批内与批内挂 modifier 的可见性/重算/广播行为一致；施加到**已无实例**的属性上的修正器 MUST 被忽略并留日志（不 ensure——框架允许动态增删且不做来源追溯）。
- `SetBaseValue(Unit, FTcsAttributeName, double)`（**2026-09-18 增补**，02 §2.2a 的"等级成长 = 宿主升级事务改基值"落点）：改写基础值 → 标脏 → 按事务纪律重算/广播（批外 = 隐式批，立即生效）。拒绝面（ensure + false）：单位未注册、属性名为空、该单位未持有此属性。

实例 MUST NOT 持有定义行或资产的引用（热路径不回查定义）。定义数据的**资产 → 载荷**解析归 DefLibrary（M6）/ 词表（M8）——本能力无 DataTable / 资产加载路径。

#### Scenario: 定义先登记后按名添加

- **WHEN** 先 `RegisterAttributeDef`（BaseValue 100、静态 0..100、Clamp）再 `AddAttribute(Unit, Health)`
- **THEN** 实例基础值与缓存占位值均为 100、`bDirty` 为真（待管线结算）、边界与值域来自定义行；调用方未传任何定义数据
- **AND** 该属性首次 `EvaluateCurrent` 后值为 100（基础值经值域收口结算）且 `bDirty` 转假

#### Scenario: 越界初值经结算收口

- **WHEN** 定义行 BaseValue 150、边界静态 0..100、Clamp，`AddAttribute` 后首次 `EvaluateCurrent`
- **THEN** 返回 100（不是 150）——收口不会因"实例刚建、值是从定义抄来的"而被跳过

#### Scenario: 定义未登记被拦截

- **WHEN** 对未登记过的属性名调用 `AddAttribute`（且暂存区无同名实例）
- **THEN** ensure 命中且返回 false（不建实例）

#### Scenario: 重复登记与重复添加被拦截

- **WHEN** 同 `DefId` 再次 `RegisterAttributeDef`，或对同一单位已持有的属性再次 `AddAttribute`
- **THEN** 各自 ensure 命中且返回 false（不覆写既有定义／实例）

#### Scenario: 动态边界自引用被拦截

- **WHEN** 登记的定义行其 `Bounds.Max.DynamicAttribute` 为自身，并对其调用 `AddAttribute`
- **THEN** ensure 命中且不建实例

#### Scenario: 移除属性 = 冻结而非销毁

- **WHEN** 对已持有的属性调用 `RemoveAttribute`
- **THEN** 返回 true、该实例不再可查（`FindInstance` 为空）、整条实例出现在暂存区（`FindFrozenInstance` 非空且字段保真）、并输出冻结日志；未持有的属性调用时 ensure 命中且返回 false

#### Scenario: 添加属性 = 解冻优先

- **WHEN** 属性被冻结后（期间其基础值可能被宿主改过）再次对该单位调用 `AddAttribute`
- **THEN** 暂存区整条搬回：基础值/边界/值域模式/修正器槽位与冻结前一致（**不回落到定义默认值**），并输出解冻日志；暂存区该条目消失

#### Scenario: 属性增删与修正器同事务

- **WHEN** 一个批内先 `AddAttribute` 再挂一条修正器，然后提交
- **THEN** 只重算一次、只广播一次（与"批内挂两条修正器"行为一致）

## ADDED Requirements

### Requirement: 属性冻结暂存区

`FTcsAttributeStore` MUST 持有一个暂存区 `FrozenAttributes`（`TMap<FTcsAttributeName, FTcsAttributeInstance>`），承载"被冻结的整条属性实例"：

- **整条进出**：冻结与解冻都以**整条实例**为单位（基础值 / 边界 / 值域模式 / 修正器槽位一并保留）——这是"装备穿脱不丢等级加成"与"buff 还在、数值不丢"两条兜底的实现基础。
- **双态约束**：同一属性名 MUST NOT 同时存在于 `Attributes` 与 `FrozenAttributes`（搬移语义保证；两处都有 = 双份真相，会导致重复计算）。
- **查询口**：`FindFrozenInstance(Name)`（mutable/const，未命中返回 nullptr，**不 ensure**——正常查询路径）。
- **与来源撤销的共存（MUST）**：来源撤销（`RemoveBySource`）**MUST 同时扫描实例槽位与暂存区**，否则来源在"属性被冻结"期间结束、其修正器永久滞留，属性恢复时**凭空多出数值**（比丢数值更难查）。
- **生命周期**：暂存区随单位容器生存——`UnregisterUnit` / `Deinitialize` 释放（不泄漏）；MUST NOT 引入条目上限、保质期清理或"解冻时校验来源存活性"（框架判断不了来源死活：`FTcsSourceHandle` 只是编号、无存活性登记，属既有设计——来源生死由持有者按既有纪律调 `RemoveBySource` 处理）。

#### Scenario: 冻结后整条保真

- **WHEN** 某属性（基础值被宿主改过、槽位内有修正器）被冻结
- **THEN** 暂存区条目与冻结前逐字段一致（基础值、边界、值域模式、槽位数）

#### Scenario: 解冻后恢复原值

- **WHEN** 冻结后再次 `AddAttribute` 同名属性
- **THEN** 实例基础值为冻结前的值（非定义默认值）、槽位内容仍在、`bDirty` 语义沿用冻结前状态

#### Scenario: 双态互斥

- **WHEN** 检查同一属性名在 `Attributes` 与 `FrozenAttributes` 的存在情况
- **THEN** 两者不同时为真（任一次冻结/解冻后均成立）

#### Scenario: 单位注销释放暂存区

- **WHEN** 单位带着冻结条目被 `UnregisterUnit`
- **THEN** 该单位的 `Attributes` 与 `FrozenAttributes` 一并释放
