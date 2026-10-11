## MODIFIED Requirements

### Requirement: 施法运行态与实例池

`TcsSkill` MUST 提供**施法运行态**——一次激活所产生的全部运行数据，且它 MUST 池化（复用 `TTcsInstancePool` 的既有机制，**MUST NOT** 新建池实现）：

- 运行态类型 MUST 为 `FTcsCastRun`，**非反射**（无 `USTRUCT`——池内池化纯数据，本轮零过网与零脚本消费面；同 `FTcsChainRun` / `FTcsLearnedSkillEntry` 的处置与理由）；
- MUST 提供句柄标签 `FTcsCastRunTag` 与池实例 `TTcsInstancePool<FTcsCastRun, FTcsCastRunTag>`；池 MUST 以 `UPROPERTY` 之外的方式**由门面持有**并在 `Deinitialize` 确定性清空（同状态门面对注册表的清理口径）；
- `FTcsCastRunHandle` MUST 补 `GetInner()` / `SetInner()` **唯一转换对**（池的 `Allocate` / `Free` / `Resolve` 收发 `TTcsInstanceHandle<FTcsCastRunTag>`，而展平句柄与之**位模式一致但类型不同**——缺此对则句柄无法进池）；照 `FTcsChainRunHandle` 的既有形态（`static_cast` 保证 `-1` ↔ `0xFFFFFFFF`）；
- `FTcsCastRun` MUST 至少含：条目身份 `FTcsSkillEntryHandle EntryHandle`、**所属单位** `FTcsCombatEntityHandle Unit`、激活时快照的等级 `int32 Level`、当前时段下标 `int32 PhaseIndex`、链运行句柄 `FTcsChainRunHandle ChainRunHandle`、参数快照 `FTcsParamSnapshot ParamSnapshot`、来源锚点 `FTcsSourceHandle RunSource`、时段到期条目 `FTcsTimeEntryHandle PhaseExpiryEntry`；
  - **`Unit` 为何必须自持（MUST 记录）**：条目句柄的 `Index` **只在单位桶内有意义**（`TcsSkillRegistry.h` 明文）⇒ 仅凭 `EntryHandle` 无法跨单位定位条目；而施法终结路径（打断 / 顶替）只拿得到运行句柄，需要 `Unit` 才能摘账本条目与按来源回收。缺它会让终结路径要么无法定位、要么退化为全桶扫描；
  - **`PhaseExpiryEntry` 的写入者自本变更起存在**（时段推进落地）：它是**当前时段的取消锚点**——四路终结任一发生时 MUST 经 `UTcsClockSubsystem::CancelExpiry` 取消它，使作废的到期回调不再触发（池的句柄代际校验是第二道防线，取消是第一道）。
    **★ 本条取代了原文的一句（MUST 留痕，防后人以为该句被误删；**原文逐字保留**）**：

    > - **`PhaseExpiryEntry` 本 Task 无写入者**（时段推进归 Task 5）：字段按"设计承诺、消费者在后"就位，**MUST NOT** 为它建机制；

    该句的**时态**（"本 Task"= Task 3）已随本轮落地而**自然到期**：Task 5 正是它所说的那个"归 Task 5"。⇒ 被取代的是"**无写入者**"这一**当时事实**，**未被取代的是它的纪律**（"零消费者不预建"）：本轮为它建机制，是因为**消费者（时段推进与四路终结的取消锚点）在本轮真的出现了**；
