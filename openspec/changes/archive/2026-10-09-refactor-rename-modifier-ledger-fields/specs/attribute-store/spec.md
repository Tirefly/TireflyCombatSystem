## MODIFIED Requirements

### Requirement: 属性定义表与单位侧添加移除

门面 MUST 持有**属性定义表**并在内部完成定义解析——**调用面纪律**（2026-09-17 用户口径）：单位侧只按属性 tag 操作，调用方 MUST NOT 传定义数据（定义解析是门面内部执行流程）。

**键类型（2026-09-22 改造）**：定义表键与全部 API 的属性参数从 `FTcsAttributeName` 改为 **`FGameplayTag`**（`FTcsAttributeName` 整体移除，见 `attribute-types` 能力的"属性身份 = GameplayTag"需求）。空值判定从 `IsNone()` 改为 `!Tag.IsValid()`。**键的归属（2026-10-01 换根）**：属性 tag 住宿主侧登记的 `Attribute` 根（形态 `Attribute.<名>`，宿主 ini 声明；见 `attribute-types` 能力与宿主仓的根段注册表）——本插件 MUST NOT 声明任何属性名，也 MUST NOT 在插件侧根段注册表里登记该根。

- `RegisterAttributeDef(const FGameplayTag& Attribute, const FTcsAttributeDefData& DefData)`：宿主 / DefLibrary 加载定义资产后登记（`UTcsAttributeDef::Def` 即定义数据、`DefTag` 即属性身份）；属性 tag 由调用方显式给出。拒绝面（ensure + false）：属性 tag 无效、同 tag 重复登记（D2-1：词表重名 = 加载期错误，不得静默覆写）。
- `FindAttributeDef(FGameplayTag)`：查回已登记定义（未登记返回 nullptr，**正常查询路径不 ensure**）。
- `AddAttribute(Unit, FGameplayTag)`：**解冻优先**——若暂存区已有同一 tag 实例则整条搬回（基础值/边界/值域模式/修正器条目集原样保留，MUST 输出解冻日志）；否则按定义数据新建实例（初值 `BaseValue`、`CachedCurrent = BaseValue`（占位：结算前的缓存值不对外承诺）、**`bDirty = true`（新建即脏：值域收口/既有修正器/动态边界都由管线在结算时生效）**、`AttrModInstances` 空，MUST 输出新建日志）。拒绝面（ensure + false）：单位未注册、属性 tag 无效、该单位已持有同 tag 属性、**定义未登记**（仅新建路径需要定义）、定义数据的动态边界自引用（D2-4）。
- `RemoveAttribute(Unit, FGameplayTag)`：**冻结**——把整条实例（含基础值/边界/值域模式/修正器条目集）从 `Attributes` 搬入暂存区 `FrozenAttributes`，MUST **不销毁、不丢弃修正器条目**，MUST 输出冻结日志（含属性 tag 与修正器条目数）。拒绝面（ensure + false）：单位未注册、属性 tag 无效、该单位未持有此属性。
- **事务纪律（2026-09-18 增补）**：`AddAttribute` / `RemoveAttribute` / `SetBaseValue` MUST 走与修正器写入**同一 store 变更路径与同一事务纪律**——批内与批内挂 modifier 的可见性/重算/广播行为一致；施加到**已无实例**的属性上的修正器 MUST 被忽略并留日志（不 ensure——框架允许动态增删且不做来源追溯）。
- `SetBaseValue(Unit, FGameplayTag, double)`（**2026-09-18 增补**，02 §2.2a 的"等级成长 = 宿主升级事务改基值"落点）：改写基础值 → 标脏 → 按事务纪律重算/广播（批外 = 隐式批，立即生效）。拒绝面（ensure + false）：单位未注册、属性 tag 无效、该单位未持有此属性。

实例 MUST NOT 持有定义数据或资产的引用（热路径不回查定义）。定义数据的**资产 → 载荷**解析归 DefLibrary（M6）/ 词表（M8）——本能力无 DataTable / 资产加载路径。

**字段名（2026-10-09 改名）**：本能力引用的修正器容器字段名 MUST 为 **`AttrModInstances`**——命名规则见 `attribute-types` 能力的"账本修正器与属性实例"需求（容器字段名 = 元素类型名复数；`Slot` 保留给可寻址/可复用的空位）。本能力正文一律以该名指代。

#### Scenario: 定义先登记后按 tag 添加

- **WHEN** 先 `RegisterAttributeDef`（BaseValue 100、静态 0..100、Clamp）再 `AddAttribute(Unit, Attribute.Health)`
- **THEN** 实例基础值与缓存占位值均为 100、`bDirty` 为真（待管线结算）、边界与值域来自定义数据；调用方未传任何定义数据
- **AND** 该属性首次 `EvaluateCurrent` 后值为 100（基础值经值域收口结算）且 `bDirty` 转假

#### Scenario: 越界初值经结算收口

- **WHEN** 定义数据 BaseValue 150、边界静态 0..100、Clamp，`AddAttribute` 后首次 `EvaluateCurrent`
- **THEN** 返回 100（不是 150）——收口不会因"实例刚建、值是从定义抄来的"而被跳过

#### Scenario: 定义未登记被拦截

- **WHEN** 对未登记过的属性 tag 调用 `AddAttribute`（且暂存区无同 tag 实例）
- **THEN** ensure 命中且返回 false（不建实例）

#### Scenario: 重复登记与重复添加被拦截

- **WHEN** 同 `DefTag` 再次 `RegisterAttributeDef`，或对同一单位已持有的属性再次 `AddAttribute`
- **THEN** 各自 ensure 命中且返回 false（不覆写既有定义／实例）

#### Scenario: 动态边界自引用被拦截

- **WHEN** 登记的定义数据其 `Bounds.Max.DynamicAttribute` 为自身 tag，并对其调用 `AddAttribute`
- **THEN** ensure 命中且不建实例

#### Scenario: 移除属性 = 冻结而非销毁

- **WHEN** 对已持有的属性调用 `RemoveAttribute`
- **THEN** 返回 true、该实例不再可查（`FindInstance` 为空）、整条实例出现在暂存区（`FindFrozenInstance` 非空且字段保真）、并输出冻结日志；未持有的属性调用时 ensure 命中且返回 false

#### Scenario: 添加属性 = 解冻优先

- **WHEN** 属性被冻结后（期间其基础值可能被宿主改过）再次对该单位调用 `AddAttribute`
- **THEN** 暂存区整条搬回：基础值/边界/值域模式/修正器条目集与冻结前一致（**不回落到定义默认值**），并输出解冻日志；暂存区该条目消失

#### Scenario: 属性增删与修正器同事务

- **WHEN** 一个批内先 `AddAttribute` 再挂一条修正器，然后提交
- **THEN** 只重算一次、只广播一次（与"批内挂两条修正器"行为一致）
