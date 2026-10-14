## MODIFIED Requirements

### Requirement: 施法运行态与实例池

`TcsSkill` MUST 提供**施法运行态**——一次激活所产生的全部运行数据，且它 MUST 池化（**经共享层的统一桶模板**，见 `entity-runtime-mechanism` 能力的「统一 per-entity 桶与注册表」需求；**MUST NOT** 新建池实现，也 **MUST NOT** 自带手写的槽位 / 代际 / 空闲栈）：

- 运行态类型 MUST 为 `FTcsCastRun`，**非反射**（无 `USTRUCT`——池内池化纯数据，本轮零过网与零脚本消费面；同 `FTcsChainRun` / `FTcsLearnedSkillEntry` 的处置与理由）；
- MUST 提供句柄标签 `FTcsCastRunTag` 与统一桶实例 `TTcsEntityRuntimeBucket<FTcsCastRun>`（其内部载体 = `TTcsInstancePool`）；桶 MUST 以 `UPROPERTY` 之外的方式**由门面持有**并在 `Deinitialize` 确定性清空（同状态门面对注册表的清理口径）；
- 运行态句柄 MUST 为 `FTcsCastRunHandle`——**MUST 保持为独立类型**（MUST NOT 与 `FTcsStateHandle` / `FTcsSkillEntryHandle` 合并，理由见 `entity-runtime-mechanism` 能力的「统一运行态句柄判据与转换」需求：合并会移除跨桶误传的**编译期**拦截）；其 `IsValid` 判据 / 相等比较 / 哈希 / 池位模式转换的**语义 MUST 收敛到同一处公共件**。**★ 判据统一为** `Index >= 0 && Generation > 0`（原先判 `Index != -1`、**不看 `Generation`**）——对本域的良构句柄**行为中性**（`Allocate` 产物 `Generation >= 1`），且本类型头注释中"**本结构只判『是否曾被赋值』**"一句 MUST 改述为"连 `Generation` 一起判"并写明理由。池的 `Allocate` / `Free` / `Resolve` 收发 `TTcsInstanceHandle<FTcsCastRunTag>`，与展平句柄**位模式一致但类型不同** ⇒ MUST 保留 `GetInner()` / `SetInner()` **唯一转换对**（照 `FTcsChainRunHandle` 的既有形态，`static_cast` 保证 `-1` ↔ `0xFFFFFFFF`）；
- `FTcsCastRun` MUST 至少含：条目身份 `FTcsSkillEntryHandle EntryHandle`、**所属实体** `FTcsCombatEntityHandle Entity`、激活时快照的等级 `int32 Level`、当前时段下标 `int32 PhaseIndex`、链运行句柄 `FTcsChainRunHandle ChainRunHandle`、参数快照 `FTcsParamSnapshot ParamSnapshot`、来源锚点 `FTcsSourceHandle RunSource`、时段到期条目 `FTcsTimeEntryHandle PhaseExpiryEntry`；
  - **`Entity` 为何必须自持（MUST 记录）**：条目句柄的 `Index` **只在实体桶内有意义**（`TcsSkillRegistry.h` 明文）⇒ 仅凭 `EntryHandle` 无法跨实体定位条目；而施法终结路径（打断 / 顶替）只拿得到运行句柄，需要 `Entity` 才能摘账本条目与按来源回收。缺它会让终结路径要么无法定位、要么退化为全桶扫描；
  - **`PhaseExpiryEntry` 的写入者自本变更起存在**（时段推进落地）：它是**当前时段的取消锚点**——四路终结任一发生时 MUST 经 `UTcsClockSubsystem::CancelExpiry` 取消它，使作废的到期回调不再触发（桶的句柄代际校验是第二道防线，取消是第一道）。
    **★ 本条取代了原文的一句（MUST 留痕，防后人以为该句被误删；**原文逐字保留**）**：

    > - **`PhaseExpiryEntry` 本 Task 无写入者**（时段推进归 Task 5）：字段按"设计承诺、消费者在后"就位，**MUST NOT** 为它建机制；

    该句的**时态**（"本 Task"= Task 3）已随本轮落地而**自然到期**：Task 5 正是它所说的那个"归 Task 5"。⇒ 被取代的是"**无写入者**"这一**当时事实**，**未被取代的是它的纪律**（"零消费者不预建"）：本轮为它建机制，是因为**消费者（时段推进与四路终结的取消锚点）在本轮真的出现了**；
