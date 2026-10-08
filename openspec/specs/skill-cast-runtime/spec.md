# skill-cast-runtime Specification

## Purpose
定义 M5 技能层的**施法运行态**：`FTcsCastRun` 池化运行态与其实例池、六道具名门禁的激活入口 `TryActivate`、激活瞬间的参数快照绑定、主效果链的起链时点，以及施法事件四枚的词与载荷——让"技能放得出来"成为可裁决、可观测、可具名拒绝的运行事实，而不是只有一份账本。

## Requirements

### Requirement: 施法运行态与实例池

`TcsSkill` MUST 提供**施法运行态**——一次激活所产生的全部运行数据，且它 MUST 池化（复用 `TTcsInstancePool` 的既有机制，**MUST NOT** 新建池实现）：

- 运行态类型 MUST 为 `FTcsCastRun`，**非反射**（无 `USTRUCT`——池内池化纯数据，本轮零过网与零脚本消费面；同 `FTcsChainRun` / `FTcsLearnedSkillEntry` 的处置与理由）；
- MUST 提供句柄标签 `FTcsCastRunTag` 与池实例 `TTcsInstancePool<FTcsCastRun, FTcsCastRunTag>`；池 MUST 以 `UPROPERTY` 之外的方式**由门面持有**并在 `Deinitialize` 确定性清空（同状态门面对注册表的清理口径）；
- `FTcsCastRunHandle` MUST 补 `GetInner()` / `SetInner()` **唯一转换对**（池的 `Allocate` / `Free` / `Resolve` 收发 `TTcsInstanceHandle<FTcsCastRunTag>`，而展平句柄与之**位模式一致但类型不同**——缺此对则句柄无法进池）；照 `FTcsChainRunHandle` 的既有形态（`static_cast` 保证 `-1` ↔ `0xFFFFFFFF`）；
- `FTcsCastRun` MUST 至少含：条目身份 `FTcsSkillEntryHandle EntryHandle`、**所属单位** `FTcsCombatEntityHandle Unit`、激活时快照的等级 `int32 Level`、当前时段下标 `int32 PhaseIndex`、链运行句柄 `FTcsChainRunHandle ChainRunHandle`、参数快照 `FTcsParamSnapshot ParamSnapshot`、来源锚点 `FTcsSourceHandle RunSource`、时段到期条目 `FTcsTimeEntryHandle PhaseExpiryEntry`；
  - **`Unit` 为何必须自持（MUST 记录）**：条目句柄的 `Index` **只在单位桶内有意义**（`TcsSkillRegistry.h` 明文）⇒ 仅凭 `EntryHandle` 无法跨单位定位条目；而施法终结路径（打断 / 顶替）只拿得到运行句柄，需要 `Unit` 才能摘账本条目与按来源回收。缺它会让终结路径要么无法定位、要么退化为全桶扫描；
  - **`PhaseExpiryEntry` 本 Task 无写入者**（时段推进归 Task 5）：字段按"设计承诺、消费者在后"就位，**MUST NOT** 为它建机制；
- **运行态与账本的配对**：建 run 成功后 MUST **回填**该条目的 `FTcsLearnedSkillEntry::RunHandles`；run 终结（含顶替）时 MUST 摘除该句柄——**回填与摘除 MUST 成对**，否则账本会永久持有一个已归还的句柄（表现为"技能显示在飞但实际放完了"）；
- **GC 纪律（本需求的关键风险点）**：`FTcsCastRun.ParamSnapshot` 的 `SourceRef` 位可持 `UObject` 引用（数值来源的副本），而池元素住 `TArray` **非 `UPROPERTY` 容器** ⇒ 门面 `AddReferencedObjects` MUST 覆盖池内在册 run 的每个快照条目。**判据是"容器是否 GC 可见"，与值语义/指针语义无关**——同状态门面对在册实例快照的既有处置（逐条走 `FInstancedStruct::AddStructReferencedObjects`，**不用** `AddPropertyReferencesWithStructARO`：`FTcsParamSnapshotEntry` 不是反射结构体，没有 `StaticStruct()` 可给）。

#### Scenario: 顶替后旧运行态归还且账本不残留

- **WHEN** 一个 `InstancePerEntity` + 可顶替的技能被激活两次（第二次顶替第一次）
- **THEN** 账本该条目的 `RunHandles` 仍恰有 **1** 个句柄（旧的已摘除）；旧句柄经池代际校验判定为**悬空**（`Resolve` 返回 nullptr）

#### Scenario: 新运行态继承来源锚点供链使用

