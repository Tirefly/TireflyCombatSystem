## MODIFIED Requirements

### Requirement: 六道具名门禁与激活入口

技能门面 MUST 提供 `TryActivate(FTcsCombatEntityHandle Entity, FGameplayTag DefTag, FTcsSkillInstanceHandle* OutRun) -> ESkillActivateResult`，且**门禁 MUST 逐道具名、MUST NOT 内联成一个笼统失败**（修 TCS 库外报告 09 的"门禁内联无钩子"缺陷）：

- 结果枚举 MUST 为 `UENUM(BlueprintType) ESkillActivateResult`，**值名带 `SAR_` 前缀**（全仓 `UENUM(BlueprintType)` 的枚举值无一例外带前缀；`BlueprintType` 是必需的——宿主查出"为什么放不出来"是本门禁的**唯一价值**）；
- **门禁序列（本 Task 评估序）** MUST 为：**① 实体 Ready → ② 已学 → ③ 冷却 → ④ Instancing → ⑤ CanAfford → ⑥ 定义可解析**，每道各有独立枚举值：
  `SAR_EntityNotReady` / `SAR_NotLearned` / `SAR_OnCooldown` / `SAR_AlreadyActive` / `SAR_CannotAfford` / `SAR_DefInvalid`，全过返回 `SAR_Success`；
- **第 ① 道 MUST 用 `ITcsEntityQuery::IsEntityReady`**（经 Task 2 的注入点），**MUST NOT** 用 `IsAlive` 代替（两者正交：前者答"框架能不能在它身上操作"、后者答"宿主认为它活着吗"）；未注入时按 Task 2 的既定语义**降级放行**；
- **第 ⑥ 道的评估时点 MUST 写明（MUST，关掉一处顺序歧义）**：第 ③④⑤ 道**均需读取定义内容**（冷却轨道 / 实例化方式与顶替位 / Cost 策略都是 Def 字段）⇒ 定义**不可解析时** MUST 立即返回 `SAR_DefInvalid`，且**不评估第 ③④⑤ 道**（它们不可求值）。枚举值的**次序**仍按上列六道序；本条约束的是**评估可达性**，不是重排门禁；
- **第 ③ 道（冷却）与第 ⑤ 道（CanAfford）在本 Task 为占位**：判据 = 冷却**轨道为空 ⇒ 恒可放**、Cost 策略取**不消耗档 ⇒ 恒可付**；**MUST NOT** 引用任何尚未定名的枚举值（`R6.5` 的定义尚在概念期，引用即制造第二真相）。两道 MUST 保持**可拒绝的结构**（各有自己的枚举值），只是本轮判据恒过；
- **拒绝路径 MUST NOT 有副作用**（纯查询语义）：任一道拒绝时 MUST NOT 建 run、MUST NOT 改账本、MUST NOT 起链、MUST NOT 发事件；
- **第 ④ 道 Instancing 四路分流**（`ECastInstancing` × `bRetriggerOnActive`）——**判定 MUST 经共享层的统一重申请决策接口**（`FTcsEntityAdmission`，见 `entity-runtime-mechanism` 能力的「统一重申请决策接口」需求），四档语义各由**默认策略类**表达，**MUST NOT 内联在激活算子里**（理由：该决策空间与状态侧的五轴堆叠决策**同型**，两处各写一份即漂移源）：
  - `CI_InstancePerExecution` ⇒ **并存**：不查在飞实例，直接建新 run（依据 GAS 原文与代码路径：每次激活新建实例、无在飞检查）；
  - `CI_InstancePerEntity` + **无在飞实例** ⇒ 正常建 run；
  - `CI_InstancePerEntity` + **有在飞实例** + `bRetriggerOnActive = false`（**默认**） ⇒ **驳回 `SAR_AlreadyActive`**（"该实体上已有在飞的同技能实例"——两个来源对调用方是同一件事，**MUST NOT** 拆成两个枚举值）；
  - `CI_InstancePerEntity` + **有在飞实例** + `bRetriggerOnActive = true` ⇒ **顶替**（见下条）；
  - **"在飞实例"的判据 MUST 取该条目的 `RunHandles`**（它按构造就是 per-entity，无需跨桶扫描）；**在飞判据 MUST 按句柄经桶复核**（账本里可能有已归还的陈旧句柄 ⇒ 复核失败即视为无在飞）；