- **运行态与账本的配对**：建 run 成功后 MUST **回填**该条目的 `FTcsLearnedSkillEntry::RunHandles`；run 终结（含顶替）时 MUST 摘除该句柄——**回填与摘除 MUST 成对**，否则账本会永久持有一个已归还的句柄（表现为"技能显示在飞但实际放完了"）；
- **GC 纪律（本需求的关键风险点）**：`FTcsCastRun.ParamSnapshot` 的 `SourceRef` 位可持 `UObject` 引用（数值来源的副本），而桶元素住 `TArray` **非 `UPROPERTY` 容器** ⇒ 门面 `AddReferencedObjects` MUST 覆盖桶内在册 run 的每个快照条目，且 MUST 经**共享层的 GC 只读遍历口**（`ForEachForGC`，见 `entity-runtime-mechanism` 能力的「统一 GC 引用收集口」需求——该口 MUST NOT 断言游戏线程，因为 GC 引用收集不在游戏线程上）。**判据是"容器是否 GC 可见"，与值语义/指针语义无关**——同状态门面对在册实例快照的既有处置（逐条走 `FInstancedStruct::AddStructReferencedObjects`，**不用** `AddPropertyReferencesWithStructARO`：`FTcsParamSnapshotEntry` 不是反射结构体，没有 `StaticStruct()` 可给）；
- **终结 MUST 为「四路汇聚同一个例程」（本变更新增，MUST 记录以免后人"补第五份清理"）**：施法运行态的四个终结点 = ① **完成**（全时段走完，或**瞬发当刻**——其触发条件由 `skill-cast-phases` 能力的「时段推进与到期驱动」需求定义）、② **打断**（`CancelCast`，同上能力）、③ **顶替**（既有路径）、④ 世界拆解（桶 `Reset`，**不逐槽走终结例程、不动账本**——世界正在拆，账本随之消亡）。四路 MUST 收敛到同一个终结例程，**MUST NOT 各写一份清理**——多份清理必然漂移，而漂移的后果是**静默泄漏**（条目留在账本里、属性不回退，且无人报错）。**四路只在一个地方分岔**：发哪一枚终止事件（完成 → `OnCastCompleted`；打断与顶替 → `OnCastInterrupted`）；
- **统一例程 MUST 做五件事**（缺一即泄漏）：① 取消时段到期锚点（`PhaseExpiryEntry`）；② 按 `RunSource` 回收**链挂的 M2 属性账本条目**；③ 按 `RunSource` 回收**本 run 物化的 M5 参数链条目**；④ 摘除账本 `RunHandles` 中的该句柄；⑤ 归还桶槽位。**事件广播 MUST 排在归还槽位之前**（"先广播后归还槽位"的既有纪律 ⇒ 载荷里的运行句柄在回调内仍有效）；
- **`②` 与 `③` 是两本不同的账、同一个来源句柄（本变更闭合台账 `CHAIN-7`，MUST 写清以免后人只摘一本）**：
  - `②` 走 `UTcsAttributeSubsystem::RemoveBySource(Entity, RunSource)`（该口**公开且已有真实使用者** `TcsDamageSubsystem.cpp:226`）——摘的是**链内 `ModifyAttribute`** 挂的条目（`TcsStepModifyAttribute.cpp:80` 挂的正是 `Context.RunSource`）；
  - `③` 走 `FTcsParamChainOps::RemoveParamModifiersBySource`——摘的是**定义侧 `ParamChainRows` 物化**的条目；
  - 两者 `Source` **同源**（Task 3 已定 `Context.RunSource` = `FTcsCastRun.RunSource`）⇒ **一次摘净**。**MUST NOT 只摘其一**——只摘 `③` ⇒ 链改的属性常驻不回退，**正是 `CHAIN-7` 的实测症状"常驻停在 X−5"**（`EVID-2026-10-05-state-layer-pie` §1.2 检查 S15）；
  - **`CHAIN-7` 的闭合依据（MUST 记录，MUST NOT 读成"三条死路被修"）**：`CHAIN-7` 的触发条件原文是"出现**链改的属性必须随某生命周期回退**的真实内容需求"，而施法运行态是链运行态的**第一个长生命周期持有者** ⇒ 条件成立。**三条死路今天依然成立**：即时链的运行态在 `ExecuteChain` 返回前已释放（无回收窗口）、按 `CausedBy` 级联**已被明文禁止**（`TcsEffectContext.h:54-55`）、`FTcsStepModifyAttribute` **刻意不提供**回收器。⇒ 本需求落地的正是台账认定的"今天唯一合法用法"（宿主自存 `RunSource` 再自调 `RemoveBySource`）**所缺的那个触发点**；
