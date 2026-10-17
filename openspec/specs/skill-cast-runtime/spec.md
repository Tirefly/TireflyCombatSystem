# skill-cast-runtime Specification

## Purpose
定义 M5 技能层的**施法运行态**：`FTcsSkillInstance` 池化运行态与其实例池、六道具名门禁的激活入口 `TryActivate`、激活瞬间的参数快照绑定、主效果链的起链时点，以及施法事件四枚的词与载荷——让"技能放得出来"成为可裁决、可观测、可具名拒绝的运行事实，而不是只有一份账本。

## Requirements

### Requirement: 施法运行态与实例池

`TcsSkill` MUST 提供**施法运行态**——一次激活所产生的全部运行数据，且它 MUST 池化（**经共享层的统一桶模板**，见 `entity-runtime-mechanism` 能力的「统一 per-entity 桶与注册表」需求；**MUST NOT** 新建池实现，也 **MUST NOT** 自带手写的槽位 / 代际 / 空闲栈）：

- 运行态类型 MUST 为 `FTcsSkillInstance`——**它自 `PLN-REFACTOR-1` Task 3 起是基底 `FTcsStateInstance` 的派生结构**（`struct FTcsSkillInstance : public FTcsStateInstance`），只声明**技能域内字段**，公共字段（`DefTag` / `Entity` / `CascadeAnchor` / `ParamSnapshot` / `Level` / `ExpiryEntry`）**MUST 由基底提供、MUST NOT 在本类型重复声明**；它 **非反射**（无 `USTRUCT`——池内池化纯数据，本轮零过网与零脚本消费面；同 `FTcsChainRun` / `FTcsLearnedSkillEntry` 的处置与理由）；
  - **基底化的判据 MUST 记录**：技能域与 Buff 域的运行态**机制同型**（同一套池 / 桶 / 代际 / 门面 / ARO / 重入守卫）⇒ 公共字段与机制收敛到共享层，派生结构只留**域内**语义（见 `entity-runtime-mechanism` 能力与 `DEC-2026-10-13-state-module-positioning`）；