- **顶替语义（MUST，刻意与 GAS 分歧）**：① 先查旧 run 当前时段的可打断性，**不可打断 ⇒ 驳回 `SAR_AlreadyActive`**（旧 run 原样继续）；② 可打断 ⇒ **终止旧 run**——发 `OnCastInterrupted`、**MUST NOT** 发 `OnCastCompleted`、**不起旧 run 的主链**、归还旧 run 桶槽位、摘除其 `RunHandles` 条目；③ 再建新 run。**顶替 = 打断的一种终止形态**，MUST NOT 新增第三枚终止事件；
  - **可打断性判据的 Task 3 → Task 5 接缝（MUST 写清）**：本 Task 取 `FTcsSkillDefData::bCancellableDefault`（即 `ECastQueryMode::DefSwitches` 档——**它是查询契约的默认档**，Task 1 已交付）。Task 5 落地 `CanBeCanceled()` 后改调它，而 **DefSwitches 档读的就是该字段 ⇒ 行为逐字不变、零回归**。本 Task **MUST NOT** 预建三档分派（`PhaseTable` 需时段推进、`Custom` 需 Fragment 求值）。

**★ 2026-10-16 `R5.5-e1` 改名与门禁扩展（本需求已现况化）**：（a）`bInterruptibleDefault` → **`bCancellableDefault`**、`IsInterruptibleNow()` → **`CanBeCanceled()`**（判据同 `skill-cast-phases` 能力：手段 vs 结果）；（b）门禁在**第 ⑥ 道之后新增一道关系门**（`R5.5-e1`，见 `state-relation-check` 能力）：**`ForbiddenTags` 命中 / `RequiredTags` 未满足 / 被存活实例的 `TagsToBlock` 覆盖** ⇒ 各自返回**具名枚举值**，MUST NOT 与第 ①~⑥ 道混同，且**拒绝路径零副作用**（同本条既有纪律：不建 run、不改账本、不起链、不发事件）。第 ①~⑥ 道的**次序与语义一字不改**；关系门是**追加的一道**，不是对既有道的重排。

#### Scenario: 六道拒绝各自可达且具名

- **WHEN** 分别构造六种拒绝条件（实体不可操作 / 未学该技能 / 冷却未就绪 / 已有在飞实例且不顶替 / 付不起 / 定义不可解析）各调用一次 `TryActivate`
- **THEN** 每次都返回**对应的**枚举值（六值互不混同），且**账本条目数与在飞 run 数均不变**（拒绝无副作用）

#### Scenario: 实例化两档的并存与驳回

- **WHEN** 同一技能先配 `InstancePerExecution` 连点两次，再改配 `InstancePerEntity` + `bRetriggerOnActive = false` 连点两次
- **THEN** 前者**两个 run 并存**（该技能在飞读数 = 2）；后者第二次返回 `SAR_AlreadyActive` 且**在飞读数仍为 1**

#### Scenario: 顶替发打断而不发完成

- **WHEN** `InstancePerEntity` + `bRetriggerOnActive = true` 且当前时段**可打断**，第二次激活触发顶替
- **THEN** 新 run 在飞（读数 = 1）、旧 run 发出 `OnCastInterrupted`、**`OnCastCompleted` 发出次数为零**——**判据 MUST 含"完成零次"**（只验"新 run 起来了"会漏掉 GAS 式误发，那正是本裁定的核心分歧点）

#### Scenario: 不可打断段位的顶替被拒（阴性对照）

- **WHEN** 同上但 `bCancellableDefault = false`，第二次激活
- **THEN** 返回 `SAR_AlreadyActive`，且旧 run **原样继续**（旧句柄仍可 `Resolve`、`RunHandles` 不变、`OnCastInterrupted` 未发）

#### Scenario: 定义不可解析时不给后续门禁假读数