- **运行态与账本的配对**：建 run 成功后 MUST **回填**该条目的 `FTcsLearnedSkillEntry::RunHandles`；run 终结（含顶替）时 MUST 摘除该句柄——**回填与摘除 MUST 成对**，否则账本会永久持有一个已归还的句柄（表现为"技能显示在飞但实际放完了"）；
- **GC 纪律（本需求的关键风险点）**：`FTcsCastRun.ParamSnapshot` 的 `SourceRef` 位可持 `UObject` 引用（数值来源的副本），而池元素住 `TArray` **非 `UPROPERTY` 容器** ⇒ 门面 `AddReferencedObjects` MUST 覆盖池内在册 run 的每个快照条目。**判据是"容器是否 GC 可见"，与值语义/指针语义无关**——同状态门面对在册实例快照的既有处置（逐条走 `FInstancedStruct::AddStructReferencedObjects`，**不用** `AddPropertyReferencesWithStructARO`：`FTcsParamSnapshotEntry` 不是反射结构体，没有 `StaticStruct()` 可给）；
- **终结 MUST 为「四路汇聚同一个例程」（本变更新增，MUST 记录以免后人"补第五份清理"）**：施法运行态的四个终结点 = ① **完成**（全时段走完，或**瞬发当刻**——其触发条件由 `skill-cast-phases` 能力的「时段推进与到期驱动」需求定义）、② **打断**（`CancelCast`，同上能力）、③ **顶替**（既有路径）、④ 世界拆解（池 `Reset`，**不逐槽走终结例程、不动账本**——世界正在拆，账本随之消亡）。四路 MUST 收敛到同一个终结例程，**MUST NOT 各写一份清理**——多份清理必然漂移，而漂移的后果是**静默泄漏**（条目留在账本里、属性不回退，且无人报错）。**四路只在一个地方分岔**：发哪一枚终止事件（完成 → `OnCastCompleted`；打断与顶替 → `OnCastInterrupted`）；
- **统一例程 MUST 做五件事**（缺一即泄漏）：① 取消时段到期锚点（`PhaseExpiryEntry`）；② 按 `RunSource` 回收**链挂的 M2 属性账本条目**；③ 按 `RunSource` 回收**本 run 物化的 M5 参数链条目**；④ 摘除账本 `RunHandles` 中的该句柄；⑤ 归还池槽位。**事件广播 MUST 排在归还槽位之前**（"先广播后归还槽位"的既有纪律 ⇒ 载荷里的运行句柄在回调内仍有效）；
- **`②` 与 `③` 是两本不同的账、同一个来源句柄（本变更闭合台账 `CHAIN-7`，MUST 写清以免后人只摘一本）**：
  - `②` 走 `UTcsAttributeSubsystem::RemoveBySource(Unit, RunSource)`（该口**公开且已有真实使用者** `TcsDamageSubsystem.cpp:226`）——摘的是**链内 `ModifyAttribute`** 挂的条目（`TcsStepModifyAttribute.cpp:80` 挂的正是 `Context.RunSource`）；
  - `③` 走 `FTcsParamChainOps::RemoveParamModifiersBySource`——摘的是**定义侧 `ParamChainRows` 物化**的条目；
  - 两者 `Source` **同源**（Task 3 已定 `Context.RunSource` = `FTcsCastRun.RunSource`）⇒ **一次摘净**。**MUST NOT 只摘其一**——只摘 `③` ⇒ 链改的属性常驻不回退，**正是 `CHAIN-7` 的实测症状"常驻停在 X−5"**（`EVID-2026-10-05-state-layer-pie` §1.2 检查 S15）；
  - **`CHAIN-7` 的闭合依据（MUST 记录，MUST NOT 读成"三条死路被修"）**：`CHAIN-7` 的触发条件原文是"出现**链改的属性必须随某生命周期回退**的真实内容需求"，而施法运行态是链运行态的**第一个长生命周期持有者** ⇒ 条件成立。**三条死路今天依然成立**：即时链的运行态在 `ExecuteChain` 返回前已释放（无回收窗口）、按 `CausedBy` 级联**已被明文禁止**（`TcsEffectContext.h:54-55`）、`FTcsStepModifyAttribute` **刻意不提供**回收器。⇒ 本需求落地的正是台账认定的"今天唯一合法用法"（宿主自存 `RunSource` 再自调 `RemoveBySource`）**所缺的那个触发点**；
- **只摘自己起的链（MUST NOT 越界）**：`RunSource` MUST 取**本次施法运行态自己的**。**子链另发新号、不沿用父链的**（`TcsEffectContext.h:49` 原文）⇒ 只摘本 run **直接**挂的条目；**MUST NOT 递归下探**（递归会摘掉不属于自己的东西，且 `CausedBy` 级联已被禁止）。**残留边界如实登记**（**MUST NOT 假装已解**）："技能起链 → 链起子链 → 子链挂属性且子链比施法活得久"这一组合下条目**仍会常驻**；
- **重入语义 MUST 收口（本变更裁掉 Task 3 留下的边界）**：事件广播是**当场同步派发** ⇒ 订阅者可在回调内**重入终结同一句柄**。MUST 使**递归终结同句柄返回 false 且静默**：MUST NOT 重复广播、MUST NOT 重复归还槽位。体例照状态侧已验证的 `InRemovalBroadcastHandles`（`TcsStateSubsystem.h:414`；进出广播窗口成对标记/解除，见 `TcsStateOps.cpp:188/244/247`）。**判据 = 订阅者在回调内重入同一句柄一次，`Interrupted` 读数仍为 1**——Task 3 只挡住了"双次归还"这一半，**"打断事件发两次"当时仍然存在**（`TcsCastOps.cpp:420-428` 自认，其归属即本变更）。

#### Scenario: 顶替后旧运行态归还且账本不残留

- **WHEN** 一个 `InstancePerEntity` + 可顶替的技能被激活两次（第二次顶替第一次）
- **THEN** 账本该条目的 `RunHandles` 仍恰有 **1** 个句柄（旧的已摘除）；旧句柄经池代际校验判定为**悬空**（`Resolve` 返回 nullptr）

#### Scenario: 新运行态继承来源锚点供链使用

- **WHEN** 激活成功并起主链
- **THEN** 该 run 的 `RunSource` 有效（非 0）且**两次激活的 `RunSource` 互异**（每次运行一个独立施加方，同链运行态的发号纪律）