- MUST 提供句柄标签 `FTcsSkillInstanceTag` 与统一桶实例 `TTcsEntityRuntimeBucket<FTcsSkillInstance>`（其内部载体 = `TTcsInstancePool`）；**桶的载荷 MUST 绑派生结构 `FTcsSkillInstance`、MUST NOT 绑基底**——池按值存 `TArray<TPayload>`，绑基底会把技能域字段**静默截断**（多数读数仍会过，是先例 `FTcsBuffInstance` 的同型陷阱）；桶 MUST 以 `UPROPERTY` 之外的方式**由门面持有**并在 `Deinitialize` 确定性清空（同状态门面对注册表的清理口径）；
- 运行态句柄 MUST 为 `FTcsSkillInstanceHandle`——**MUST 保持为独立类型**（MUST NOT 与 `FTcsBuffInstanceHandle` / `FTcsSkillEntryHandle` 合并，理由见 `entity-runtime-mechanism` 能力的「统一运行态句柄判据与转换」需求：合并会移除跨桶误传的**编译期**拦截）；其 `IsValid` 判据 / 相等比较 / 哈希 / 池位模式转换的**语义 MUST 收敛到同一处公共件**。**★ 判据统一为** `Index >= 0 && Generation > 0`（原先判 `Index != -1`、**不看 `Generation`**）——对本域的良构句柄**行为中性**（`Allocate` 产物 `Generation >= 1`），且本类型头注释中"**本结构只判『是否曾被赋值』**"一句 MUST 改述为"连 `Generation` 一起判"并写明理由。池的 `Allocate` / `Free` / `Resolve` 收发 `TTcsInstanceHandle<FTcsSkillInstanceTag>`，与展平句柄**位模式一致但类型不同** ⇒ MUST 保留 `GetInner()` / `SetInner()` **唯一转换对**（照 `FTcsChainRunHandle` 的既有形态，`static_cast` 保证 `-1` ↔ `0xFFFFFFFF`）；
- **字段归属 MUST 按基底 / 派生分列**（`PLN-REFACTOR-1` Task 3 的形状：**11 字段 = 5 上提基底 + 6 留本域**）：
  - **基底 `FTcsStateInstance` 提供 6 个**：`FGameplayTag DefTag`（**技能侧自 Task 3 起真的填上**，读取点改读自己；**但「条目已被撤销 ⇒ 不广播终止事件」的存在性门禁语义逐字不变**）、`FTcsCombatEntityHandle Entity`、`FTcsSourceHandle CascadeAnchor`（**由原 `RunSource` 改名**——基底只放**撤销锚点**）、`FTcsParamSnapshot ParamSnapshot`、`int32 Level`、`FTcsTimeEntryHandle ExpiryEntry`（**由原 `PhaseExpiryEntry` 改名**——基底字段名 MUST NOT 偏向任一域）；
  - **本派生结构提供 6 个技能域字段**：`FTcsSkillEntryHandle EntryHandle`、`int32 PhaseIndex`、`FTcsChainRunHandle ChainRunHandle`、`FGameplayTag ActiveChainId`、`EMainChainStart MainChainStart`、`FGameplayTag MainChainStartPhaseTag`；
  - **`Entity` 为何必须自持（MUST 记录）**：条目句柄的 `Index` **只在实体桶内有意义**（`TcsSkillRegistry.h` 明文）⇒ 仅凭 `EntryHandle` 无法跨实体定位条目；而施法终结路径（打断 / 顶替）只拿得到运行句柄，需要 `Entity` 才能摘账本条目与按来源回收。缺它会让终结路径要么无法定位、要么退化为全桶扫描；
  - **`ExpiryEntry` 的写入者自本变更起存在**（时段推进落地）：它是**当前时段的取消锚点**——四路终结任一发生时 MUST 经 `UTcsClockSubsystem::CancelExpiry` 取消它，使作废的到期回调不再触发（桶的句柄代际校验是第二道防线，取消是第一道）。
  - **`Entity` 为何必须自持（MUST 记录）**：条目句柄的 `Index` **只在实体桶内有意义**（`TcsSkillRegistry.h` 明文）⇒ 仅凭 `EntryHandle` 无法跨实体定位条目；而施法终结路径（打断 / 顶替）只拿得到运行句柄，需要 `Entity` 才能摘账本条目与按来源回收。缺它会让终结路径要么无法定位、要么退化为全桶扫描；
  - **`PhaseExpiryEntry` 的写入者自本变更起存在**（时段推进落地）：它是**当前时段的取消锚点**——四路终结任一发生时 MUST 经 `UTcsClockSubsystem::CancelExpiry` 取消它，使作废的到期回调不再触发（桶的句柄代际校验是第二道防线，取消是第一道）。
    **★ 本条取代了原文的一句（MUST 留痕，防后人以为该句被误删；**原文逐字保留**）**：

    > - **`PhaseExpiryEntry` 本 Task 无写入者**（时段推进归 Task 5）：字段按"设计承诺、消费者在后"就位，**MUST NOT** 为它建机制；

    该句的**时态**（"本 Task"= Task 3）已随本轮落地而**自然到期**：Task 5 正是它所说的那个"归 Task 5"。⇒ 被取代的是"**无写入者**"这一**当时事实**，**未被取代的是它的纪律**（"零消费者不预建"）：本轮为它建机制，是因为**消费者（时段推进与四路终结的取消锚点）在本轮真的出现了**；
- **运行态与账本的配对**：建 run 成功后 MUST **回填**该条目的 `FTcsLearnedSkillEntry::RunHandles`；run 终结（含顶替）时 MUST 摘除该句柄——**回填与摘除 MUST 成对**，否则账本会永久持有一个已归还的句柄（表现为"技能显示在飞但实际放完了"）；
- **GC 纪律（本需求的关键风险点）**：`FTcsSkillInstance.ParamSnapshot` 的 `SourceRef` 位可持 `UObject` 引用（数值来源的副本），而桶元素住 `TArray` **非 `UPROPERTY` 容器** ⇒ 门面 `AddReferencedObjects` MUST 覆盖桶内在册 run 的每个快照条目，且 MUST 经**共享层的 GC 只读遍历口**（`ForEachForGC`，见 `entity-runtime-mechanism` 能力的「统一 GC 引用收集口」需求——该口 MUST NOT 断言游戏线程，因为 GC 引用收集不在游戏线程上）。**判据是"容器是否 GC 可见"，与值语义/指针语义无关**——同状态门面对在册实例快照的既有处置（逐条走 `FInstancedStruct::AddStructReferencedObjects`，**不用** `AddPropertyReferencesWithStructARO`：`FTcsParamSnapshotEntry` 不是反射结构体，没有 `StaticStruct()` 可给）；
- **终结 MUST 为「四路汇聚同一个例程」（本变更新增，MUST 记录以免后人"补第五份清理"）**：施法运行态的四个终结点 = ① **完成**（全时段走完，或**瞬发当刻**——其触发条件由 `skill-cast-phases` 能力的「时段推进与到期驱动」需求定义）、② **打断**（`CancelCast`，同上能力）、③ **顶替**（既有路径）、④ 世界拆解（桶 `Reset`，**不逐槽走终结例程、不动账本**——世界正在拆，账本随之消亡）。四路 MUST 收敛到同一个终结例程，**MUST NOT 各写一份清理**——多份清理必然漂移，而漂移的后果是**静默泄漏**（条目留在账本里、属性不回退，且无人报错）。**四路只在一个地方分岔**：发哪一枚终止事件（完成 → `OnCastCompleted`；打断与顶替 → `OnCastInterrupted`）；
- **统一例程 MUST 做五件事**（缺一即泄漏）：① 取消时段到期锚点（`PhaseExpiryEntry`）；② 按 `RunSource` 回收**链挂的 M2 属性账本条目**；③ 按 `RunSource` 回收**本 run 物化的 M5 参数链条目**；④ 摘除账本 `RunHandles` 中的该句柄；⑤ 归还桶槽位。**事件广播 MUST 排在归还槽位之前**（"先广播后归还槽位"的既有纪律 ⇒ 载荷里的运行句柄在回调内仍有效）；
- **`②` 与 `③` 是两本不同的账、同一个来源句柄（本变更闭合台账 `CHAIN-7`，MUST 写清以免后人只摘一本）**：
  - `②` 走 `UTcsAttributeSubsystem::RemoveBySource(Entity, RunSource)`（该口**公开且已有真实使用者** `TcsDamageSubsystem.cpp:226`）——摘的是**链内 `ModifyAttribute`** 挂的条目（`TcsStepModifyAttribute.cpp:80` 挂的正是 `Context.RunSource`）；
  - `③` 走 `FTcsParamChainOps::RemoveParamModifiersBySource`——摘的是**定义侧 `ParamChainRows` 物化**的条目；
  - 两者 `Source` **同源**（Task 3 已定 `Context.RunSource` = `FTcsSkillInstance.RunSource`）⇒ **一次摘净**。**MUST NOT 只摘其一**——只摘 `③` ⇒ 链改的属性常驻不回退，**正是 `CHAIN-7` 的实测症状"常驻停在 X−5"**（`EVID-2026-10-05-state-layer-pie` §1.2 检查 S15）；
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

