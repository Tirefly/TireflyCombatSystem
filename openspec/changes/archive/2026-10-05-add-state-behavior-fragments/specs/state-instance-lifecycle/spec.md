## MODIFIED Requirements

### Requirement: 状态实例句柄

TcsState MUST 提供 `FTcsStateHandle`（`USTRUCT(BlueprintType)`）：两字段 `int32 Index` 与 `int32 Generation`（**展平形态**，与 `FTcsChainRunHandle` / `FTcsEffectTriggerInstance` 同款——反射与宿主脚本往返需要可反射的标量字段，对象身份不外传）。

- `IsValid()`：`Index >= 0` 且 `Generation > 0`（0 恒为“从未分配”）；
- 相等比较按 `(Index, Generation)` 全等；MUST 提供 `GetTypeHash`（供 `TMap` / `TSet` 键控）；
- `Index` MUST 是单位桶内槽位下标；**分配代际 MUST 由 TcsState 模块出线的进程级发号器统一发放不复用的正奇数号**，世界拆解 MUST NOT 复位发号器；不同桶、被删除后重建的桶、不同世界即使使用同一下标，也 MUST NOT 发放相同实例身份；
- 释放 MUST 将该槽代际置为偶数；再次分配 MUST 取新的进程级奇数号，MUST NOT 仅将桶内旧代际 +1 当作分配身份；
- 句柄仍相对其世界门面有义，MUST NOT 将旧句柄跨世界重用；旧身份在新世界 MUST 无法命中。反射 `int32` 的正值代际空间耗尽时 MUST 明确失败，MUST NOT 回绕复用。

#### Scenario: 未分配句柄无效

- **WHEN** 取一个默认构造的 `FTcsStateHandle` 并调 `IsValid`
- **THEN** 返回 false（`Index` 为负或 `Generation` 为 0）

#### Scenario: 分配出的句柄可直接判有效

- **WHEN** 施加一个状态并取回其句柄（`Index` 可能为 0）
- **THEN** `IsValid` 返回 true（合法性判据是“`Generation` 非 0”，不是“下标非 0”——下标 0 是合法槽位）

#### Scenario: 用过的句柄在槽位回收后失配

- **WHEN** 移除一个状态使槽位回收，随后用旧句柄查询
- **THEN** 查询被拒绝（代际失配），且同一槽位新分配的实例不受影响

#### Scenario: 不同单位的相同槽位身份互异

- **WHEN** 两个单位各自分配桶内第 0 个状态实例
- **THEN** 两个句柄的 `Index` 可以相同，`Generation` MUST 互异；各自调用 `GetState(Handle)` 命中各自单位，按句柄移除其中一个 MUST NOT 影响另一个

#### Scenario: 新世界不重用旧状态身份

- **WHEN** 世界 A 建出状态实例后被拆解，世界 B 再分配同一下标
- **THEN** 新句柄的代际与 A 互异，A 的旧句柄在 B 查询返回 nullptr

### Requirement: per-unit 桶注册表与代际校验

TcsState MUST 提供 `FTcsStateRegistry`：**per-unit 桶**——`TMap<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>>`（`TUniquePtr` 间接层，`TMap` 扩容**不搬移桶地址**）。

桶 = 槽位数组（`TArray<FTcsStateInstance>`）+ 每槽代际数组 + 空闲槽栈。分配 MUST 优先复用空闲槽；每次分配的身份 MUST 遵守「状态实例句柄」需求的进程级唯一代际规则；释放将代际 +1 置为偶数，使旧句柄失效。MUST NOT 令各桶各自从代际 1 重新发放相同实例身份。

**脏句柄一律拒绝 + `Warning`**（不 `ensure`——这是**时序竞态**语义，与“配置错误”分开）：代际失配 / 下标越界 / 单位未注册。拒绝 MUST NOT 影响同桶其它实例。

`UnregisterUnit` 删桶时 MUST 回收该单位全部槽位（否则池占用统计残留）。

#### Scenario: 脏句柄被拒绝且不影响他人

