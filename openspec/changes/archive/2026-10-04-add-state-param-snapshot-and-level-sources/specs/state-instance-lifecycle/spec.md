# Delta: state-instance-lifecycle

> **本 delta 的性质**：Task 2 已把"实例骨架"落进主规格；本 delta 只补**时间与数值两条腿**带来的四处口径变化（实例字段面 / 门面时长操作与等级读口 / 撤销顺序扩展 / 反初始化清空面）。
>
> **归档器硬约束（第三次实证，2026-10-04）**：MODIFIED 块里 `### Requirement:` 与 `#### Scenario:` 的**标题都不可改、不可删**——改名或删除会被 `validate` 拒为 `omits scenario(s)`，`## RENAMED Requirements` 写 `#### Scenario:` 会被**静默丢弃**。故下面每条 MODIFIED 均**原样保留全部既有场景标题**，新语义只以"新增场景 + 正文补句"承载。

## MODIFIED Requirements

### Requirement: 池化状态实例记录

TcsState MUST 提供 `FTcsStateInstance`——**纯数据**记录，字段分三组：

- **身份与归属**：`DefTag`（`FGameplayTag`）/ `Handle` / `Source`（`FTcsSourceHandle`，发号经统一发号器）/ `Instigator`（`FTcsCombatEntityHandle`）/ `Unit`（宿主单位——句柄里没有单位段，自持后 `GetState` 才是 O(1) 定位）；
- **数值与阶段**：`Stacks`（`int32`，从 1 起）/ `Level`（`int32`）/ `Phase`（`EStatePhase`）/ `ParamSnapshot`（`FTcsParamSnapshot`——施加瞬间冻结的生效数值，形状与构建规则见 `state-param-snapshot` 能力）；
- **时间**：`DurationRemaining` / `PeriodRemaining`（`double`）/ `ExpiryEntry`（时值到期条目锚点）/ `PeriodEntry`（周期到期条目锚点）。

`Source` MUST 在**每次施加**时经 `FTcsSourceHandleRegistry::Allocate()` 取新值（进程内唯一、永不复用）——它就是"同一个状态 = 同一个来源"的级联锚点（触发行退订与属性修正器摘除按它一次清干净）。

**时值到期与周期到期 MUST 各持一个条目锚点**（`ExpiryEntry` / `PeriodEntry`）：`Finite` 与 `Period > 0` 是可同时成立的两个语义，挤在一个锚点里会让"撤销旧条目"分不清撤的是哪一条。

**MUST NOT** 持有：策略（`FInstancedStruct` / `TInstancedStruct` 作配置载体）、事件载荷、订阅句柄、任何 `UObject` / `TWeakObjectPtr<UObject>`（D3-7 v2 与 D2-9 纪律：值语义记录是池化与后续操作复制的地基）。

**`ParamSnapshot` 内的 `SourceRef` 不构成本条的例外**：它是**已求值来源的副本**（值语义，非策略引用），作用是 Debug 可见取值来源与 Live 化零结构迁移；其内层若含对象引用，由门面的 `AddReferencedObjects` 统一补引用（GC 纪律，见 `state-param-snapshot` 能力）。

#### Scenario: 实例不持有策略与对象

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 无策略载体、无载荷、无订阅句柄、无 `UObject` 引用（策略归 Def 资产、载荷归事件、订阅句柄归注册表/总线）

#### Scenario: 两次施加得到互异的来源句柄

- **WHEN** 对同一单位连续施加同一 `DefTag` 两次（两次各自建出实例）
- **THEN** 两个实例的 `Source` 互异且均非 0

#### Scenario: 时值与周期各持条目锚点

- **WHEN** 检查一个 `Finite + Period > 0` 的在册实例
- **THEN** 它的 `ExpiryEntry` 与 `PeriodEntry` 是两个独立字段，各自可被独立撤销（互不挤占）

#### Scenario: 实例携带施加瞬间的数值快照

- **WHEN** 施加一个带参数行的状态后检查该实例
- **THEN** 它的 `ParamSnapshot` 含施加时刻求值得到的条目（不是空表，也不随外部输入变化）

### Requirement: 状态门面（施加、查询、移除与生命周期操作）

TcsState MUST 提供 `UTcsStateSubsystem : UWorldSubsystem`，**仅在 Game / PIE / GamePreview 世界实例化**（同族先例）；`Deinitialize` MUST 全量清理（登记表复位 + 全部桶清空 + 待撤销的到期条目撤销）——MUST NOT 留跨世界残影。

操作面：

- **施加**：`ApplyState(Target, DefTag, Source, Instigator, ParamTable, Overrides) -> EApplyResult`——施加方传入的 `Source` 为**可选**（无效则本门面发号）；`Instigator`（发起者实体）与 `ParamTable`（施加方参数表）为**可选**（`Instigator` 无效时取 `Target`、`ParamTable` 为空时引用类源落兜底）；`Overrides` 为施加方参数覆盖，参与快照构建（构造规则见 `state-param-snapshot` 能力）；
- **查询**：`GetState(Handle) -> const FTcsStateInstance*`（脏句柄返回 nullptr 且不 ensure）、`IsStateActive(Handle)`、`ForEachState(Target, TFunctionRef<bool(const FTcsStateInstance&)>)`——**按槽位下标升序稳定遍历**（同输入同输出，可复现验收的前提）；
- **移除**：`RemoveState(Handle, Cause)` / `ExpireState(Handle)`（= `Cause` 为 `Expired` 的移除）/ `UnregisterUnit(Target)`；
- **生命周期操作**：`ExtendDuration(Handle, Delta)` / `SetRemaining(Handle, Seconds)`——MUST 同步到期堆（撤销旧条目 + 按新余量重入堆）；**`Infinite` 时值策略**上调二者 MUST 留 `Warning` 并**无操作**（无到期条目可调）；
- **等级读口**：`SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider>)` / `GetEntityLevelProvider()`——宿主实现"这个实体几级"，注入物以 `UPROPERTY` 持有；**未注入 = `nullptr` 是配置状态不是错误**（等级类源落兜底）。

