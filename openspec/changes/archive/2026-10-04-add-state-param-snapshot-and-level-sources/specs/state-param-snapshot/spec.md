## ADDED Requirements

### Requirement: 参数快照与其构建规则

TcsState MUST 提供参数快照类型：`FTcsParamSnapshotEntry{ FGameplayTag Key; double Value; FInstancedStruct SourceRef; }` 与 `FTcsParamSnapshot{ TArray<FTcsParamSnapshotEntry> Entries; }`，并 MUST 提供只读查询 `TryGetNumericParam(FGameplayTag Key, double& OutValue) const`（miss 返回 false 且 `OutValue` 内容未定义）。

`FTcsStateInstance` MUST 持一份快照（Task 2 的注释留白在此落地）。施加时 MUST 对 `Def.Params` 逐行求值并冻结：

- **覆盖优先**：`ApplyState` 的 `Overrides` 命中该行 `Key` ⇒ 取覆盖值；否则取 `Def.Base.Evaluate(Ctx)`；
- **求值上下文**：`ParamTable` = 施加方参数表（未提供 = 空）、`Subject` = 被施加方实体、`Instigator` = 发起者实体（无效时取被施加方）、`EffectiveLevel` = `Def.LevelBase`、`LevelProvider` = 门面登记的宿主等级读口（后两个字段见「等级来源与实体等级读口」需求）；
- **值约定在此写入点发生**：该行的 `ValueConvention` MUST 经 `FTcsValueConvention::ConvertToCanonical` 转换——**快照内恒为规范值**；
- **源引用位**：`SourceRef` MUST 存该行数值来源的副本（Debug 可见取值来源、Live 化零结构迁移）；
- **重建语义**：刷新路径 MUST 按同规则**重建快照**（新 payload 覆盖旧的）——实例生命周期内读快照、不重算。

#### Scenario: 施加瞬间冻结、之后不追溯

- **WHEN** 以某 `DefTag` 施加一个状态，随后改变该行数值来源会在别处读到的输入（如同一上下文里传入的参数表内容）
- **THEN** 快照内该键的值保持施加时刻求值得到的值（实例存续期内不重算）

#### Scenario: 覆盖值优先于定义默认

- **WHEN** 某行 `Key` 既在 `Def.Params` 里配了默认值，又在施加时经 `Overrides` 传入不同值
- **THEN** 快照内该键的值等于覆盖值（不是定义默认值）

#### Scenario: 值约定在写入点转规范值

- **WHEN** 某行配 `ValueConvention = VCF_Percent` 且书写值为 85
- **THEN** 快照内该键的值为 0.85（转换发生一次，读快照不再转）

#### Scenario: 刷新重建快照

- **WHEN** 对同一单位以同一 `DefTag` 再次施加并传入不同的覆盖值
- **THEN** 原实例的快照被重建为该次施加的结果（新值覆盖旧值，不是两份并存）

### Requirement: 参数快照的参数表读取适配器

TcsState MUST 提供 `FTcsStateParamTableReader`——把 `FTcsParamSnapshot` 当参数表暴露的读取适配器（把键查询转发到快照查询）。

它是**修正器物化与 `ParamRef` 源从快照取值的唯一入口**（R5 Task 4 的输入口）——消费方 MUST NOT 自行遍历快照条目去拼一套平行的键查找。

**实现形态 = 纯 C++ 类（持快照只读指针），本轮 MUST NOT 派生 `UObject`、也 MUST NOT 去实现 `ITcsParamTableReader`**：唯一的消费方（Task 4 的物化器）是 C++ 直调，而 `ITcsParamTableReader` 今天**不是 `Blueprintable`**（宿主脚本无法实现它）⇒ 为它造一个 `UObject` 壳属"零消费者预建"，且会把快照指针的寿命绑到 GC 上。它继承 `ITcsParamTableReader` 的时机 = 出现第一个需要"把快照当参数表挂在上下文上"的反射消费方。

#### Scenario: 读口命中与 miss

- **WHEN** 用一份含键 K（值 7）的快照构造读取适配器，分别以 K 与不含的键 M 调键查询
- **THEN** 前者返回 true 且输出 7，后者返回 false

#### Scenario: ParamRef 源经适配器从快照取值

- **WHEN** 以该适配器作上下文参数表，用 `FTcsParamSource_ParamRef{K, 兜底}` 求值
- **THEN** 返回快照内 K 的值（不是兜底值）

### Requirement: 等级来源与实体等级读口