- **WHEN** 激活成功并起主链
- **THEN** 该 run 的 `RunSource` 有效（非 0）且**两次激活的 `RunSource` 互异**（每次运行一个独立施加方，同链运行态的发号纪律）

### Requirement: 六道具名门禁与激活入口

技能门面 MUST 提供 `TryActivate(FTcsCombatEntityHandle Unit, FGameplayTag DefTag, FTcsCastRunHandle* OutRun) -> ESkillActivateResult`，且**门禁 MUST 逐道具名、MUST NOT 内联成一个笼统失败**（修 TCS 库外报告 09 的"门禁内联无钩子"缺陷）：

- 结果枚举 MUST 为 `UENUM(BlueprintType) ESkillActivateResult`，**值名带 `SAR_` 前缀**（全仓 `UENUM(BlueprintType)` 的枚举值无一例外带前缀；`BlueprintType` 是必需的——宿主查出"为什么放不出来"是本门禁的**唯一价值**）；
- **门禁序列（本 Task 评估序）** MUST 为：**① 实体 Ready → ② 已学 → ③ 冷却 → ④ Instancing → ⑤ CanAfford → ⑥ 定义可解析**，每道各有独立枚举值：
  `SAR_EntityNotReady` / `SAR_NotLearned` / `SAR_OnCooldown` / `SAR_AlreadyActive` / `SAR_CannotAfford` / `SAR_DefInvalid`，全过返回 `SAR_Success`；
- **第 ① 道 MUST 用 `ITcsEntityQuery::IsEntityReady`**（经 Task 2 的注入点），**MUST NOT** 用 `IsAlive` 代替（两者正交：前者答"框架能不能在它身上操作"、后者答"宿主认为它活着吗"）；未注入时按 Task 2 的既定语义**降级放行**；
- **第 ⑥ 道的评估时点 MUST 写明（MUST，关掉一处顺序歧义）**：第 ③④⑤ 道**均需读取定义内容**（冷却轨道 / 实例化方式与顶替位 / Cost 策略都是 Def 字段）⇒ 定义**不可解析时** MUST 立即返回 `SAR_DefInvalid`，且**不评估第 ③④⑤ 道**（它们不可求值）。枚举值的**次序**仍按上列六道序；本条约束的是**评估可达性**，不是重排门禁；
- **第 ③ 道（冷却）与第 ⑤ 道（CanAfford）在本 Task 为占位**：判据 = 冷却**轨道为空 ⇒ 恒可放**、Cost 策略取**不消耗档 ⇒ 恒可付**；**MUST NOT** 引用任何尚未定名的枚举值（`R6.5` 的定义尚在概念期，引用即制造第二真相）。两道 MUST 保持**可拒绝的结构**（各有自己的枚举值），只是本轮判据恒过；
- **拒绝路径 MUST NOT 有副作用**（纯查询语义）：任一道拒绝时 MUST NOT 建 run、MUST NOT 改账本、MUST NOT 起链、MUST NOT 发事件；
- **第 ④ 道 Instancing 四路分流**（`ECastInstancing` × `bRetriggerOnActive`）：
  - `CI_InstancePerExecution` ⇒ **并存**：不查在飞实例，直接建新 run（依据 GAS 原文与代码路径：每次激活新建实例、无在飞检查）；
  - `CI_InstancePerEntity` + **无在飞实例** ⇒ 正常建 run；
  - `CI_InstancePerEntity` + **有在飞实例** + `bRetriggerOnActive = false`（**默认**） ⇒ **驳回 `SAR_AlreadyActive`**（"该实体上已有在飞的同技能实例"——两个来源对调用方是同一件事，**MUST NOT** 拆成两个枚举值）；
  - `CI_InstancePerEntity` + **有在飞实例** + `bRetriggerOnActive = true` ⇒ **顶替**（见下条）；
  - **"在飞实例"的判据 MUST 取该条目的 `RunHandles`**（它按构造就是 per-unit，无需跨桶扫描）；**在飞判据 MUST 按句柄经池复核**（账本里可能有已归还的陈旧句柄 ⇒ 复核失败即视为无在飞）；
- **顶替语义（MUST，刻意与 GAS 分歧）**：① 先查旧 run 当前时段的可打断性，**不可打断 ⇒ 驳回 `SAR_AlreadyActive`**（旧 run 原样继续）；② 可打断 ⇒ **终止旧 run**——发 `OnCastInterrupted`、**MUST NOT** 发 `OnCastCompleted`、**不起旧 run 的主链**、归还旧 run 池槽位、摘除其 `RunHandles` 条目；③ 再建新 run。**顶替 = 打断的一种终止形态**，MUST NOT 新增第三枚终止事件；
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

