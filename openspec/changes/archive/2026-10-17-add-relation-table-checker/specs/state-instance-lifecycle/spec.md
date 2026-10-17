## MODIFIED Requirements

### Requirement: 广播点与阶段迁移

**施加的共存结果 MUST 恰好广播一枚生命周期事件**（`Applied` / `Refreshed` 二者之一；替换路径是"旧实例被移除 + 新实例被建出"两笔，不属于"同一条实例的共存结果"）：无同组实例 ⇒ `TcsEvent.State.Applied`；命中同组 ⇒ `TcsEvent.State.Refreshed`，层数变化时另有 `TcsEvent.State.StackChanged`。`ExpireState` ⇒ `Expired`（`Cause = Expired`）、`RemoveState` ⇒ `Removed`（`Cause` 由调用方给）。**周期到点**另有 `TcsEvent.State.Periodic`（周期条目的重复回调，载荷带当前 `Stacks` 与 `Level`）——它是**追加**语义，不替代任一生命周期事件。

**"同组"判据归 `state-stacking-policies` 能力**（组键 = `实体 + DefTag` + 轴附加项）——本能力只保证**共存结果与广播的对应关系**：新建 ⇒ `Applied`；同来源刷新 ⇒ `Refreshed`；异来源叠层 ⇒ `StackChanged` + `Refreshed`（前者先到）；替换 ⇒ `Removed`（旧）+ `Applied`（新）；满仓拒绝 ⇒ 无广播。事件面 MUST NOT 因策略不同而增减（除上述对应关系外）。

**阶段机**：`EBuffPhase{ Inactive / Active / Expiring }`，合法迁移 `Inactive → Active`（施加成功）、`Active → Expiring`（进入移除流程）、`Expiring → Inactive`（槽位释放）、`Expiring → Active`（仍在册实例的刷新过渡态归位）；**非法迁移 = `ensure`**（配置错误语义，与脏句柄的竞态口径分开）。

**撤销顺序**：MUST 先广播后释放槽位——订阅者在回调里仍能 `GetState` 读到该实例（否则"收到移除事件却查不到实例"）。**该顺序的前置两步**（2026-10-05 由"一步"扩为"两步"）：进入撤销流程时 MUST 先（①）按实例的级联锚点摘除属性修正器**并退订该实例的施加期登记物**（内联触发行；行为 Fragment 的订阅退订与①同处，见 `state-behavior-fragments` 的「行为订阅的生命周期接线」），再（②）撤销该实例的到期与周期条目。①排在广播之前的意义 = **该实例自己的触发行 MUST NOT 看见自己的 `Removed` / `Expired`**（"状态在，行为就在"的对偶：状态走了，行为先走）；②的意义 = 条目持句柄，槽位复用后回调仍会到达——先撤条目才不产生无谓回调。

**接线顺序（施加侧的对偶）**：`Applied` 广播**之前** MUST 已完成修正器挂载、内联触发行登记与（若该 Def 配了行为 Fragment）行为订阅——订阅者在 `Applied` 回调里读到的属性已是挂载后的数值，且新登记的触发行**能看见自己的 `Applied`**。

`UnregisterEntity` MUST 对该实体每条在册实例逐条广播 `Removed` 后删桶。

**有限重入守卫（STAT-5）**：门面 MUST 仅在真正移除广播窗口将句柄登记到 InRemovalBroadcastHandles；递归 Remove 同句柄 false 静默，不重复广播或释放。MUST NOT 因任意 Expiring 判为正在移除（刷新也暂用此阶段）。既有清理顺序不改变，任意递归修改组合仍未保证。

**广播后身份重查**：Applied/Refreshed 回调可移除自身或注销实体；广播者 MUST 用值快照，之后按句柄重查，不访问旧指针、不恢复已释放实例、不补播失效实例。移除广播返回后同样只释放仍在册且身份一致的槽位。

#### Scenario: 施加广播 Applied 且订阅者能在回调内查到实例

- **WHEN** 订阅 `TcsEvent.State.Applied` 后施加一个状态
- **THEN** 订阅者收到一笔该事件，且在回调内用载荷句柄调 `GetState` 能读到实例（广播早于释放，实例为 `Active`）

#### Scenario: 重复施加广播 Refreshed 而非 Applied

- **WHEN** 对同一实体以同一 `DefTag` **与同一来源句柄**连续施加两次
- **THEN** 第二笔广播的是 `TcsEvent.State.Refreshed`（`ApplyState` 返回 `Refreshed`），`Applied` 只收到一笔

**★ 2026-10-16 `R5.5-e1` 补充：`Cancelled` 首次有内建产生者（本需求已现况化）**：`EBuffRemoveCause::EBRC_Cancelled` 自落地起**只有注释自述「由调用方显式传入」，框架从不产生它**。`R5.5-e1` 的关系表检查器落地后，`TagsToCancel` 挂点会在施加成功路径上按它逐条 `RemoveState(handle, EBRC_Cancelled)` ⇒ 该枚举**第一次由框架自身产生**。MUST 记录的边界：本补充**只增一个产生者**，`Removed` / `Expired` 两档的来源与语义**一字不改**；`TagsToCancel` 为空（既有资产的现状）时**零行为变化**。

#### Scenario: 移除广播 Removed 且原因可读

- **WHEN** 以 `Removed` 与 `Cancelled` 两种原因各移除一个实例
- **THEN** 两笔 `TcsEvent.State.Removed` 的载荷 `Cause` 分别等于给定原因（`Cancelled` 本轮无内建产生者，由调用方显式传入）

#### Scenario: 到期广播 Expired

- **WHEN** 对某实例调 `ExpireState`
- **THEN** 订阅者收到 `TcsEvent.State.Expired` 且载荷 `Cause == Expired`（`Removed` 不广播）

#### Scenario: 实体注销逐条广播 Removed

- **WHEN** 某实体在册两个实例时调 `UnregisterEntity`
- **THEN** 收到两笔 `TcsEvent.State.Removed`，随后该实体全部句柄查询被拒绝

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

#### Scenario: Applied 回调可移除自身

- **WHEN** Applied 订阅者按载荷句柄移除自己
- **THEN** 自移除成立，施加流程不再访问失效实例或重复清理，无额外红字

#### Scenario: Refreshed 自移除不被过渡阶段误拒

- **WHEN** Refreshed 回调移除处于刷新过渡态的实例
- **THEN** 自移除成立，刷新流程重查后不恢复已释放实例，无额外红字

#### Scenario: Removed 递归移除静默拒绝

- **WHEN** Removed 回调再次移除同一句柄
- **THEN** 递归调用 false 静默，外层广播和释放各一次，不重复归还槽位