- **只摘自己起的链（MUST NOT 越界）**：`RunSource` MUST 取**本次施法运行态自己的**。**子链另发新号、不沿用父链的**（`TcsEffectContext.h:49` 原文）⇒ 只摘本 run **直接**挂的条目；**MUST NOT 递归下探**（递归会摘掉不属于自己的东西，且 `CausedBy` 级联已被禁止）。**残留边界如实登记**（**MUST NOT 假装已解**）："技能起链 → 链起子链 → 子链挂属性且子链比施法活得久"这一组合下条目**仍会常驻**；
- **重入语义 MUST 收口（本变更裁掉 Task 3 留下的边界）**：事件广播是**当场同步派发** ⇒ 订阅者可在回调内**重入终结同一句柄**。MUST 使**递归终结同句柄返回 false 且静默**：MUST NOT 重复广播、MUST NOT 重复归还槽位。体例 MUST 经**共享层的统一重入守卫**（见 `entity-runtime-mechanism` 能力的「统一门面清理口与重入广播守卫」需求——它取代了状态侧 `InRemovalBroadcastHandles` 与技能侧 `InFinishBroadcastHandles` 两份；进出广播窗口成对标记/解除）。**判据 = 订阅者在回调内重入同一句柄一次，`Interrupted` 读数仍为 1**——Task 3 只挡住了"双次归还"这一半，**"打断事件发两次"当时仍然存在**（`TcsCastOps.cpp:420-428` 自认，其归属即本变更）。

#### Scenario: 顶替后旧运行态归还且账本不残留

- **WHEN** 一个 `InstancePerEntity` + 可顶替的技能被激活两次（第二次顶替第一次）
- **THEN** 账本该条目的 `RunHandles` 仍恰有 **1** 个句柄（旧的已摘除）；旧句柄经桶代际校验判定为**悬空**（`Resolve` 返回 nullptr）

#### Scenario: 新运行态继承来源锚点供链使用

- **WHEN** 激活成功并起主链
- **THEN** 该 run 的 `RunSource` 有效（非 0）且**两次激活的 `RunSource` 互异**（每次运行一个独立施加方，同链运行态的发号纪律）

#### Scenario: 四路终结各自可达且行为一致

- **WHEN** 分别在**完成**（含瞬发当刻完成）／**打断**／**顶替**三个内容侧终结点各终结一个已起链、链已改属性的 run（世界拆解那一路不取属性读数——账本随世界消亡）
- **THEN** 三条路径下：`RunHandles` 都被摘净（读数回到该 run 之前的条数）、桶槽位都归还（旧句柄 `Resolve` = nullptr）、链挂的属性条目都被摘净、时段到期锚点都被取消（不再有后续 `PhaseChanged`）。**判据 MUST 逐路各取一读数**——只验一路无法发现"多份清理漂移"（那正是本需求存在的理由）

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
- **THEN** `TcsEvent.Cast.Interrupted` **恰好 1 次**（递归那次静默返回 false）、无 `ensure`、无 `Fatal`，且桶槽位只归还一次（第二次不再命中桶的悬空句柄断言）

#### Scenario: 瞬发技能不在池中滞留

- **WHEN** 一个 `Phases` 为空数组的技能激活成功且不再有任何后续调用
- **THEN** 该 run 不是飞状态（在飞读数 = 0）、其桶槽位已归还、账本 `RunHandles` 中不留其句柄——**MUST NOT** 出现"成功施法后运行态常驻"（那会让唯一的关闭口是打断，把一次成功施法报成"被打断"）

### Requirement: 六道具名门禁与激活入口

技能门面 MUST 提供 `TryActivate(FTcsCombatEntityHandle Entity, FGameplayTag DefTag, FTcsCastRunHandle* OutRun) -> ESkillActivateResult`，且**门禁 MUST 逐道具名、MUST NOT 内联成一个笼统失败**（修 TCS 库外报告 09 的"门禁内联无钩子"缺陷）：

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
  - **可打断性判据的 Task 3 → Task 5 接缝（MUST 写清）**：本 Task 取 `FTcsSkillDefData::bInterruptibleDefault`（即 `ECastQueryMode::DefSwitches` 档——**它是查询契约的默认档**，Task 1 已交付）。Task 5 落地 `IsInterruptibleNow()` 后改调它，而 **DefSwitches 档读的就是该字段 ⇒ 行为逐字不变、零回归**。本 Task **MUST NOT** 预建三档分派（`PhaseTable` 需时段推进、`Custom` 需 Fragment 求值）。

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

- **WHEN** 同上但 `bInterruptibleDefault = false`，第二次激活
- **THEN** 返回 `SAR_AlreadyActive`，且旧 run **原样继续**（旧句柄仍可 `Resolve`、`RunHandles` 不变、`OnCastInterrupted` 未发）

#### Scenario: 定义不可解析时不给后续门禁假读数

- **WHEN** 某条目存在，但其 `DefTag` 的定义已被注销（条目授予后注销定义），随后 `TryActivate`
- **THEN** 返回 `SAR_DefInvalid`——**MUST NOT** 因第 ③④⑤ 道无法求值而落到其它枚举值，也 MUST NOT 建 run

#### Scenario: 四档分流不内联（本变更新增）

- **WHEN** 检查激活算子里对 `ECastInstancing` 与 `bRetriggerOnActive` 的处理
- **THEN** MUST 经 `FTcsEntityAdmission` 的策略类判定，MUST NOT 存在内联的档位分流；且四档的**行为读数与重构前逐字一致**（阴性对照：四态各取一次读数）