`ApplyState` 与 `RemoveState` / `GetState` 一类**本轮 MUST NOT 加 `UFUNCTION`**（句柄与 `TMap` 形参今天不可反射；脚本面整体留 `SCRIPT` 系列，登记台账）。

**返回值语义**：`EApplyResult{ Applied / Refreshed / Stacked / Rejected }`——本轮真实可达 `Applied` / `Refreshed` / `Rejected`；`Stacked` 归 Task 5（五轴堆叠），**拒绝原因的具名细分**归 R5.5-e（本轮只有 `Rejected` 一档 + 日志）。

#### Scenario: 非游戏世界不创建

- **WHEN** 在编辑器预览/检查器世界解析该子系统
- **THEN** 子系统不存在

#### Scenario: 世界反初始化确定性清空

- **WHEN** 某世界的状态门面建过实例后世界被反初始化
- **THEN** 登记表与全部桶被清空、待撤销的到期条目被撤销，旧句柄在新世界一律被拒绝

#### Scenario: 无限时长上调时长操作被拒

- **WHEN** 对 `EDP_Infinite` 的实例调 `ExtendDuration` / `SetRemaining`
- **THEN** 留 Warning 且实例的时值字段不变（无到期条目可调，属**配置状态**不是错误）

#### Scenario: 遍历稳定且可按谓词提前终止

- **WHEN** 某单位在册三个实例，调 `ForEachState` 并在访问两个后返回 false
- **THEN** 访问序按槽位下标升序、第三次访问不发生；重复调用得到同一顺序

#### Scenario: 未登记定义时施加被拒

- **WHEN** 用一个本世界未登记（或无法解析）的 `DefTag` 调 `ApplyState`
- **THEN** 返回 `EApplyResult::Rejected` 并留 Warning，单位桶不新增实例（MUST NOT 出现 ensure）

#### Scenario: 时长操作同步到期堆

- **WHEN** 对一个 `Finite` 实例调 `ExtendDuration` 后让时钟越过原本的到期时刻
- **THEN** 该实例仍在册（旧条目已被撤销并按新余量重入堆）

#### Scenario: 可选形参缺省时的施加路径

- **WHEN** 以未填 `Instigator` 与未提供 `ParamTable` 的形态调 `ApplyState`
- **THEN** 施加照常成功，且快照构建走"发起者取被施加方 + 引用类源落兜底"的缺省路径（不崩溃、不 ensure）

#### Scenario: 未注入等级读口不构成错误

- **WHEN** 未注入 `ITcsEntityLevelProvider` 时查询 `GetEntityLevelProvider`
- **THEN** 返回空（配置状态），门面不留红字

### Requirement: 广播点与阶段迁移

**施加的共存结果 MUST 恰好广播一枚事件**：无同组成活实例 ⇒ `TcsEvent.State.Applied`；命中同组 ⇒ `TcsEvent.State.Refreshed`（层数变化时另有 `StackChanged`）。`ExpireState` ⇒ `Expired`（`Cause = Expired`）、`RemoveState` ⇒ `Removed`（`Cause` 由调用方给）。**周期到点**另有 `TcsEvent.State.Periodic`（周期条目的重复回调，载荷带当前 `Stacks` 与 `Level`）——它是**追加**语义，不替代任一生命周期事件。

**本轮的"同组"判据 = 同单位 + 同 `DefTag`**（`GroupBy = None` 的语义）——它是 **Task 5 五轴共存决策的暂用位**，Task 5 MUST 在原处替换为策略驱动而不改变事件面。

**阶段机**：`EStatePhase{ Inactive / Active / Expiring }`，合法迁移 `Inactive → Active`（施加成功）、`Active → Expiring`（进入移除流程）、`Expiring → Inactive`（槽位释放）；**非法迁移 = `ensure`**（配置错误语义，与脏句柄的竞态口径分开）。

**撤销顺序**：MUST 先广播后释放槽位——订阅者在回调里仍能 `GetState` 读到该实例（否则"收到移除事件却查不到实例"）。**该顺序的前置一步**：进入撤销流程时 MUST 先撤销该实例的到期与周期条目（条目持句柄，槽位复用后回调仍会到达；先撤条目才不产生无谓回调）。

`UnregisterUnit` MUST 对该单位每条在册实例逐条广播 `Removed` 后删桶。

#### Scenario: 施加广播 Applied 且订阅者能在回调内查到实例

- **WHEN** 订阅 `TcsEvent.State.Applied` 后施加一个状态
- **THEN** 订阅者收到一笔该事件，且在回调内用载荷句柄调 `GetState` 能读到实例（广播早于释放，实例为 `Active`）

#### Scenario: 重复施加广播 Refreshed 而非 Applied

- **WHEN** 对同一单位以同一 `DefTag` 连续施加两次
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
