## ADDED Requirements

> **术语订正（2026-10-14）**：本文原用「单位 / `Unit`」，该词已退役，现统一为「**实体 / `Entity`**」（见 `DEC-2026-10-14-entity-vocabulary`）。**正文为当时原话，保留不改。**

### Requirement: 冷却多轨运行态（起冷 / 判可放 / 改轨 / 到期驱动）

`TcsSkill` MUST 提供冷却的**运行态**（`FTcsCooldownOps`），使 `FTcsCooldownPolicy` 的 `Tracks`
数组成为真的计时线。**判据的唯一实现住本族函数**：门禁③（激活序列）与三个提交点共用同一份，
**MUST NOT 各写一份**。

- **起轨** `StartCooldownOnEntry(Subsystem, Unit, EntryHandle)`：
  - 逐轨取 `Duration` → 乘 CDR → 算 `EndTimeSeconds = Clock.GetClock().Elapsed + EffectiveDuration`
    → `PushExpiry`；返回**实际起的轨道条数**；
  - **CDR 快照于此刻**（`D5-15`："快照于冷却触发时刻，中途变化不追溯"）——
    开头取**一次**并作用于**全部**轨道（取多次会让同一批轨道拿到不同系数）；
  - **定位 MUST 靠 lambda 捕获**（`[WeakSubsystem, Unit, EntryHandle, TrackIndex]`），
    `OwnerId` **只作回调归属读数**——**MUST NOT** 把定位职责压给 `OwnerId`
    （那会与既有堆使用约定造出第二真相：同一个堆、两种回调约定）；
  - **组轨写单位级共享槽**（`GroupTag` 有效时）；**同组已有活槽 ⇒ 复用、不重起、不叠加**
    （重起会让"放完 A 立刻放 B"把 GCD 刷满，而 GCD 的意义恰恰是不许这样做）；
  - **零时长轨道视为"不起"**（不入一个立刻到期的条目——它只会在下一帧空跑一次）；
  - 条目悬空 / 定义取不到 / 时钟不可得 ⇒ 返回 0 并如实报出（**不 ensure**——时序或设施缺席）。
- **判可放** `IsEntryOnCooldown(...)`（`D5-15` 原文："**可施放 = 全部自身轨道清空 + 所属组槽清空**"）：
  - 写成**一条短路遍历**：任一自身轨未清 **或** 任一所属组槽未清 ⇒ `true`；
  - **判据 MUST 是"`EndTimeSeconds > Now`"（时轴上的事实），MUST NOT 是"`ExpiryEntry` 有效"**：
    只看句柄的话，一旦某条到期条目被别的路径撤掉而状态没清，该轨会**永久堵死**
    （症状 = 这个技能再也放不出来，且没有任何日志）；
  - **边界（MUST 处理，不得崩）**：`Tracks` 为空 ⇒ **恒可放**；`GroupTag` 无效 ⇒ 只算自身轨；
    条目状态数组比定义短（定义被热改）⇒ 缺的轨道按"未在跑"算，**MUST NOT 越界读**；
    组槽存在但已过期 ⇒ 不再阻拦；
  - **时钟不可得 ⇒ 判为不阻拦**（与"未注入实体查询 ⇒ 降级放行"同口径：设施缺席 MUST NOT 把一切拦死）。
- **CDR** `EvaluateCooldownPct(...)`：取 `TcsStateParam.CooldownPct` 契约键上的折叠结果；
  - **钳制上下界写死为 `[0, 1]`**（**MUST NOT** 写"合理处理"）：负值 ⇒ `0`
    （**负的 CDR 不得变成冷却延长**——那是"配了个负减免反而被罚"的静默怪相）；
    `> 1` ⇒ `1`（**不得产生负时长**）；非有限值（NaN/Inf）⇒ 记 `Warning` 并按 `0`（MUST NOT 静默）；
  - **生效式**：`EffectiveDuration = Duration × (1 - CDR)` ⇒ `0.3` = 时长打七折、`1` = 冷却为 0；
  - 该键**未命中 ⇒ CDR = 0**（无缩减），零日志（多数技能不会有 CDR 修正）。