#### Scenario: 四路终结各自可达且行为一致

- **WHEN** 分别在**完成**（含瞬发当刻完成）／**打断**／**顶替**三个内容侧终结点各终结一个已起链、链已改属性的 run（世界拆解那一路不取属性读数——账本随世界消亡）
- **THEN** 三条路径下：`RunHandles` 都被摘净（读数回到该 run 之前的条数）、池槽位都归还（旧句柄 `Resolve` = nullptr）、链挂的属性条目都被摘净、时段到期锚点都被取消（不再有后续 `PhaseChanged`）。**判据 MUST 逐路各取一读数**——只验一路无法发现"多份清理漂移"（那正是本需求存在的理由）

#### Scenario: 瞬发完成时属性按来源回退（`CHAIN-7` 闭合读数）

- **WHEN** 一条主链含 `ModifyAttribute`（写在属性 `Attack` 上）的技能以**空时段表**激活，链把 `Attack` 改掉，随后完成终结
- **THEN** `Attack` **回到链写入之前的值**，且该属性上的账本条目数回到 **0**。**对照组 = R5 实测的"常驻停在 X−5"**；**读数 MUST 取属性现值 + 条目数**（成对），**MUST NOT** 取"回收函数被调用了几次"（后者在"调了但摘错 Source"时仍会全绿）

#### Scenario: 打断路径与顶替路径同样回退

- **WHEN** 同上，但分别在**打断**与**顶替**两个终结点终结该 run
- **THEN** 两条路径下 `Attack` **同样**回到写入之前的值、条目数同样回到 0——**三路读数各自可判**（**MUST NOT** 只验"完成"一路就 claim 统一例程成立；只验一路无法发现"三份清理漂移"这一本需求要防的形态）

#### Scenario: 只摘自己起的链，子链条目不动

- **WHEN** 技能起主链 → 主链起子链（子链另发新号）→ 子链挂一条属性修正器；随后施法终结
- **THEN** **本 run 直接挂的条目被摘净**，而**子链挂的条目仍在**（其 `Source` 与 `RunSource` 不同）——**这是本轮如实登记的残留边界，MUST NOT 被读成"递归回收已实现"**

#### Scenario: 广播内重入终结同一句柄只发一次事件

- **WHEN** 一个订阅了 `TcsEvent.Cast.Interrupted` 的订阅者在回调内对**同一句柄**再调一次终结
- **THEN** `TcsEvent.Cast.Interrupted` **恰好 1 次**（递归那次静默返回 false）、无 `ensure`、无 `Fatal`，且池槽位只归还一次（第二次不再命中池的悬空句柄断言）

#### Scenario: 瞬发技能不在池中滞留

- **WHEN** 一个 `Phases` 为空数组的技能激活成功且不再有任何后续调用
- **THEN** 该 run 不是飞状态（在飞读数 = 0）、其池槽位已归还、账本 `RunHandles` 中不留其句柄——**MUST NOT** 出现"成功施法后运行态常驻"（那会让唯一的关闭口是打断，把一次成功施法报成"被打断"）

### Requirement: 施法事件四枚

`TcsSkill` MUST 原生声明施法生命周期事件四枚（**框架协议词汇，由插件原生声明**——事件广播面的契约词若由宿主配置，宿主漏配即静默破坏广播面）：

- 事件 Tag MUST 为 `TcsEvent.Cast.Started` / `TcsEvent.Cast.PhaseChanged` / `TcsEvent.Cast.Completed` / `TcsEvent.Cast.Interrupted`——**落在既有的 `TcsEvent` 根下**（域段 = `Cast`），**MUST NOT** 为此新增根；
- 变量声明 MUST 带**模块导出宏**（`extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_*`）：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无 `__declspec(dllexport)`）⇒ 跨模块引用必 `LNK2001`，而"供宿主订阅"正是本事件词汇的设计意图；
- 载荷 MUST 为**单一 `USTRUCT`**（`FTcsCastEventPayload`，四枚共用一种形状，同状态侧六枚共用 `FTcsStateEventPayload` 的先例）：至少含 `FTcsCastRunHandle Run` / `FTcsCombatEntityHandle Unit` / `FGameplayTag DefTag` / `int32 Level` / `FTcsSkillEntryHandle EntryHandle`；
  - **`Run` 是句柄不是指针**：订阅者若要读运行态细节，在回调内用句柄复核——**"先广播后归还槽位"的纪律**保证那一刻 run 仍在册；
  - **句柄字段的反射性**：`FTcsCastRunHandle` 已是 `USTRUCT(BlueprintType)`（Task 2 落地）⇒ 载荷可反射，无需二次展平；
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
- **THEN** `TcsEvent.Cast.Started` 被广播一次，其载荷的 `Run` / `Unit` / `DefTag` / `EntryHandle` 与该次激活一致

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
