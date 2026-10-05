## MODIFIED Requirements

### Requirement: 池化状态实例记录

TcsState MUST 提供 `FTcsStateInstance`——**纯数据**记录，字段分三组：

- **身份与归属**：`DefTag`（`FGameplayTag`）/ `Handle` / `Source`（`FTcsSourceHandle`——**施加方来源句柄**，见下）/ `CascadeAnchor`（`FTcsSourceHandle`——**级联撤销锚点**，见下）/ `Instigator`（`FTcsCombatEntityHandle`）/ `Unit`（宿主单位——句柄里没有单位段，自持后 `GetState` 才是 O(1) 定位）；
- **数值与阶段**：`Stacks`（`int32`，从 1 起）/ `Level`（`int32`）/ `Phase`（`EStatePhase`）/ `ParamSnapshot`（`FTcsParamSnapshot`——施加瞬间冻结的生效数值，形状与构建规则见 `state-param-snapshot` 能力）；
- **时间**：`DurationRemaining` / `PeriodRemaining`（`double`）/ `ExpiryEntry`（时值到期条目锚点）/ `PeriodEntry`（周期到期条目锚点）。

**两个句柄的分工 MUST 守死（2026-10-05 解耦，判据见下）**：

- `CascadeAnchor` MUST 在**建出实例**时经 `FTcsSourceHandleRegistry::Allocate()` 取新值（进程内唯一、永不复用）；**刷新 / 叠层 MUST NOT 更换它**，替换路径是新实例 ⇒ 新锚点。它就是"撤销只摘本实例"的精确锚点——属性修正器条目与状态施加期登记的触发行都按它挂、按它一次摘净（`state-modifier-materialization` 与 `effect-trigger`）；
- `Source` MUST 是**施加方来源句柄**：调用方声明时沿用（同一施加方多次施加 ⇒ 同值），未声明时由门面发号（每次一枚新号）。它承担两件事——共存决策的"续杯 / 叠层"判据（`state-stacking-policies`）与生命周期事件载荷的归属读数；**MUST NOT 被当作撤销锚点**（一个来源可施加多个不同定义，按它撤销会互相误摘——这正是解耦的判据）。

**时值到期与周期到期 MUST 各持一个条目锚点**（`ExpiryEntry` / `PeriodEntry`）：`Finite` 与 `Period > 0` 是可同时成立的两个语义，挤在一个锚点里会让"撤销旧条目"分不清撤的是哪一条。

**MUST NOT** 持有：策略（`FInstancedStruct` / `TInstancedStruct` 作配置载体）、事件载荷、订阅句柄、任何 `UObject` / `TWeakObjectPtr<UObject>`（D3-7 v2 与 D2-9 纪律：值语义记录是池化与后续操作复制的地基）。**内联触发行同样不例外**：实例 MUST NOT 存触发行句柄，退订一律按 `CascadeAnchor` 级联。

**`ParamSnapshot` 内的 `SourceRef` 不构成本条的例外**：它是**已求值来源的副本**（值语义，非策略引用），作用是 Debug 可见取值来源与 Live 化零结构迁移；其内层若含对象引用，由门面的 `AddReferencedObjects` 统一补引用（GC 纪律，见 `state-param-snapshot` 能力）。

#### Scenario: 实例不持有策略与对象

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 无策略载体、无载荷、无订阅句柄、无 `UObject` 引用（策略归 Def 资产、载荷归事件、订阅句柄归注册表/总线）

#### Scenario: 两次施加得到互异的来源句柄

- **WHEN** 对同一单位连续施加同一 `DefTag` 两次，且共存策略使两次各自建出实例（`EGB_Custom` 判不同组 / `EGB_PerSource` 且两次来源不同）
- **THEN** 两个实例的 `Source` 互异且均非 0，且 `CascadeAnchor` 亦互异

#### Scenario: 同一来源的两个实例撤销互不牵连

- **WHEN** 以**同一个声明来源**先后施加两个**不同 `DefTag`** 的状态（各自建出实例、各自挂修正器），随后移除其中一个
- **THEN** 被移除者的修正器与触发行被摘净，另一实例的修正器与触发行**不受影响**（两实例的 `CascadeAnchor` 互异；按 `Source` 撤销会互相误摘，故撤销一律按锚点）

#### Scenario: 时值与周期各持条目锚点

- **WHEN** 检查一个 `Finite + Period > 0` 的在册实例
- **THEN** 它的 `ExpiryEntry` 与 `PeriodEntry` 是两个独立字段，各自可被独立撤销（互不挤占）

#### Scenario: 实例携带施加瞬间的数值快照

- **WHEN** 施加一个带参数行的状态后检查该实例
- **THEN** 它的 `ParamSnapshot` 含施加时刻求值得到的条目（不是空表，也不随外部输入变化）

### Requirement: 广播点与阶段迁移