- **改轨** `AdjustTrack(...)`：`Delta` 作用于**剩余**（正 = 延长、负 = 缩短）；
  **缩短到 `<= 0` ⇒ 等于清轨**（复用清轨路径，而非入一个立刻到期的条目）；
  `Total` **只增不减**（维持 `Remaining <= Total` 不变量——否则 UI 径向会画出超过 100% 的弧）；
  - **★ 改完 MUST 重挂到期堆**（`CancelExpiry` 旧的 + `PushExpiry` 新的）：
    **MUST NOT** 只改 `EndTimeSeconds` 而不重挂——堆里的旧到期时刻会继续生效，
    症状是"改了却不生效"或"提前到期"（两个方向都可能，取决于调整方向）。
- **清轨** `ClearTrack(...)`：撤锚点 + 状态**整体复位**（`= FTcsCooldownTrackState()`，非逐字段）。
- **到期回调** `ExpireTrack(...)` / `ExpireGroup(...)`：
  - **★ 第一条语句即把该轨状态整体复位**——这一步同时兑现两条纪律，**MUST NOT 拆开或挪到后面**：
    ① **防 `ensure` 红字**：`CancelExpiry` 对**悬空**句柄是 `ensureMsgf`，而"到期回调正在跑"那一刻
    它自己的条目**仍然有效**（`FireDueItem` 在回调**返回之后**才释放）⇒ 回调内若再撤本句柄就是
    "取消一个正在跑的条目"；复位后 `IsValid()` 为假，后续清理路径天然跳过
    （**同款先例** = `FTcsCastRun::PhaseExpiryEntry` 的既有注释、`FTcsCastOps::AdvancePhase` 的首条语句）；
    ② **池槽复用纪律**："整体重置而非逐字段赋值"——将来给该结构加字段时自动是干净的；
  - 组轨到期 = **整槽移除**（不需要逐条目遍历——这正是"组槽住单位"的收益）；
  - **无需额外的"允许再次激活"状态**：门禁③ 每次现算，不存在缓存态要清。
- **生命周期配对**（本仓"外部资源成对"纪律的落点）：
  - `ReleaseEntryCooldowns(...)` / `ReleaseUnitCooldowns(...)` MUST 存在——
    冷却是本仓**第一个在账本条目上挂外部资源**的机制（到期堆条目 + 单位级组槽），
    而账本条目此前的契约是"零外部资源 ⇒ 直接删桶即可"；该前提**已失效**；
  - 撤销路径 MUST 在**释放槽位之前**调用配对清理（槽位一放，条目内容整体重置 ⇒ 锚点找不回来）；
  - **组槽刻意不随条目撤销而摘**：组槽是**单位级**的、有自己的到期条目，会自行解除；
    替它解除会**替同组的其它技能**解除公共冷却（症状 = 撤销 A 之后同组的 B 立刻能放，而 GCD 本该还在）。
    ⇒ 组槽的生命周期归 `ExpireGroup` 与 `ReleaseUnitCooldowns`，**MUST NOT 在条目级越界清理**。

#### Scenario: 空 Tracks 恒可放（阴性对照）

- **WHEN** 某技能的定义**不含任何冷却轨道**，在其激活后查询 `IsOnCooldown`
- **THEN** 返回 `false`，且该技能能再次成功激活（**MUST NOT** 被门禁③ 拒）
- **判据的作用**：先证明"没有轨道的技能放得出来"，后面所有"冷却中 ⇒ 拒"才不是假绿灯
  （否则一个"恒拒"的实现会让全部正面判据都通过）

#### Scenario: 自身轨起冷后门禁③ 拒

- **WHEN** 一个配了单条自身轨（`GroupTag` 无效、时长 1.0s）的瞬发技能被激活成功，随即**再次**激活
- **THEN** 第二次返回 `SAR_OnCooldown`；`GetCooldownRemaining(...) > 0` 且 `<= GetCooldownTotal(...)`，
  且 `Total` 等于配置时长；`GetCooldownTrackCount(...)` 等于定义的轨道数

#### Scenario: 组轨跨条目共享（真 GCD）

- **WHEN** 两个*不同*的已学技能配了**同一个** `GroupTag`，其中 A 起冷
- **THEN** **B 也被拒**（`SAR_OnCooldown`），且两者查询到的剩余时长相**等**
- **判据**：组槽若被错写成"每条目各一份"，同组两个条目会各起各的线 ⇒ B 能放出来，本条即失败

#### Scenario: CDR 快照于触发时刻

- **WHEN** 一条 1.0s 的轨道配了 `CooldownPct = 0.5`，技能被激活
- **THEN** `GetCooldownTotal(...)` 为 **0.5**（`1.0 × (1 − 0.5)`）；起冷之后修改该键**不改变**本次总时长