TcsState MUST 提供宿主等级读口 `ITcsEntityLevelProvider`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) int32 GetEntityLevel(FTcsCombatEntityHandle Entity)`），并 MUST 提供四个等级类参数源（均 MUST 继承 `FTcsParamEnumerableSource` 并覆写 `GetIndexForLevel`，使"等级 → 档位下标"的唯一真相留在源内）：

| 源 | 等级来源 | 取值 |
|---|---|---|
| `FTcsParamSource_StateLevelArray{ TArray<double> Values }` | `Context.EffectiveLevel` | `Values[GetIndexForLevel(Level)]`；越界落最后一档 |
| `FTcsParamSource_StateLevelMap{ TMap<int32, double> Values }` | 同上 | 命中键即取；未命中落兜底 |
| `FTcsParamSource_InstigatorLevelArray{ TArray<double> Values }` | `Context.Instigator` → 门面登记的 `ITcsEntityLevelProvider` → level | 同数组源规则 |
| `FTcsParamSource_InstigatorLevelMap{ TMap<int32, double> Values }` | 同上 | 同映射源规则 |

`UTcsStateSubsystem` MUST 提供 `SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider>)` / `GetEntityLevelProvider()`，并以 `UPROPERTY` 持有注入的读口（GC 纪律）。**未注入 = `nullptr` 是配置状态、不是错误**——等级类源 MUST 落兜底路径（不崩溃、不 ensure、不向别的子系统反查）。

**读口不进 Core 上下文，走派生上下文**：等级读口的唯一消费者是本模块的四个等级源，而 `FTcsParamEvaluateContext` 住 `TcsCore`——把域读口塞进 Core 会让 Core 持有只有上层使用的类型（且日后每个域都想往那里塞自己的读口）。故 TcsState MUST 提供派生上下文 `FTcsStateEvaluateContext : FTcsParamEvaluateContext`，**唯一新增字段** `TScriptInterface<ITcsEntityLevelProvider> LevelProvider`，并按 PV-1 既有机制覆写类型标识；等级类源 MUST 以 `GetScriptStruct()->IsChildOf(FTcsStateEvaluateContext::StaticStruct())` 判定后取值，判定失败（收到基础上下文或他域上下文）落兜底。**先例 = 同族的 `FTcsAttributeEvaluateContext` 持 `ITcsAttributeProvider`**（PV-1 明文支持多层派生）。

**分界判据**（哪些字段进基础上下文、哪些进派生）：**跨域通用的主体身份**（`Subject` / `Instigator` / `EffectiveLevel`）进基础；**某域的读口**（属性读口、等级读口）进该域派生上下文。故 `Instigator` 由 `param-value` 能力补进基础上下文（它答"谁施加的"，任何域都会问），而 `LevelProvider` 住本派生上下文。

**取值归属**：`EffectiveLevel` 与 `Subject` / `Instigator` 一律由**装配上下文的调用方**填写，源 MUST NOT 自行反查实例或全局表。

#### Scenario: 数组源按等级取档

- **WHEN** 以 `Values = [10, 20, 30]` 的数组源，分别以 `EffectiveLevel` = 1 / 2 / 3 求值
- **THEN** 依次返回 10 / 20 / 30，且 `GetIndexForLevel` 与 `Evaluate` 落在同一档

#### Scenario: 越界落最后一档

- **WHEN** 同一数组源以超出档位数的等级求值
- **THEN** 返回最后一档的值（不回绕、不崩溃）

#### Scenario: 无等级语义时落兜底

- **WHEN** 上下文未填等级（`EffectiveLevel == 0`）或未填发起者（`Instigator.Id == 0`），等级类源求值
- **THEN** 落该源的兜底值，`GetIndexForLevel` 返回 `INDEX_NONE`，且不发生任何子系统反查

#### Scenario: 未注入等级读口时发起者等级源落兜底

- **WHEN** 未向门面登记 `ITcsEntityLevelProvider`，以发起者等级源求值
- **THEN** 落兜底值（未注入是配置状态，不留红字、不 ensure）

#### Scenario: 注入读口后按宿主给出的等级取档

- **WHEN** 向门面登记一个对某实体返回 2 的宿主读口，并以 `Instigator` = 该实体的发起者等级数组源求值
- **THEN** 返回值取自第 2 档

#### Scenario: 上下文类型不匹配时落兜底

- **WHEN** 以**基础** `FTcsParamEvaluateContext`（或他域派生上下文）求值一个等级类源
- **THEN** checked cast 判定为假，落该源兜底值（不崩溃、不 ensure、不误取他域字段）

### Requirement: 时值与周期语义（到期堆落点）

施加一个状态时 MUST 按下述规则挂时间条目（`EDurationPolicy` / `Period` / `PeriodRefresh` 的语义）：

- **`Finite`**：`DurationRemaining` MUST 由快照的 `DurationTime` 求值取得，并 MUST 入堆一条到期条目（到期时刻 = 时钟 `Elapsed` + 剩余时长）；堆的驱动归时钟泵，本层 MUST NOT 自建 tick。
- **`Infinite`**：MUST NOT 入堆（字段不使用）——**"≤0 = 永久"的魔法值语义不成立**。
- **`Period > 0`**：MUST 走**重复到期条目**——每次周期回调广播 `TcsEvent.State.Periodic`（载荷带当前 `Stacks` 与 `Level`）后**重新入堆**；默认**等首个周期**（"施加即生效"由 apply 响应链表达，本层不额外补一次）。
- **`PeriodRefresh`**：刷新路径上 `EPR_Keep` = 不动周期计时；`EPR_Reset` = 重建周期条目；`EPR_Immediate` = 立即按周期语义走一次再重置。
- **周期条目独立**：时值到期与周期到期 MUST 各持一个条目锚点（`ExpiryEntry` / `PeriodEntry`）——两者是可同时存在的两个语义，MUST NOT 挤在一个锚点里。

#### Scenario: 周期到点广播后继续计时

- **WHEN** 施加一个 `Finite + Period > 0` 的状态并让时钟推进跨过两个周期
- **THEN** `TcsEvent.State.Periodic` 被广播两次（`Stacks` / `Level` 可读），且该状态仍在册

#### Scenario: 到期广播 Expired 并回收

- **WHEN** 让时钟推进跨过 `DurationTime`
- **THEN** 与 `ExpireState` 同一条链（`Expired` 广播 + 槽位归还），实例随后查不到

#### Scenario: 无限时长不产生到期回调

- **WHEN** 施加一个 `EDP_Infinite` 的状态并让时钟推进任意时长
- **THEN** 不发生到期回调（无到期条目），实例仍在册

#### Scenario: 两种条目互不挤占

- **WHEN** 施加一个 `Finite + Period > 0` 且周期短于时值的状态
- **THEN** 周期回调正常重复发生、时值到期仍按其原定时刻到达（两条条目各自独立）

### Requirement: 时长操作的到期堆同步

`UTcsStateSubsystem::ExtendDuration(Handle, Delta)` / `SetRemaining(Handle, Seconds)` MUST 在更新剩余时长的同时**撤销旧条目并按新余量重新入堆**（句柄配对清理）。

- `EDP_Infinite` 上调二者 MUST 留 `Warning` 并**无操作**（无到期条目可调，属配置状态不是错误）；定义未登记时同样 `Warning` + 无操作（不猜策略）。
- 剩余时长被置为 0 或负值时 MUST 按 0 处理，并使其在下一个泵点到期。
- 已到期 / 已移除的句柄 MUST 按脏句柄口径拒绝（不影响同桶其它实例）。

#### Scenario: 延长后按新余量到期

- **WHEN** 对一个 `Finite` 实例调 `ExtendDuration` 后让时钟推进到原本的到期时刻之后、新到期时刻之前
- **THEN** 该实例仍在册（旧条目已被撤销，不发生过期回调）

#### Scenario: 缩短后提前到期

- **WHEN** 用 `SetRemaining` 把剩余时长设为小于当前值后推进时钟
- **THEN** 该实例在缩短后的时刻到期（`Expired`）

#### Scenario: 无限时长上调用被拒且不改字段

- **WHEN** 对 `EDP_Infinite` 的实例调 `ExtendDuration` / `SetRemaining`
- **THEN** 留 `Warning`，实例的时值字段与在册状态均不变

#### Scenario: 置零后在下一泵点到期

- **WHEN** 用 `SetRemaining(Handle, 0)` 把剩余时长置零
- **THEN** 该实例在下一个时钟泵点到期（不早于本帧的游戏逻辑，也不无限拖延）

### Requirement: 生命期与撤销顺序（条目面）

- `RemoveState` / `ExpireState` MUST 在进入撤销流程时**先撤销该实例的到期与周期条目**，再走既有的 `Expiring → 广播 → 归还槽位` 顺序——条目持句柄，槽位复用后回调仍会到达，先撤条目才不产生无谓回调。
- `UnregisterUnit` MUST 同样逐条撤销条目（它复用同一条移除链）。
- `UTcsStateSubsystem::Deinitialize` MUST 在清空桶之前撤销全部待撤销条目——MUST NOT 留跨世界残影。
- 到期回调 MUST 经 `UWorld` 弱引用与句柄代际校验：世界已拆解或句柄悬空时 MUST 静默丢弃（时序竞态语义，不留红字、不 ensure）。

#### Scenario: 世界反初始化撤销条目

- **WHEN** 某世界存在带到期条目的实例时该世界被反初始化
- **THEN** 这些条目被撤销（此后不再产生回调），桶与登记表被清空

#### Scenario: 移除后旧条目不再回调

- **WHEN** 移除一个带到期/周期条目的实例后推进时钟越过其原到期时刻
- **THEN** 不产生该实例的 `Expired` / `Periodic` 广播（条目已被撤销）

#### Scenario: 悬空回调被静默丢弃

- **WHEN** 某条目因任何原因仍到达回调而句柄已悬空（代际失配）
- **THEN** 回调静默返回，不留红字、不 ensure、不影响同桶其它实例