- **WHEN** 某条目存在，但其 `DefTag` 的定义已被注销（条目授予后注销定义），随后 `TryActivate`
- **THEN** 返回 `SAR_DefInvalid`——**MUST NOT** 因第 ③④⑤ 道无法求值而落到其它枚举值，也 MUST NOT 建 run

#### Scenario: 四档分流不内联（本变更新增）

- **WHEN** 检查激活算子里对 `ECastInstancing` 与 `bRetriggerOnActive` 的处理
- **THEN** MUST 经 `FTcsEntityAdmission` 的策略类判定，MUST NOT 存在内联的档位分流；且四档的**行为读数与重构前逐字一致**（阴性对照：四态各取一次读数）

### Requirement: 施法事件四枚

`TcsSkill` MUST 原生声明施法生命周期事件四枚（**框架协议词汇，由插件原生声明**——事件广播面的契约词若由宿主配置，宿主漏配即静默破坏广播面）：

- 事件 Tag MUST 为 `TcsEvent.Cast.Started` / `TcsEvent.Cast.PhaseChanged` / `TcsEvent.Cast.Completed` / `TcsEvent.Cast.Interrupted`——**落在既有的 `TcsEvent` 根下**（域段 = `Cast`），**MUST NOT** 为此新增根；
- 变量声明 MUST 带**模块导出宏**（`extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_*`）：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无 `__declspec(dllexport)`）⇒ 跨模块引用必 `LNK2001`，而"供宿主订阅"正是本事件词汇的设计意图；
- 载荷 MUST 为**单一 `USTRUCT`**（`FTcsCastEventPayload`，四枚共用一种形状，同状态侧六枚共用 `FTcsBuffEventPayload` 的先例）：至少含 `FTcsSkillInstanceHandle Run` / `FTcsCombatEntityHandle Entity` / `FGameplayTag DefTag` / `int32 Level` / `FTcsSkillEntryHandle EntryHandle`，以及
  **`ECastFinishReason Reason`（2026-10-16 `R5.5-e1` 新增）**——载荷 MUST 携带**终结原因**。**判据**：`Interrupted` 与 `Replaced` **发同一枚事件**（`TcsEvent.Cast.Interrupted`），而 `ECastFinishReason` 自己的注释写明其存在理由是「两者对订阅者语义不同」；没有本字段时订阅者**只能靠「哪枚事件」反推**，无法区分「我被打断了」与「我被自己的下一次施法顶掉了」⇒ 该区分只在枚举注释里成立、在载荷面上不成立。四枚**共用同一形状**，故 `Started` / `PhaseChanged` 两枚按「尚未终结」填各自档位、字段**恒有值**——MUST NOT 让它变成「只有终结事件才填」的第二真相；
  - **`Run` 是句柄不是指针**：订阅者若要读运行态细节，在回调内用句柄复核——**"先广播后归还槽位"的纪律**保证那一刻 run 仍在册；
  - **句柄字段的反射性**：`FTcsSkillInstanceHandle` 已是 `USTRUCT(BlueprintType)`（Task 2 落地）⇒ 载荷可反射，无需二次展平；
- **四枚的产生者现已齐备（本变更落地后，MUST 逐枚写明，缺一即"声明了但不可达"）**：
  - `Tag_TcsEvent_Cast_Started` —— 激活成功后（Task 3 已落）；
  - `Tag_TcsEvent_Cast_Interrupted` —— **打断**与**顶替**两条终结路径（Task 3 只落了顶替）；
  - `Tag_TcsEvent_Cast_PhaseChanged` —— **时段推进**（本变更落地；**进入第 1 段不计**——激活时 `PhaseIndex` 已是 0，它是起点不是变化）；
  - `Tag_TcsEvent_Cast_Completed` —— **完成终结**（全时段走完，或**瞬发当刻**。本变更落地）。
    **★ 本条取代了原文的一句（MUST 留痕，防后人以为该句被误删；**原文逐字保留**）**：

    > - **本 Task 的产生者**：只发 `Tag_TcsEvent_Cast_Started`（激活成功后）与 `Tag_TcsEvent_Cast_Interrupted`（**顶替路径**）。`PhaseChanged` / `Completed` 两枚**只落词与载荷，产生者归 Task 5**（时段推进与自然终结）——**MUST NOT** 在本 Task 假装它们已产生；

    该句的**时态**（"本 Task"= Task 3）已随本轮落地而**自然到期**：Task 5 正是它所说的那个"归 Task 5"。⇒ 被取代的是"**两枚零产生者**"这一**当时事实**，**未被取代的是它的纪律**（**MUST NOT 假装已产生**）：本轮**真的**把两个产生者落了下来，故该读取面由"检查代码里没有广播"改为"**运行读数证明各自只在对应路径产生**"（见本需求末的成对读数要求与「未实现产生者的事件不假装已产生」scenario）；