#### Scenario: 到期后恢复可放

- **WHEN** 一条 1.0s 自身轨的冷却在**战斗时钟**上自然到期
- **THEN** 该技能重新可激活成功（**MUST NOT** 留下"永不恢复"的状态）

#### Scenario: 改轨重挂到期堆

- **WHEN** 对一条在跑的轨道调用 `AdjustCooldown(...)` 延长剩余
- **THEN** 新的剩余**立即**生效（读数随之改变），且**不出现"提前到期"**——判据 = 堆里的旧时刻已被撤销

### Requirement: 冷却的终结侧触发点与时机分派

冷却的 `ETcsCooldownTiming` MUST 接到**四路终结的唯一汇聚点**上：

- **挂点 = `FTcsCastOps::FinishCastRun`**（末段走完 / 瞬发当刻 / 打断 / 顶替四路**全部汇聚**到它，
  且其签名自带 `ECastFinishReason Reason`）⇒ **一处分派即可覆盖四路**。
  **MUST NOT** 去四个调用点各挂一份（那是"同一件事两个入口"，必然漂移）；
- **分派表**（`ECastFinishReason` 只有三档，且**瞬发没有自己的档**——瞬发走 `ECFR_Completed`）：

  | `Reason` | 起哪一档 |
  |---|---|
  | `ECFR_Interrupted` | `ECT_OnCastInterrupted`（真打断） |
  | `ECFR_Replaced` | `ECT_OnCastInterrupted`（**顶替 = 打断的一种终止形态**） |
  | `ECFR_Completed` | `ECT_OnCastCompleted`（**含瞬发**） |

- **`ECT_Manual` 在终结路径 MUST 零动作**（含日志）：它的定义就是"宿主自己调 `StartCooldown`"，
  框架做任何事都违背它；
- **触发点次序 MUST 排在"按 `RunSource` 回收"之后、归还池槽位之前**：
  - **之后**：回收会**广播属性变更事件**（M2 批提交重算 + 广播），排在回收之前会让
    回收尚未完成就可能触发新动作、且新条目与回收交错；**同族对照** = ⓪ 段（完成档起链）
    必须排在回收**之前**（否则新起的链挂的条目**当场被自己的终结例程摘掉**）
    ⇒ **两处次序相反、判据相同："别让自己的动作被自己的清理吃掉"**；
  - **之前（归还槽位）**：广播纪律是"先广播、后归还槽位"⇒ 订阅者拿到的载荷里若含 run 句柄，
    那一刻它 MUST 仍在册；
- **定义 MUST 在回收之后重新解析**：回收会广播，订阅者可在其中**注销技能定义**，
  而登记表是 `TUniquePtr` 持有、**注销即析构** ⇒ 沿用更早的 `Def` 指针是 use-after-free。

#### Scenario: 瞬发技能也按完成档起冷

- **WHEN** 一个**瞬发**（空 `Phases`）技能配 `ECT_OnCastCompleted` + 一条冷却轨，被激活
- **THEN** 激活当刻完成后该技能**进冷却**（`IsOnCooldown` 为真）
- **判据**：瞬发**没有时段推进**，若照"时段走完"接就永远不会触发——而它走的是
  `ECFR_Completed` ⇒ 挂在 `FinishCastRun` 上的分派**天然覆盖**它，**不需要额外挂点**

#### Scenario: 打断与顶替都走打断档

- **WHEN** 一个配 `ECT_OnCastInterrupted` 的技能分别被 `CancelCast` 打断、被同类技能顶替
- **THEN** **两种情形都起冷**（顶替是打断的一种终止形态）；而配 `ECT_OnCastCompleted` 的技能
  在被顶替时**不起冷**

#### Scenario: Manual 档终结路径零动作

- **WHEN** 一个配 `ECT_Manual` 的技能完成一次施法
- **THEN** 终结后该技能**没有冷却**（`IsOnCooldown` 为假）——框架 MUST NOT 替它起冷

### Requirement: 冷却事件（三枚）与可变性 API 成对

`TcsSkill` MUST 提供三枚冷却事件，且**与可变性 API 成对**（`MEM-20260902-12` 的核心判据：
有该状态改变就 MUST 有对应事件，反之亦然）：