### Requirement: 激活期参数快照绑定

激活成功后 MUST **在门禁全过之后、任何后续读取之前**构建并绑定一份参数快照——它是"施法期间读到的数值不再漂移"的唯一机制：

- MUST 提供 `BuildSkillSnapshot`：按 `FTcsSkillDefData.Params` **逐行**处理并冻结进 `FTcsSkillInstance.ParamSnapshot`；**写入点规则 MUST 与状态侧逐字同款**（覆盖值优先且不再过值约定 / 否则求值后按该行 `ValueConvention` 转规范值 / 重建语义 = 先 `Reset` 再填）；
- **`Mode` 列 MUST 参与分流（D5-12 v2 的"逐参数 Mode"）**：`EPM_Snapshot`（默认）⇒ **求值并冻结**进快照；`EPM_Live` ⇒ **标记跳过**（不进快照的数值面，实时读取走账本求值）。**MUST NOT 无差别地把 `EPM_Live` 行也冻结**——那会让"实时通道"名存实亡（配了 Live 的行在施法期读到的仍是冻结值）；
- **快照的消费面 MUST 是"求值期的参数表载体"（2026-10-09 用户裁定 A′，本条是防误读的唯一说明）**：快照 MUST 作为**物化/求值时的参数表**被绑定（经既有读取适配器 `UTcsStateParamTableReader` + `FTcsStateSnapshotScope`），供引用类数值源（`ParamRef`）取值——**先例 = 状态侧的 `FTcsBuffModifierMaterializer` 把 `Instance.ParamSnapshot` 装进 `Ctx.ParamTable`**。技能侧的落实点 = `FTcsCastOps::Activate` 物化 `ParamChainRows` 时的那段绑定作用域。**MUST NOT 把本快照读成"公共读口的冻结来源"**：`GetNumericParam` 走**实时**通道（参数行初值 + 账本槽位折叠），**MUST NOT** 被要求改读快照——判据：快照住**每次 run**（`FTcsSkillInstance.ParamSnapshot`），而读口签名 `(Entity, EntryHandle, Key)` **没有 run 段**，且 `CI_InstancePerExecution` 下同一 Entry **可有两个 run 并存** ⇒ "读哪一次施法的冻结值"**无唯一答案**；为一个无唯一答案的读数给读口加 run 形参 = 零消费者预建（施法期确需"某次 run 的冻结读数"时按台账另议读口）；
- **缺口登记（2026-10-09 实测，MUST 记录以防"以为已实现"）**：本能力的初版**只构建了快照、没有任何消费者**，且 `Mode` 列**完全未被读取**（`BuildSkillSnapshot` 无 `Mode` 判断 ⇒ `EPM_Live` 行也被冻结、"实时通道"名存实亡）。本变更即其闭合：**补上 `Mode` 分流 + 快照作为求值参数表被真正消费**（快照从"只写不读"变为"激活期物化链行的求值表"）。**同批如实订正一处初版误记**：初版曾把缺口写成"`GetNumericParam` MUST 改走快照"，该措辞**与 A′ 裁定冲突且本身不成立**（读口无 run 段、无唯一答案）——正确缺口是"快照零消费者 + `Mode` 未接"，不是"读口未接快照"；
- **复用面与不复用面 MUST 分清**（防后人误读为"建了第二套快照"）：**快照类型** `FTcsParamSnapshot` 与**读取适配器** `FTcsStateParamTableReader` + `FTcsStateSnapshotScope` MUST **复用**，MUST NOT 新建第二套快照类型、也 MUST NOT 出现第二处键查找语义；但**构建函数** MUST 另有一份——状态侧的 `FTcsBuffOps::BuildSnapshot` 形参硬编码为 `const FTcsBuffDef&`，技能侧**物理上无法复用**。两份构建函数 MUST 保持同一套写入点规则（判据 = 规则同款，而非函数同一）；
- **时序 MUST 为**：门禁全过 → 构建快照 → 绑定到新建的 run → 起主链 → 发 `OnCastStarted`。**MUST NOT** 在门禁之前构建（门禁失败即白算一份快照）；
- `Level` MUST 取 **激活时快照的生效等级**（`EffectiveLevel` 语义 = `clamp(0, LevelBase + Σ参数账本 Level 键修正)`；本轮 `Σ` 由 Task 4 的参数链提供，本 Task 取 `LevelBase` 与条目的持久等级口径）——**运行中升级不追溯**；
- 快照 MUST 随 run 存活（run 终结时随池槽位归还而失效），MUST NOT 跨 run 共享一份快照。