- **"完成"与"打断" MUST NOT 互相冒充（MUST）**：任一路径 MUST 只发对应那一枚——**完成路径 MUST NOT 发 `Interrupted`**、**打断与顶替路径 MUST NOT 发 `Completed`**。判据：**每条路径的读数 MUST 成对取**（"该发的发了一次" + "**另一个零次**"）——只验前者会让"两枚都发"的实现全绿，而那正是本仓反复拦的形态（顶替曾差点照抄 GAS 的 `EndAbility(bWasCancelled = false)` 而误发完成）；
- 广播 MUST 走总线**立即通道**（`PublishImmediate`——同状态事件的既定口径，订阅方在调用返回前收到）；**总线订阅是精确 tag 匹配**（非层级）⇒ 订阅方 MUST 逐叶子具名，本 Task MUST NOT 依赖父 tag 收全。

#### Scenario: 激活发出开始事件且载荷身份完整

- **WHEN** 激活成功
- **THEN** `TcsEvent.Cast.Started` 被广播一次，其载荷的 `Run` / `Entity` / `DefTag` / `EntryHandle` 与该次激活一致

#### Scenario: 顶替发出打断事件而非完成事件

- **WHEN** 顶替路径被触发
- **THEN** `TcsEvent.Cast.Interrupted` 被广播一次（载荷 `Run` = **旧** run 的句柄），且 `TcsEvent.Cast.Completed` **零次**

#### Scenario: 未实现产生者的事件不假装已产生

> 场景标题里的「未实现产生者」是**建立时的轮次标签**（本仓裁定：`#### Scenario:` 标题在 MODIFIED
> 语义下**不可改名**，先例 = `archive/2026-10-04-add-tcs-state-module/specs/plugin-descriptor/spec.md:18`）
> ——本场景的**判据已随本轮现况化**：四枚产生者现已齐备，故"不假装已产生"的**当时**读法
> （"没有任何位置广播它们"）已被取代；**保留的判据**是它真正要防的那件事。

- **WHEN** 检查本轮落地后的代码路径与运行读数
- **THEN** `PhaseChanged` / `Completed` 两枚**确有产生者**（分别为时段推进与完成终结），且**各自只在对应路径上产生**：瞬发技能完成时 `Completed` **1 次** / `Interrupted` **0 次**；被打断时 `Interrupted` **1 次** / `Completed` **0 次**——**MUST NOT** 出现"事件已声明但无从产生"，也 **MUST NOT** 出现"两枚在同一个终结点上都发"

#### Scenario: 时段变化事件按推进次数产生

- **WHEN** 一个配三段的技能激活并推进到全部时段到期
- **THEN** `TcsEvent.Cast.PhaseChanged` **恰好 2 次**（进入第 2、3 段各一次；**进入第 1 段不计**），且第二次 `PhaseChanged` 早于唯一那次 `Completed`

#### Scenario: 终结点只发对应那一枚（成对读数）

- **WHEN** 分别取**完成**（瞬发当刻）与**打断**两个终结点各一次读数
- **THEN** 完成：`Completed` = 1 且 `Interrupted` = 0；打断：`Interrupted` = 1 且 `Completed` = 0——**报出 MUST 同时含两枚的计数**（只报"该发的那枚发了"无法区分"另一枚也发了"）