- **WHEN** 对同一单位建两个实例后，用代际失配的旧句柄调移除
- **THEN** 拒绝该调用并留 Warning，两个实例仍在册（无实例被误移除），不出现 ensure

#### Scenario: 桶地址不随登记表增长失效

- **WHEN** 持续为不同单位建桶与实例（`TMap` 发生多次扩容）后，用早先单位的句柄查询
- **THEN** 查询照常命中（桶经 `TUniquePtr` 间接持有，登记表扩容不搬移桶地址）

#### Scenario: 单位销毁后桶被回收

- **WHEN** 对某单位建若干实例后调 `UnregisterUnit`
- **THEN** 该单位全部实例被移除、桶被删除，且用其旧句柄查询一律被拒绝

### Requirement: 状态生命周期事件

TcsState MUST 原生声明六枚框架事件 tag（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG_COMMENT`，常量带模块导出宏——裸 `extern` 会 `LNK2001`），词落在既有 `TcsEvent` 根下（**不新增根**）：

| 常量 | 词 | 广播时机 |
|---|---|---|
| `Tag_TcsEvent_State_Applied` | `TcsEvent.State.Applied` | 施加成功（新实例） |
| `Tag_TcsEvent_State_Refreshed` | `TcsEvent.State.Refreshed` | 重复施加命中同组成活实例（刷新） |
| `Tag_TcsEvent_State_StackChanged` | `TcsEvent.State.StackChanged` | 层数变化 |
| `Tag_TcsEvent_State_Expired` | `TcsEvent.State.Expired` | 到期（自然结束） |
| `Tag_TcsEvent_State_Removed` | `TcsEvent.State.Removed` | 显式移除 / 取消 / 单位注销 |
| `Tag_TcsEvent_State_Periodic` | `TcsEvent.State.Periodic` | 周期到点（需周期计时，归 Task 3） |

载荷 MUST 为 `FTcsStateEventPayload{ Handle, Unit, DefTag, Source, Instigator, Stacks, Level, Cause }`：`Unit` 为实例的被施加方，广播装配 MUST 从 `Instance.Unit` 填入；`Handle` / `Unit` / `DefTag` / `Source` / `Stacks` / `Level` 恒有效；`Cause`（`EStateRemoveCause`）**仅 `Expired` / `Removed` 有意义**，其余事件里无意义。`Source` 保留既有非反射运行态字段纪律，其余身份与标量为反射字段。

`TcsState` MUST 为该载荷登记触发读取器：`Caster` 取有效 `Instigator`，无效时取 `Unit`；主体身份取反射的 `FTcsStateHandle`。读取器与触发行的主体过滤契约归 `effect-trigger` 能力。

广播 MUST 走总线**立即通道**（`PublishImmediate`）——同一提交内到达，装置可在同一帧断言。

#### Scenario: 六枚事件词的归属与形态

- **WHEN** 检查六枚常量
- **THEN** 它们均为本模块原生声明、词形为 `TcsEvent.State.<名>`（3 段，落在既有 `TcsEvent` 根下），无新增根

#### Scenario: 载荷可经总线传递且字段可读

- **WHEN** 订阅者收到任一事件
- **THEN** 载荷为 `FTcsStateEventPayload`，可读到 `Handle` / `Unit` / `DefTag` / `Source` / `Stacks` / `Level`（`Cause` 只在 `Expired` / `Removed` 上有意义）

#### Scenario: 状态事件提供发起者与实例身份

- **WHEN** 读取状态生命周期载荷，发起者有效或无效
- **THEN** `Caster` 分别取发起者或被施加方 `Unit`，主体身份保持载荷中的实例 Handle；TcsEffect MUST NOT 依赖 TcsState 的类型

### Requirement: 广播点与阶段迁移

**施加的共存结果 MUST 恰好广播一枚生命周期事件**（`Applied` / `Refreshed` 二者之一；替换路径是"旧实例被移除 + 新实例被建出"两笔，不属于"同一条实例的共存结果"）：无同组实例 ⇒ `TcsEvent.State.Applied`；命中同组 ⇒ `TcsEvent.State.Refreshed`，层数变化时另有 `TcsEvent.State.StackChanged`。`ExpireState` ⇒ `Expired`（`Cause = Expired`）、`RemoveState` ⇒ `Removed`（`Cause` 由调用方给）。**周期到点**另有 `TcsEvent.State.Periodic`（周期条目的重复回调，载荷带当前 `Stacks` 与 `Level`）——它是**追加**语义，不替代任一生命周期事件。

**"同组"判据归 `state-stacking-policies` 能力**（组键 = `单位 + DefTag` + 轴附加项）——本能力只保证**共存结果与广播的对应关系**：新建 ⇒ `Applied`；同来源刷新 ⇒ `Refreshed`；异来源叠层 ⇒ `StackChanged` + `Refreshed`（前者先到）；替换 ⇒ `Removed`（旧）+ `Applied`（新）；满仓拒绝 ⇒ 无广播。事件面 MUST NOT 因策略不同而增减（除上述对应关系外）。

**阶段机**：`EStatePhase{ Inactive / Active / Expiring }`，合法迁移 `Inactive → Active`（施加成功）、`Active → Expiring`（进入移除流程）、`Expiring → Inactive`（槽位释放）、`Expiring → Active`（仍在册实例的刷新过渡态归位）；**非法迁移 = `ensure`**（配置错误语义，与脏句柄的竞态口径分开）。

**撤销顺序**：MUST 先广播后释放槽位——订阅者在回调里仍能 `GetState` 读到该实例（否则"收到移除事件却查不到实例"）。**该顺序的前置两步**（2026-10-05 由"一步"扩为"两步"）：进入撤销流程时 MUST 先（①）按实例的级联锚点摘除属性修正器**并退订该实例的施加期登记物**（内联触发行；行为 Fragment 的订阅退订与①同处，见 `state-behavior-fragments` 的「行为订阅的生命周期接线」），再（②）撤销该实例的到期与周期条目。①排在广播之前的意义 = **该实例自己的触发行 MUST NOT 看见自己的 `Removed` / `Expired`**（"状态在，行为就在"的对偶：状态走了，行为先走）；②的意义 = 条目持句柄，槽位复用后回调仍会到达——先撤条目才不产生无谓回调。

**接线顺序（施加侧的对偶）**：`Applied` 广播**之前** MUST 已完成修正器挂载、内联触发行登记与（若该 Def 配了行为 Fragment）行为订阅——订阅者在 `Applied` 回调里读到的属性已是挂载后的数值，且新登记的触发行**能看见自己的 `Applied`**。

`UnregisterUnit` MUST 对该单位每条在册实例逐条广播 `Removed` 后删桶。

**有限重入守卫（STAT-5）**：门面 MUST 仅在真正移除广播窗口将句柄登记到 InRemovalBroadcastHandles；递归 Remove 同句柄 false 静默，不重复广播或释放。MUST NOT 因任意 Expiring 判为正在移除（刷新也暂用此阶段）。既有清理顺序不改变，任意递归修改组合仍未保证。

**广播后身份重查**：Applied/Refreshed 回调可移除自身或注销单位；广播者 MUST 用值快照，之后按句柄重查，不访问旧指针、不恢复已释放实例、不补播失效实例。移除广播返回后同样只释放仍在册且身份一致的槽位。

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

#### Scenario: Applied 回调可移除自身

- **WHEN** Applied 订阅者按载荷句柄移除自己
- **THEN** 自移除成立，施加流程不再访问失效实例或重复清理，无额外红字

#### Scenario: Refreshed 自移除不被过渡阶段误拒

- **WHEN** Refreshed 回调移除处于刷新过渡态的实例
- **THEN** 自移除成立，刷新流程重查后不恢复已释放实例，无额外红字

#### Scenario: Removed 递归移除静默拒绝

- **WHEN** Removed 回调再次移除同一句柄
- **THEN** 递归调用 false 静默，外层广播和释放各一次，不重复归还槽位