| Tag | 产生者 |
|---|---|
| `TcsEvent.Cast.CooldownStarted` | 起冷成功（自动档与 `ECT_Manual` 档**同一条路径**） |
| `TcsEvent.Cast.CooldownUpdated` | `AdjustCooldown` / `ResetCooldown` |
| `TcsEvent.Cast.CooldownEnded` | 到期堆回调（**权威侧**——到期是时轴上的事实，不是"某个调用者说结束了"） |

- 三枚 MUST **共用一种载荷形状** `FTcsCooldownEventPayload`
  （`{EntryHandle, Unit, DefTag, TrackIndex, GroupTag, Total, Remaining, EndTimeSeconds}`）——
  订阅者拿到的永远是"哪个单位的哪条技能的哪条轨道"，各写一个 struct 会让订阅侧复制三份解析代码
  （先例 = `FTcsCastEventPayload` 的四枚共用）；
- **`D5-16` 的 `SkillId` 字段 MUST 用 `DefTag` 取代**（**MUST NOT 为它发明一个类型**）：
  `D5-9` 修订后 `EffectiveDefId` 已整体移除，"这个条目用哪份定义"的唯一答案就是 `DefTag`；
- **命名 MUST 去掉 `On` 前缀**（`D5-16` 原文写 `OnCooldownStarted` 等）：
  `TcsEvent.Cast` 根下既有的四枚是 `Started` / `PhaseChanged` / `Completed` / `Interrupted`
  （**零 `On` 前缀**）⇒ 按**同族对称**取名。**MUST NOT 照抄 `On*` 进 tag 名**
  （那会让同一根下两种命名风格并存）；
- **根数 MUST NOT 增加**：三枚落既有 `TcsEvent` 根（域段 `Cast`）——"增根不增层"；
- **导出宏**：三枚皆 MUST 为 `extern TCSSKILL_API FNativeGameplayTag`
  （`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`** ⇒ 跨模块引用会 `LNK2001`）；
- **一次起冷发一枚 `CooldownStarted`**（**MUST NOT** 逐轨发 N 枚）：订阅者的语义是
  "**这个技能**进冷却了"，不是"第 N 条轨进冷却了"；逐轨发 N 枚会让最简单的单轨用例收到 N 次
  而信息量完全相同。载荷的轨道读数取**第一条实际起的轨**，逐轨读数经查询口按 `TrackIndex` 取；
- **起冷事件 MUST 排在全部轨道写完槽之后**：订阅者在回调里查 `IsOnCooldown` 时应当能看到
  **完整**的冷却状态，而不是"第一条轨已起、后两条还没起"的中间态；
- **组轨到期事件 MUST 按"配了本组轨道的每个条目"各发一枚**：组槽是**单位级**的、
  **不记"谁起的它"**，而订阅者需要**自己的条目身份**才能刷新对应 UI 行
  （只发一枚无身份的组事件会让订阅者无从知道该更新哪一行）——这是"组槽住单位"的已知代价；
- **广播走立即通道**：冷却状态是"查询面可验证的事实"，发事件若走帧末，
  订阅者会在"事件还没到、但查询已经说在冷却"的窗口里看到不自洽的读数；
- **时钟不可得 / 世界无总线 ⇒ 静默跳过**（不 ensure、不红字——时序或设施缺席，非契约违规）。

#### Scenario: 一次起冷恰发一枚 `CooldownStarted` 且载荷身份吻合

- **WHEN** 一个配了**多条**轨道的技能起冷，订阅者计数
- **THEN** `CooldownStarted` 恰 **1** 次；载荷的 `EntryHandle` 与 `Unit` 与该条目一致

#### Scenario: 调轨与清轨各发 `CooldownUpdated`

- **WHEN** 依次调用 `AdjustCooldown(...)` 与 `ResetCooldown(...)`
- **THEN** 两次都发 `CooldownUpdated`，且调轨那次的 `Remaining` 与新值一致

#### Scenario: 到期发 `CooldownEnded` 且 `Remaining` 为 0

- **WHEN** 一条自身轨在战斗时钟上到期
- **THEN** 发出 `CooldownEnded`，其 `Remaining` 为 **0**（"剩余时间到点了"就是它的定义）

#### Scenario: 事件与可变性成对（无遗漏）

- **WHEN** 检查冷却侧的全部状态改变路径（起冷 / 调轨 / 清轨 / 到期）
- **THEN** 每条路径都有对应事件；且**不存在**"有事件但无对应状态改变"或反过来的情形