**施加的共存结果 MUST 恰好广播一枚生命周期事件**（`Applied` / `Refreshed` 二者之一；替换路径是"旧实例被移除 + 新实例被建出"两笔，不属于"同一条实例的共存结果"）：无同组实例 ⇒ `TcsEvent.State.Applied`；命中同组 ⇒ `TcsEvent.State.Refreshed`，层数变化时另有 `TcsEvent.State.StackChanged`。`ExpireState` ⇒ `Expired`（`Cause = Expired`）、`RemoveState` ⇒ `Removed`（`Cause` 由调用方给）。**周期到点**另有 `TcsEvent.State.Periodic`（周期条目的重复回调，载荷带当前 `Stacks` 与 `Level`）——它是**追加**语义，不替代任一生命周期事件。

**"同组"判据归 `state-stacking-policies` 能力**（组键 = `单位 + DefTag` + 轴附加项）——本能力只保证**共存结果与广播的对应关系**：新建 ⇒ `Applied`；同来源刷新 ⇒ `Refreshed`；异来源叠层 ⇒ `StackChanged` + `Refreshed`（前者先到）；替换 ⇒ `Removed`（旧）+ `Applied`（新）；满仓拒绝 ⇒ 无广播。事件面 MUST NOT 因策略不同而增减（除上述对应关系外）。

**阶段机**：`EStatePhase{ Inactive / Active / Expiring }`，合法迁移 `Inactive → Active`（施加成功）、`Active → Expiring`（进入移除流程）、`Expiring → Inactive`（槽位释放）；**非法迁移 = `ensure`**（配置错误语义，与脏句柄的竞态口径分开）。

**撤销顺序**：MUST 先广播后释放槽位——订阅者在回调里仍能 `GetState` 读到该实例（否则"收到移除事件却查不到实例"）。**该顺序的前置两步**（2026-10-05 由"一步"扩为"两步"）：进入撤销流程时 MUST 先（①）按实例的级联锚点摘除属性修正器**并退订该实例的施加期登记物**（内联触发行；行为 Fragment 的订阅退订与①同处，见 `state-behavior-fragments` 的「行为订阅的生命周期接线」），再（②）撤销该实例的到期与周期条目。①排在广播之前的意义 = **该实例自己的触发行 MUST NOT 看见自己的 `Removed` / `Expired`**（"状态在，行为就在"的对偶：状态走了，行为先走）；②的意义 = 条目持句柄，槽位复用后回调仍会到达——先撤条目才不产生无谓回调。

**接线顺序（施加侧的对偶）**：`Applied` 广播**之前** MUST 已完成修正器挂载、内联触发行登记与（若该 Def 配了行为 Fragment）行为订阅——订阅者在 `Applied` 回调里读到的属性已是挂载后的数值，且新登记的触发行**能看见自己的 `Applied`**。

`UnregisterUnit` MUST 对该单位每条在册实例逐条广播 `Removed` 后删桶。

#### Scenario: 施加广播 Applied 且订阅者能在回调内查到实例

- **WHEN** 订阅 `TcsEvent.State.Applied` 后施加一个状态
- **THEN** 订阅者收到一笔该事件，且在回调内用载荷句柄调 `GetState` 能读到实例（广播早于释放，实例为 `Active`）

#### Scenario: 重复施加广播 Refreshed 而非 Applied

- **WHEN** 对同一单位以同一 `DefTag` **与同一来源句柄**连续施加两次
- **THEN** 第二笔广播的是 `TcsEvent.State.Refreshed`（`ApplyState` 返回 `Refreshed`），`Applied` 只收到一笔

#### Scenario: 移除广播 Removed 且原因可读

- **WHEN** 以 `Removed` 与 `Cancelled` 两种原因各移除一个实例
- **THEN** 两笔 `TcsEvent.State.Removed` 的载荷 `Cause` 分别等于给定原因（`Cancelled` 本轮无内建产生者，由调用方显式传入）

#### Scenario: 到期广播 Expired

- **WHEN** 对某实例调 `ExpireState`
- **THEN** 订阅者收到 `TcsEvent.State.Expired` 且载荷 `Cause == Expired`（`Removed` 不广播）

#### Scenario: 单位注销逐条广播 Removed

- **WHEN** 某单位在册两个实例时调 `UnregisterUnit`
- **THEN** 收到两笔 `TcsEvent.State.Removed`，随后该单位全部句柄查询被拒绝

#### Scenario: 非法阶段迁移触发 ensure

- **WHEN** 尝试把已释放（`Inactive`）的实例再次迁入 `Expiring`
- **THEN** 触发 `ensure`（配置错误面），且不产生第二笔移除广播

#### Scenario: 周期事件是追加而非替代

- **WHEN** 一个 `Finite + Period > 0` 的状态跨过一个周期并随后到期
- **THEN** 先收到 `Periodic`（实例仍在册），后收到 `Expired`——`Periodic` 不替代任何生命周期事件

#### Scenario: 移除时先撤条目再广播

- **WHEN** 移除一个带未到期条目的实例后推进时钟越过其原到期时刻
- **THEN** 不出现该实例的第二笔事件（条目已在撤销流程中被撤掉）

#### Scenario: 移除前已退订本实例的触发行

- **WHEN** 一个带内联触发器行的实例被移除或到期（其触发行订阅了 `TcsEvent.State.Removed` / `Expired`）
- **THEN** 退订发生在广播之前——该实例自己的触发行**不会**因自己的死亡事件起链
