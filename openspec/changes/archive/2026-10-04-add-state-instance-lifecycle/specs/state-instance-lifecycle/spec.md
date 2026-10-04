## ADDED Requirements

### Requirement: 状态实例句柄

TcsState MUST 提供 `FTcsStateHandle`（`USTRUCT(BlueprintType)`）：两字段 `int32 Index` 与 `int32 Generation`（**展平形态**，与 `FTcsChainRunHandle` / `FTcsEffectTriggerInstance` 同款——反射与宿主脚本往返需要可反射的标量字段，对象身份不外传）。

- `IsValid()`：`Index >= 0` 且 `Generation > 0`（`Generation` 从 1 起，0 恒为"从未分配"）；
- 相等比较按 `(Index, Generation)` 全等；MUST 提供 `GetTypeHash`（供 `TMap` / `TSet` 键控）；
- **句柄相对其发放方（该世界的状态门面）有义**：`Index` / `Generation` 的语义由**桶**定义；MUST NOT 把句柄跨世界或跨门面重用。

#### Scenario: 未分配句柄无效

- **WHEN** 取一个默认构造的 `FTcsStateHandle` 并调 `IsValid`
- **THEN** 返回 false（`Index` 为负或 `Generation` 为 0）

#### Scenario: 分配出的句柄可直接判有效

- **WHEN** 施加一个状态并取回其句柄（`Index` 可能为 0）
- **THEN** `IsValid` 返回 true（合法性判据是"`Generation` 非 0"，不是"下标非 0"——下标 0 是合法槽位）

#### Scenario: 用过的句柄在槽位回收后失配

- **WHEN** 移除一个状态使槽位回收，随后用旧句柄查询
- **THEN** 查询被拒绝（代际失配），且同一槽位新分配的实例不受影响

### Requirement: 池化状态实例记录

TcsState MUST 提供 `FTcsStateInstance`——**纯数据**记录，字段分三组：

- **身份与归属**：`DefTag`（`FGameplayTag`）/ `Handle` / `Source`（`FTcsSourceHandle`，发号经统一发号器）/ `Instigator`（`FTcsCombatEntityHandle`）；
- **数值与阶段**：`Stacks`（`int32`，从 1 起）/ `Level`（`int32`）/ `Phase`（`EStatePhase`）；
- **时间**：`DurationRemaining` / `PeriodRemaining`（`double`）/ `ExpiryEntry`（到期条目锚点）。

`Source` MUST 在**每次施加**时经 `FTcsSourceHandleRegistry::Allocate()` 取新值（进程内唯一、永不复用）——它就是"同一个状态 = 同一个来源"的级联锚点（触发行退订与属性修正器摘除按它一次清干净）。

**MUST NOT** 持有：策略（`FInstancedStruct` / `TInstancedStruct`）、事件载荷、订阅句柄、任何 `UObject` / `TWeakObjectPtr<UObject>`（D3-7 v2 与 D2-9 纪律：值语义记录是池化与后续操作复制的地基）。

#### Scenario: 实例不持有策略与对象

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 无策略载体、无载荷、无订阅句柄、无 `UObject` 引用（策略归 Def 资产、载荷归事件、订阅句柄归注册表/总线）

#### Scenario: 两次施加得到互异的来源句柄

- **WHEN** 对同一单位连续施加同一 `DefTag` 两次（两次各自建出实例）
- **THEN** 两个实例的 `Source` 互异且均非 0

### Requirement: per-unit 桶注册表与代际校验

TcsState MUST 提供 `FTcsStateRegistry`：**per-unit 桶**——`TMap<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>>`（`TUniquePtr` 间接层，`TMap` 扩容**不搬移桶地址**）。

桶 = 槽位数组（`TArray<FTcsStateInstance>`）+ 每槽代际数组 + 空闲槽栈（照 `FTcsTriggerRegistry` 的手法：分配优先复用空闲槽、释放使代际 +1、新槽代际从 1 起）。

**脏句柄一律拒绝 + `Warning`**（不 `ensure`——这是**时序竞态**语义，与"配置错误"分开）：代际失配 / 下标越界 / 单位未注册。拒绝 MUST NOT 影响同桶其它实例。

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

### Requirement: 状态门面（施加、查询、移除与生命周期操作）

TcsState MUST 提供 `UTcsStateSubsystem : UWorldSubsystem`，**仅在 Game / PIE / GamePreview 世界实例化**（同族先例）；`Deinitialize` MUST 全量清理（登记表复位 + 全部桶清空 + 待撤销的到期条目撤销）——MUST NOT 留跨世界残影。

操作面：