#### Scenario: 施法中途改数值来源不影响本次读数

- **WHEN** 激活一个含参数行的技能，随后改变该行数值来源在别处会读到的输入
- **THEN** 本次 run 的快照内该键保持激活时刻求值得到的值（run 存续期内不重算）

#### Scenario: 快照写入点的值约定只转一次

- **WHEN** 某行配 `VCF_Percent` 且书写值为 85，激活该技能
- **THEN** run 快照内该键为 `0.85`（转换发生一次，读快照不再转）

### Requirement: 主效果链起链时点

激活成功后 MUST 按定义的 **`MainChainStart`** 决定何时起主链，且**未实现的档位 MUST 具名报出、MUST NOT 静默不生效**：

- `CastChainId` MUST 经**效果链登记表**解析（`UTcsEffectSubsystem::ExecuteChain` 的既有接口）；**无效 `CastChainId` MUST 在作者期校验拦下**（`skill-def-asset` 的既有校验规则），运行期遇到则**不阻断激活**（技能已放出去，只是没有链）+ `Warning`；
- 起链 MUST 用 `ExecuteChain(CastChainId, Context)`，且 **`Context.RunSource` MUST = `FTcsSkillInstance.RunSource`**——这是台账 `CHAIN-7`（"链挂的账本条目没有框架侧回收触发点"）的**闭合依据**：施法运行态是链运行态的**第一个长生命周期持有者**，故链内 `ModifyAttribute` 挂的条目可按该锚点回收。**回收例程本身归 Task 5 Step 3**（施法终结三路统一回收），本 Task 只落锚点；
- `Context.Caster` MUST = 施法实体、`Context.Instigator` MUST = 施法实体（本轮无独立的发起者参数）；
- **`MainChainStart` 的档位处置（MUST 逐档写明，不得留白）**：本 Task 只实现 **`MCS_OnCastStarted`**；`MCS_OnPhaseEnter` / `MCS_OnCastCompleted` / `MCS_Custom` 三档**命中时 MUST 记 Warning 并明确报出"该档未实现、主链未起"**——**MUST NOT** 静默跳过（那会让配了 `OnCastCompleted` 的技能"不报错也不生效"）；
- 链跑完与否 MUST NOT 影响激活的返回（链可挂起）；`ChainRunHandle` MUST 存入 run（本轮无读取者，如实登记——Task 5 的终结路径要按它摘链锚）。

#### Scenario: 主链在施法开始起且继承来源锚点

- **WHEN** 配 `MCS_OnCastStarted` 与有效 `CastChainId` 的技能激活成功
- **THEN** 该链被起一次，且其黑板的 `RunSource` = 该 run 的 `RunSource`（非 0、非其它来源）

#### Scenario: 未实现的启动档位具名报出

- **WHEN** 配 `MCS_OnCastCompleted`（本轮未实现）的技能激活成功
- **THEN** 激活**仍然成功**（返回 `SAR_Success`），但留下一条具名 `Warning` 指明该档未实现且主链未起——**MUST NOT** 静默通过

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