### Requirement: 激活期参数快照绑定

激活成功后 MUST **在门禁全过之后、任何后续读取之前**构建并绑定一份参数快照——它是"施法期间读到的数值不再漂移"的唯一机制：

- MUST 提供 `BuildSkillSnapshot`：按 `FTcsSkillDefData.Params` **逐行求值一次**并冻结进 `FTcsCastRun.ParamSnapshot`；**写入点规则 MUST 与状态侧逐字同款**（覆盖值优先且不再过值约定 / 否则求值后按该行 `ValueConvention` 转规范值 / 重建语义 = 先 `Reset` 再填）；
- **复用面与不复用面 MUST 分清**（防后人误读为"建了第二套快照"）：**快照类型** `FTcsParamSnapshot` 与**读取适配器** `FTcsStateParamTableReader` + `FTcsStateSnapshotScope` MUST **复用**，MUST NOT 新建第二套快照类型、也 MUST NOT 出现第二处键查找语义；但**构建函数** MUST 另有一份——状态侧的 `FTcsStateOps::BuildSnapshot` 形参硬编码为 `const FTcsBuffDef&`，技能侧**物理上无法复用**。两份构建函数 MUST 保持同一套写入点规则（判据 = 规则同款，而非函数同一）；
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
- 起链 MUST 用 `ExecuteChain(CastChainId, Context)`，且 **`Context.RunSource` MUST = `FTcsCastRun.RunSource`**——这是台账 `CHAIN-7`（"链挂的账本条目没有框架侧回收触发点"）的**闭合依据**：施法运行态是链运行态的**第一个长生命周期持有者**，故链内 `ModifyAttribute` 挂的条目可按该锚点回收。**回收例程本身归 Task 5 Step 3**（施法终结三路统一回收），本 Task 只落锚点；
- `Context.Caster` MUST = 施法单位、`Context.Instigator` MUST = 施法单位（本轮无独立的发起者参数）；
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
- 载荷 MUST 为**单一 `USTRUCT`**（`FTcsCastEventPayload`，四枚共用一种形状，同状态侧六枚共用 `FTcsStateEventPayload` 的先例）：至少含 `FTcsCastRunHandle Run` / `FTcsCombatEntityHandle Unit` / `FGameplayTag DefTag` / `int32 Level` / `FTcsSkillEntryHandle EntryHandle`；
  - **`Run` 是句柄不是指针**：订阅者若要读运行态细节，在回调内用句柄复核——**"先广播后归还槽位"的纪律**保证那一刻 run 仍在册；
  - **句柄字段的反射性**：`FTcsCastRunHandle` 已是 `USTRUCT(BlueprintType)`（Task 2 落地）⇒ 载荷可反射，无需二次展平；
- **本 Task 的产生者**：只发 `Tag_TcsEvent_Cast_Started`（激活成功后）与 `Tag_TcsEvent_Cast_Interrupted`（**顶替路径**）。`PhaseChanged` / `Completed` 两枚**只落词与载荷，产生者归 Task 5**（时段推进与自然终结）——**MUST NOT** 在本 Task 假装它们已产生；
- 广播 MUST 走总线**立即通道**（`PublishImmediate`——同状态事件的既定口径，订阅方在调用返回前收到）；**总线订阅是精确 tag 匹配**（非层级）⇒ 订阅方 MUST 逐叶子具名，本 Task MUST NOT 依赖父 tag 收全。

#### Scenario: 激活发出开始事件且载荷身份完整

- **WHEN** 激活成功
- **THEN** `TcsEvent.Cast.Started` 被广播一次，其载荷的 `Run` / `Unit` / `DefTag` / `EntryHandle` 与该次激活一致

#### Scenario: 顶替发出打断事件而非完成事件

- **WHEN** 顶替路径被触发
- **THEN** `TcsEvent.Cast.Interrupted` 被广播一次（载荷 `Run` = **旧** run 的句柄），且 `TcsEvent.Cast.Completed` **零次**

#### Scenario: 未实现产生者的事件不假装已产生

- **WHEN** 检查本轮代码路径
- **THEN** 没有任何位置广播 `PhaseChanged` / `Completed`（两枚只有 tag 声明与载荷形状；其产生者归 Task 5）——**MUST NOT** 以"事件已声明"为由 claim 它们可用