- **施加**：`ApplyState(Target, DefTag, Source, Overrides) -> EApplyResult`——施加方传入的 `Source` 为**可选**（无效则本门面发号）；`Overrides` 为施加方参数覆盖（本轮只落形参，快照求值归 Task 3）；
- **查询**：`GetState(Handle) -> const FTcsStateInstance*`（脏句柄返回 nullptr 且不 ensure）、`IsStateActive(Handle)`、`ForEachState(Target, TFunctionRef<bool(const FTcsStateInstance&)>)`——**按槽位下标升序稳定遍历**（同输入同输出，可复现验收的前提）；
- **移除**：`RemoveState(Handle, Cause)` / `ExpireState(Handle)`（= `Cause` 为 `Expired` 的移除）/ `UnregisterUnit(Target)`；
- **生命周期操作**：`ExtendDuration(Handle, Delta)` / `SetRemaining(Handle, Seconds)`——**`Infinite` 时值策略**上调二者 MUST 留 `Warning` 并**无操作**（无到期条目可调）；本轮二者只做到"字段落点 + 拒绝面"，到期堆同步归 Task 3。

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

### Requirement: 状态定义到运行态的登记口

`UTcsStateSubsystem` MUST 提供**定义登记口**：`RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef& Def)` / `UnregisterStateDef(FGameplayTag)` / `GetRegisteredStateDef(FGameplayTag) -> const FTcsBuffDef*`。

**为什么是登记口而不是反向依赖**：定义库（`UTcsDefinitionSubsystem`）住 `TcsIntegration`，而 `TcsIntegration` **依赖** `TcsState`——若状态门面去 include `TcsIntegration` 反查定义库即成环。故依赖方向 MUST 保持单向：**定义库把定义写进门的登记口**（同款先例 = 定义库把触发行写进 `UTcsEffectSubsystem::RegisterTriggerRow`）。

**为什么身份是形参**：身份归**资产**声明（`UTcsStateDef::DefTag`——资产身份、`GetPrimaryAssetId()` 的取值来源、作者期校验对象），`FTcsBuffDef` 是**定义内容**；若为"从内容里读身份"再往数据 struct 加一份 `DefTag`，策划就得在同一个资产里把同一个 tag 手填两遍 = **双真相**。故运行期的身份由登记口一次对齐（调用方 = 定义库，其缓存键本就是 `DefTag`）。

登记表 MUST **自持定义副本**（`TUniquePtr` 持有使 `GetRegisteredStateDef` 返回的指针地址稳定），且 `DefTag` 无效 / 重复登记 MUST 被拒绝并留 Error（不静默覆写）。

#### Scenario: 定义登记后可被施加

- **WHEN** 把某状态定义登记进该世界的状态门面，随后以该 `DefTag` 施加
- **THEN** 施加成功（`Applied`），取回的实例 `DefTag` 与登记时一致

#### Scenario: 重复登记被拒

- **WHEN** 同一 `DefTag` 被登记两次
- **THEN** 第二次被拒绝并留 Error，第一次的定义仍可解析（不静默覆写）

#### Scenario: 未登记的定义解析为 nullptr

- **WHEN** 以未登记过的 `DefTag` 调 `GetRegisteredStateDef`
- **THEN** 返回 nullptr（正常查询路径，不 ensure）

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

载荷 MUST 为纯反射数据 `FTcsStateEventPayload{ Handle, DefTag, Source, Instigator, Stacks, Level, Cause }`：`Handle` / `DefTag` / `Source` / `Stacks` / `Level` 恒有效；`Cause`（`EStateRemoveCause`）**仅 `Expired` / `Removed` 有意义**，其余事件里无意义。

广播 MUST 走总线**立即通道**（`PublishImmediate`）——同一提交内到达，装置可在同一帧断言。

#### Scenario: 六枚事件词的归属与形态

- **WHEN** 检查六枚常量
- **THEN** 它们均为本模块原生声明、词形为 `TcsEvent.State.<名>`（3 段，落在既有 `TcsEvent` 根下），无新增根

#### Scenario: 载荷可经总线传递且字段可读

- **WHEN** 订阅者收到任一事件
- **THEN** 载荷为 `FTcsStateEventPayload`，可读到 `Handle` / `DefTag` / `Source` / `Stacks` / `Level`（`Cause` 只在 `Expired` / `Removed` 上有意义）

### Requirement: 广播点与阶段迁移

**施加的共存结果 MUST 恰好广播一枚事件**：无同组成活实例 ⇒ `TcsEvent.State.Applied`；命中同组 ⇒ `TcsEvent.State.Refreshed`（层数变化时另有 `StackChanged`）。`ExpireState` ⇒ `Expired`（`Cause = Expired`）、`RemoveState` ⇒ `Removed`（`Cause` 由调用方给）。

**本轮的"同组"判据 = 同单位 + 同 `DefTag`**（`GroupBy = None` 的语义）——它是 **Task 5 五轴共存决策的暂用位**，Task 5 MUST 在原处替换为策略驱动而不改变事件面。

**阶段机**：`EStatePhase{ Inactive / Active / Expiring }`，合法迁移 `Inactive → Active`（施加成功）、`Active → Expiring`（进入移除流程）、`Expiring → Inactive`（槽位释放）；**非法迁移 = `ensure`**（配置错误语义，与脏句柄的竞态口径分开）。

**撤销顺序**：MUST 先广播后释放槽位——订阅者在回调里仍能 `GetState` 读到该实例（否则"收到移除事件却查不到实例"）。

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
