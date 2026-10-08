# Change: 施法运行态与六道具名门禁（`TryActivate` / `FTcsCastRun` / 事件四枚）+ 删除技能侧 AttrCapture

> 对应计划 `PLN-R6` Task 3（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`）。
> **本轮同批并入一处对已归档能力的删除**：技能侧 `AttrCapture` 整套判定为**机制重叠、零消费者**（依据见 What Changes 第 3 条），
> 字段与机制一并删除。删除动作按用户 2026-10-08 裁定**并入本提案**，不另开独立删除提案。

## Why

Task 2 让"某个单位会哪些技能"成为**可查询的事实**，但技能**还放不出来**——没有激活入口、没有施法运行态、
没有门禁裁决。Task 3 交付那个入口，它同时是 Task 4（参数链）与 Task 5（时段与打断）的前置。

设计 §4 把 `TryActivate` 定义为**显式门禁序列**并逐道具名原因（修 TCS 库外报告 09 的"门禁内联无钩子"缺陷）——
本提案把这个序列结构完整落地，其中两道（冷却 / CanAfford）因真分支归 `R6.5` 而落"具名枚举位 + 恒过的占位判据"，
使**门禁序列的结构可测**，只是这两道的非默认分支留待 `R6.5`。

起草前按 `MEM-20260918-01` 的"照计划实现前先扫清单"做了规范扫描，抓到**四处计划与实况不符**
（其中一处为**已归档规格的正面冲突**、一处为**全仓惯例冲突**），必须在提案期当场关掉而不是留到实施期：

| # | 计划原文 | 实况 | 处置 |
|---|---|---|---|
| 1 | `skill-registry` 规格称 `RunHandles`「**本轮恒为空数组**：写入者是 Task 3 的激活路径」 | Task 3 正是那个写入者 ⇒ 规格与实现将正面冲突 | **MODIFIED `skill-registry`**（同 Task 2 Step 0 的处置口径） |
| 2 | Task 3 Step 5 要按 `CastConfig` 捕获属性进 `CapturedAttrs` | 该机制**判定为重叠**（见第 3 条）⇒ 捕获整体删除 | 该冲突随删除**自动消解**；**不改** `attribute-pipeline` |
| 3 | `FTcsCastRunHandle` 只是纯句柄值类型 | 池的 `Allocate` / `Free` / `Resolve` 收发 `TTcsInstanceHandle<>`，而该句柄**无 `GetInner` / `SetInner`** ⇒ 无法进池 | 本 Task 补这一对（**唯一转换点**，照 `FTcsChainRunHandle` 的既有形态） |
| 4 | Task 3 Step 4 写"复用 `FTcsParamSnapshot` + `FTcsStateParamTableReader` + `FTcsStateSnapshotScope`，**不建第二套快照**" | 但构建函数 `FTcsStateOps::BuildSnapshot` 的形参**硬编码为 `const FTcsBuffDef&`** ⇒ 技能侧**无法复用** | 复用面 = **快照类型与读取壳**（不建第二套）；**构建函数按 Def 类型各一份**（`BuildSkillSnapshot`），写入点规则逐字同款 |

另有两处计划措辞必须当场关掉（不留到实施期）：

- **`MainChainStart` 的"四档"**：时段推进归 Task 5 ⇒ 本 Task 物理上**只能实现 `MCS_OnCastStarted`**。
  其余三档按"命中未实现档 MUST 具名报出、MUST NOT 静默不生效"处置（静默不生效是本仓反复拦的错误形态）。
- **`ESkillActivateResult` 的裸枚举值名**（`EntityNotReady` / `Success`）：全仓 `UENUM(BlueprintType)` 的枚举值
  **无一例外带 2–4 字母前缀**（`EAR_` / `ESRC_` / `TAO_` / `EDP_` / `CI_` / `MCS_` / `CQM_`）；唯一的裸名族
  `ETcsStateStackDecisionKind` **不是 UENUM**。⇒ 取 `SAR_` 前缀（gate 的真实诉求是"宿主能查出为什么放不出来"，
  故 MUST 保持 `BlueprintType`）。

## What Changes

### 1. ADDED `skill-cast-runtime`（新能力，5 需求）

- **施法运行态与实例池**：`FTcsCastRun`（**非反射**池内运行态）+ `TTcsInstancePool<FTcsCastRun, FTcsCastRunTag>` +
  `FTcsCastRunHandle` 的 `GetInner` / `SetInner` 转换对；建 run 后**回填** `FTcsLearnedSkillEntry::RunHandles`，
  终结时摘除；**GC 纪律**——`ParamSnapshot.SourceRef` 位可持对象引用，而池住 `TArray` 非 `UPROPERTY` 容器
  ⇒ 门面 `AddReferencedObjects` MUST 覆盖池内每个 run 的快照条目（同状态门面对实例快照的既有处置）。
- **六道具名门禁与激活入口**：`ESkillActivateResult`（`UENUM(BlueprintType)`，`SAR_` 前缀，七值 = 六道拒绝 + 成功）
  + `TryActivate(...)`；门禁序 = **实体 Ready → 已学 → 冷却（`R6.5` 占位，恒过）→ Instancing 四路分流 →
  CanAfford（`R6.5` 占位，恒过）→ 定义可解析**；**拒绝路径 MUST NOT 有副作用**（纯查询语义）。
  Instancing 四路 = 并存 / 正常建 run / 驳回 `AlreadyActive` / **顶替**（发 `OnCastInterrupted`、
  **MUST NOT** 发 `OnCastCompleted`、不起旧 run 主链、归还池槽位、摘账本 `RunHandles`）；
  顶替的可打断性判据取 `bInterruptibleDefault`（= `ECastQueryMode::DefSwitches` 档，Task 1 已交付的**默认档**，
  非临时实现），Task 5 落地 `IsInterruptibleNow()` 时该档行为**逐字不变**。
- **激活期参数快照绑定**：`BuildSkillSnapshot`（门禁全过 → 按 `FTcsSkillDefData.Params` 逐行求值一次 →
  `FTcsCastRun.ParamSnapshot` 绑定 → 后续一切读快照）；写入点规则与状态侧**逐字同款**
  （覆盖优先 / 值约定在写入点转规范值 / 重建语义）；复用 `FTcsParamSnapshot` 与 `FTcsStateParamTableReader`，
  **MUST NOT** 建第二套快照类型或第二处键查找语义。
- **主效果链起链时点**：`CastChainId` 解析 + `MainChainStart`（**本 Task 只实现 `MCS_OnCastStarted`**，
  其余三档具名 `Warning`）；`Context.RunSource` = `FTcsCastRun.RunSource`（该锚点是台账 `CHAIN-7` 的闭合依据，
  其回收例程归 Task 5 Step 3）。
- **施法事件四枚**：`OnCastStarted` / `OnCastPhaseChanged` / `OnCastCompleted` / `OnCastInterrupted`——
  **原生 tag + 模块导出宏**（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**，跨模块消费面缺
  `TCSSKILL_API` 必 `LNK2001`）；落 `TcsEvent.Cast.*`（**既有 `TcsEvent` 根下**，无需新增根）；
  载荷为单一 `USTRUCT`（四枚共用形状，同状态侧六枚共用 `FTcsStateEventPayload` 的先例）。
  **总线订阅是精确 tag 匹配** ⇒ 逐叶子具名，不靠父 tag 收全。本 Task 只**发** `OnCastStarted` 与
  （顶替路径的）`OnCastInterrupted`；另两枚的**产生者**归 Task 5，本 Task 只落词与载荷。

### 2. MODIFIED `skill-registry`（`RunHandles` 由"恒空"改为激活期回填）

「已学技能账本（per-unit 条目与代际句柄）」需求内 `RunHandles` 的语义由"**本轮恒为空数组**、写入者是 Task 3"
改为"**激活期回填、终结时摘除**（Task 3 起）"。**同批订正一处既有的字段清单偏差**：
该需求原文写条目字段**恰好**四项，而落地类型实为六项（`Handle` 与 `Unit` 亦在其中，两者在类型注释里各有存在理由）
⇒ 清单按实况订正。

### 3. MODIFIED `skill-def-asset`（**删除技能侧 AttrCapture 整套**）

**判决依据（用户 2026-10-08 质询后逐条取证）**——三条同时成立：

1. **零消费者**：`FTcsDamageFlowContext::CapturedAttrs` 是"0 填充 0 读取"（台账 `WAIT-7` 原话）；
   技能侧 `CapturedAttrs` **全仓无任何类型读它**（草案期实测）。`TcsDamageFlowContextView.h:35` 更把
   `CapturedAttrs` 归为"**框架簿记面**……宿主脚本**无消费场景**"——连宿主面都没打算给它。
2. **机制重叠（真实重叠，非疑似）**：设计给 AttrCapture 写的用途是"捕获命中 → **改 `CapturedAttrs`**
   （随流程消失，零账本污染）"（`09-module-damage.md:31`）。而**流程属性黑板**已经提供同一件事且更强——
   `FTcsFlowAttributes`"作用域 = 流程用完即弃"、`FlowModify` 步骤是"**数据化黑板写入**"
   （`09:29` / `09:53`）。⇒ 同一个"流程局部可变值空间"存在两份。
3. **技能侧已被参数快照占满**：技能侧的两个冻结需求各有一半归属——技能**自己的参数**由
   `FTcsCastRun.ParamSnapshot` 冻结（`AttributeScaled` 类源在快照构建时即取值，捕获不再必要）；
   **流程内的工作值**由 `FTcsFlowAttributes` 承担。**剩下的空间说不出一个技能侧独有的业务场景**，
   且设计语料（`SPEC-04-skill` §2 / D5-12 v2）**没有给出技能侧独有的用例**——属一处设计表述空缺。

**删除面（逐项）**：

- 删 `Source/TcsSkill/Public/Def/TcsCastAttrCapture.h`（含 `ETcsCastAttrCaptureFrom`）；
- 删 `FTcsSkillDefData::AttrCaptureList` 字段；
- **不落** `FTcsCastRun.CapturedAttrs`（计划 Task 3 Step 2 字段表相应删一项）；
- **不建** `FTcsSkillAttributeAccess`：删除后 `TcsSkill` 对属性门面**零依赖**（实测：`Source/TcsSkill/`
  对 `UTcsAttributeSubsystem` / 属性类型的引用 = 0）⇒ 建第三处白名单即"为零消费者预建"。
  这正是 `attribute-pipeline`「属性门面的解析点」要求的"两模块各自白名单薄壳"**不必扩成三处**的原因
  ——该规格**不改**；
- 同批回写设计文本三处（`SPEC-04-skill` §2 该条 + §1 依赖行的"AttrCapture"字样、`LOG-03-skill` 的 D5-12 v2 行）；
- **`09-module-damage.md:31` 只标注技能侧那半已废**——**伤害流程侧的 `WAIT-7` 不动**
  （两层机制独立，`SPEC-04-skill:28` 明文；干掉技能侧**不会**顺手关闭 `WAIT-7`）。

**删除的动作面（MUST 如实登记）**：`AttrCaptureList` 是 Task 1 **已取证归档**的交付
（`skill-def-asset` 规格 + `EVID-2026-10-07-skill-def-asset` 的"能存能取能往返"读数 + 已提交 `bb0e05d`）
⇒ 本次删除**必然**使该证据里 `AttrCaptureList` 相关的读数失效。处置 = 该证据包**按"被后续任务取代"就地留痕**
（保留原文、加取代说明），**不重跑 Task 1 全部取证**。

### 4. MODIFIED `instance-handle-pool`（**新增 GC 专用只读遍历**）

**起因 = 首轮 PIE 实测暴露的一处真实缺陷**：`TTcsInstancePool` 的**每一个**正常进入点
（`Allocate`/`Free`/`Resolve`/`IsValid`/`ForEach`/`Reset`）都无条件 `ensure(IsInGameThread())`
（D0-4 单游戏线程假设），而**GC 的引用收集不在游戏线程上** ⇒ 门面在 `AddReferencedObjects` 里
调 `CastRuns.ForEach` 时，**每次 GC 都触发一次** `Ensure condition failed: IsInGameThread()`。

**实测证据**（首轮快照 `Saved/Task3/snapshot-R6Task3-run1.log`）：
`:2151` 与 `:2176` 两条 `ensure`，**callstack 为**
`FRealtimeGC::CollectReferencesForGC` → `UTcsSkillSubsystem::AddReferencedObjects` →
`TTcsInstancePool<FTcsCastRun,FTcsCastRunTag>::ForEach()`（`TcsInstancePool.h:91`）。
**时间戳 `12:07:27` 早于探针运行 `12:09:48` 两分二十一秒** ⇒ 它**与探针无关、与池空不空无关**，
GC 一跑就报。**症状极具隐蔽性**：`ensure` 每站点每进程只报一次 ⇒ 表现为"偶尔一条红字"。

**改动**：池新增 `ForEachForGC(TFunctionRef<void(const TInstanceType&)>)`——**不断言游戏线程**、
**纯读**、**判据与 `ForEach` 逐字相同**（Index 升序 + 代际奇偶跳空闲槽）。**它的唯一合法调用者是 ARO**
（规格里 MUST 写明这点：拿它做业务遍历等于静默放弃单线程纪律）。门面侧新增 `ForEachCastRunForGC`
转调它，`AddReferencedObjects` 改走该口。

**为什么修在池而不是绕过它**：`CastRuns` 是门面私有成员但池的存储也是 `private`（无 friend）
⇒ 门面**读不到**池内数组；而"把裸遍历开成池的公开口"正是本改动做的——所以规格必须同时钉住
"该口不得用于常规逻辑"，否则等于给单线程纪律开了后门。

### 5. 计划侧回写（同批，落 `PLN-R6`）

- Task 1 交付物与 Step 2：删 `TcsCastAttrCapture.h` 与 `AttrCaptureList`；
- Task 3 Step 2 字段表：删 `CapturedAttrs`；Task 3 Step 5：整体删（捕获机制）；
- Task 3 交付物：**不加**属性白名单（原计划未列，本提案明确不列）；
- Task 3 Step 6 / Step 8：`MainChainStart` 由"四档"收窄为"一档实现 + 三档具名 Warning"；
- Task 3 Step 8 ①：删"两次各自推完"（该半句依赖 Task 5 的时段推进）；
- 变更记录追加本轮。

## 明确不做（非目标）

- **不做时段推进**（`FTcsPhaseSpan` 的到期堆驱动 / `PhaseIndex` 推进 / `OnCastPhaseChanged` 的产生者）
  ——归 Task 5。**连带边界（MUST 如实登记）**：本 Task 的施法运行态**没有自然终结路径**
  （全时段走完归 Task 5），终结只经**顶替**一路可达。
- **不做打断的完整结算**（`CancelCast(run, Reason)` 与来源优先级裁决）归 Task 5；
  **不做施法终结三路统一回收例程**（台账 `CHAIN-7` 的闭合点）归 Task 5 Step 3。
- **不做冷却与 Cost 的真分支**（`R6.5-a` / `R6.5-d`）：两道门禁只落**具名枚举位 + 恒过的占位判据**
  （冷却 = 空轨道恒可放；Cost = 不消耗档恒可付）。**MUST NOT** 引用任何尚未定名的枚举值
  （`FCostConfig.Policy` 连枚举名都未定，引用即制造第二真相）。
- **不做参数链修正器物化**（Task 4）；**不做链重定向栈**（Task 5）。
- **不做 `ITcsCastStateQuery` 三级实现**（`DefSwitches` / `PhaseTable` / `Custom`）——归 Task 5；
  本 Task 只直读 `DefSwitches` 档的字段。
- **不收 `TryActivate` 的"施法上下文"形参**（设计 §4 的 `Context`）：本 Task 无消费者
  （链侧 `SelectTargets` 自填目标集、`Instigator` 缺省 = 施法者）⇒ 收一个无人填的形参即"零消费者预建"。
  归属 = 出现"宿主指定目标集施法"的真实消费者时。
- **不做属性捕获**（见 What Changes 第 3 条）；**不做技能侧属性白名单**；
  **不动** `TcsSkill.Build.cs` 依赖集（仍不含 `TcsDamage` / `TcsTargeting` / `TcsIntegration`）。

## Impact

- **Affected specs**：
  - `skill-cast-runtime`（**ADDED** × 5 需求）
  - `skill-registry`（**MODIFIED** × 1 需求：「已学技能账本（per-unit 条目与代际句柄）」——`RunHandles` 语义 + 字段清单订正）
  - `skill-def-asset`（**MODIFIED** × 1 需求：「技能 Def 数据形状」——删 AttrCapture 条目 + 补一条 scenario）
  - `instance-handle-pool`（**MODIFIED** × 1 需求：「实例池的分配释放与解析」——新增 GC 专用只读遍历 `ForEachForGC` + 一条 scenario）
  - **不改**：`attribute-pipeline`（消费模块仍为两个）、`gameplay-tag-governance`（事件落既有 `TcsEvent` 根，无新根）
- **Affected code**（插件仓 TCS）：
  - 新：`Source/TcsSkill/Public/Skill/TcsCastRun.h`（`FTcsCastRun` + `FTcsCastRunTag`）、
    `Skill/TcsCastOps.h`、`Skill/TcsCastEvents.h`、`Def/TcsSkillActivateResult.h`、
    `Private/Skill/TcsCastOps.cpp`（+ 按 300 行惯例分片）、`Private/Skill/TcsCastEvents.cpp`、
    `Private/Skill/TcsCastOps_Snapshot.cpp`、`Private/Skill/TcsCastOps_Events.cpp`
  - 改：`Source/TcsSkill/Public/Skill/TcsCastRunHandle.h`（补 `GetInner` / `SetInner`）、
    `Public/TcsSkillSubsystem.h`（激活口 + 运行态池 + `AddReferencedObjects` 扩面 + `friend`）、
    `Private/TcsSkillSubsystem.cpp`（`Deinitialize` 清池 + ARO）、`Private/TcsSkillSubsystem_Registry.cpp`（薄壳转发）
  - **删**：`Source/TcsSkill/Public/Def/TcsCastAttrCapture.h`
  - 改（删字段）：`Source/TcsSkill/Public/Def/TcsSkillDefData.h`
  - **改（TcsCore，本 Task 唯一的跨模块改动）**：`Source/TcsCore/Public/Pool/TcsInstancePool.h`——新增 `ForEachForGC`（GC 专用只读遍历，见 What Changes 第 4 条）。**一处如实登记**：这是 Task 3 触碰的**唯一**非 `TcsSkill` 文件，理由是缺陷本身在池的"入口全部断言线程"这一设计上，**绕过它（在门面里裸读私有数组）物理上不可行**（池存储是 `private` 且无 friend）。
- **跨仓**（宿主 LAC）：新建 `Source/TcsDev/Public|Private/Dev/TcsDevSkillCastProbe*`（施法面装置，
  照 `TcsDevSkillRegistryProbe` 三面分片体例：常规 / 拒绝 / 内部件）+ `TcsDevModule.cpp` 注册 +
  `Config/DefaultGameplayTags.ini` 补检查词。
- **分工遵循设计 §3.1**：`UTcsSkillSubsystem` 仍为**门面薄壳**，六道门禁的裁决流程与运行态操作全在
  **`FTcsCastOps`**（静态函数族，`friend` 开放最小私有集）——同 `FTcsStateOps` / `FTcsSkillOps` 的既有处置。
- **命名**：`ESkillActivateResult`（`SAR_` 前缀）；`FTcsCastRun` / `FTcsCastRunTag` / `FTcsCastOps` /
  `FTcsCastEventPayload`；事件 `Tag_TcsEvent_Cast_{Started,PhaseChanged,Completed,Interrupted}`。
