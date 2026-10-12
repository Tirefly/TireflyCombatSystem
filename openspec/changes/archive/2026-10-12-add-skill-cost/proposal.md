# Change: 技能消耗的运行态（策略 × 时机 / fail-closed 判定 / 提交与 TOCTOU）

> 对应计划 `PLN-R6.5-1`（`Documents/combat-system-design/plans/plan-r6.5-cooldown-and-cost.md`）
> **Task 2 + Task 3 的 Cost 半 + Task 4 的 `CostApplied`**。
> 依赖 `add-cooldown-and-cost-types`（已交付数据形状）。
>
> **★ 本稿是补开的（2026-10-11，MUST 如实记录，MUST NOT 读成"当初就开过"）**：Task 2~4 的代码
> **先于本提案落地**（实现在无提案的情况下完成，属流程偏离，已登记在 `PLN-R6.5-1` 的执行 ledger）。
> 补开本提案的目的是**让规格如实反映已建成的事实**——否则 `specs/skill-cost` 会永久停在
> "只有数据形状"，与代码不符。**"提案先行"的审查价值已经损失**，这一点必须在收束报告里如实告知。

## Why

`add-cooldown-and-cost-types` 只交付了**数据形状**（`ETcsCostPolicy` / `ETcsCostTiming` /
`FTcsCostConfig`），而 R6 留在 `TcsCastOps.cpp:237` 的**门禁⑤** 仍是带 `R6.5 占位` 注释的空分支
（判据恒过）。本变更把它变成真的，并交付提交路径：

- **门禁⑤ 答"现在付得起吗"**，实现单点 = `FTcsCostOps::CanAffordCost`；
- **三类"配错"一律 fail-closed**（本仓既有惯例：`TcsFlowStepConditions.h:56` /
  `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72` 三处措辞几乎逐字相同：
  "**不静默通过**：静默会让'条件写错'表现成'步骤照跑'"）；
- **扣减走 `SetBaseValue` 直写**（读基值 → 减 → 写回），**MUST NOT** 用 `ApplyModifier` 挂修正器；
- **四个提交点**（`OnCastStarted` / `OnCastCompleted` / `OnEvent` / `Manual`）各按其时机提交，
  且**除激活路径那一处外都重查一次 `CanAffordCost`**（TOCTOU 防护）。

## What Changes

### 1. ADDED `skill-cost`（形状 → 运行态）

- **门禁判定** `CanAffordCost(Subsystem, Unit, Config, OutReason)`——**五类分开处置**：

  | `Policy` | 条件 | 返回 | 日志 |
  |---|---|---|---|
  | `ECP_None` | — | `true` | **无**（正确声明"不消耗"） |
  | `ECP_CustomFragment` | — | 按 `None` 处理 ⇒ `true` | **具名 `Warning`**（含档名） |
  | `ECP_ResourceAttr` | `ResourceAttr` 无效 | `false` | **`Warning`** |
  | `ECP_ResourceAttr` | `Amount < 0` | `false` | **`Warning`** |
  | `ECP_ResourceAttr` | `Amount == 0` | `true` | 无（"消耗 0 点"就是不消耗） |
  | `ECP_ResourceAttr` | 单位**未持有**该属性 | `false` | **`Warning`**（与"不足"可区分） |
  | `ECP_ResourceAttr` | 正常 | `基值 >= Amount` | 不足时 `Log`（正常业务路径） |

  - **区分点 = "正确声明不消耗"（`None` / `Amount == 0`，零日志）与"想配消耗但没配清"
    （无效 tag / 负值 / 没加属性，拦住 + `Warning`）是两件事**；混成一类会把后者**伪装成前者**；
  - **`Amount < 0` 尤其必须拦**：扣减式是 `Base - Amount` ⇒ 负值会让技能**倒给**资源
    （`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态，不是"配置不生效"；
  - **MUST 先确认"持有"再比基值**：底层对**未持有**返回 `0.0`，与"持有且基值为 0"**不可区分**
    ⇒ 不先问持有，就会把"属性配错 / 压根没加"表现成"**付不起**"——两种红字含义完全不同；
  - **MUST NOT ensure**：付不起是**正常业务路径**，不是契约违规。
- **提交** `CommitCost(Subsystem, Unit, Config, OutApplied)`——**唯一的扣减实现**；
  - **扣减载体 = `SetBaseValue` 直写**（读基值 → 减 → 写回），**MUST NOT 用 `ApplyModifier`**。
    判据三条：① **语义**——`SetBaseValue` 的注释原文即"改写属性基础值（等级成长等宿主升级事务的
    落点）"，而"消耗资源"正是**永久改变基值**；挂修正器表达的是"某个来源带来的增减"，
    与"花掉了"不是一回事；② **来源锚点矛盾**——`ApplyModifier` **必须**给 `Source`，
    而资源扣减**不可**挂施法运行态来源（施法一终结 ⇒ 级联摘除 ⇒ 扣掉的资源回来了），
    挂"长寿命来源"则要发明一个**零撤销消费者**的新发号器；③ **`CHAIN-8` 已立判据**——
    修正器层效力**不等同**于直写基值（会被 `TAO_Override` 整体抹掉、参与 `PercentAdd`/`Mul` 复利、
    可按来源摘除）⇒ "花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的；
  - **读的 MUST 是基值而不是 `EvaluateCurrent`**：后者是**折叠后**当前值（含全部修正器），
    拿它减会在有修正器时**多减**（扣掉的是"当前值 − Amount"，写回的却是基值）；
  - **MUST NOT 用属性侧 `BeginBatch`/`Commit` 造"事务"包裹**：`D5-10` 的"激活事务"指**逻辑上的一次激活**，
    不是 M2 属性事务批——包起来会让本次扣减与其它属性改动**共享一个今天不存在**的提交点；
  - **提交失败 MUST 被报出**（`SetBaseValue` 对"单位未注册 / 属性名为空 / 未持有"返回 false）
    ⇒ MUST NOT 假设它一定成功（否则 `PayCost` 会报成功而资源没扣）。
- **`PayCostForEntry(Subsystem, Unit, EntryHandle)`**——**`ECT_Manual` 档的专用提交入口** + 宿主重试口；
  - **自动档 MUST NOT 走它**（自动档直调 `CommitCost`）：自动档的提交点**已经知道**自己该不该提交，
    走本口会多一层"按 Timing 判断"的分支——那正是"同一件事两个入口"；
  - **内部重查一次 `CanAffordCost`**（TOCTOU 防护：门禁⑤ 通过到此刻之间资源可能已被花掉），
    判据与门禁**同一函数**（MUST NOT 各写一份）。
- **四个提交点的落位**：
  - `ECT_OnCastStarted` ⇒ 激活路径的"**门禁⑤ 之后、建 run 之前**"（判据三条见下）；
  - `ECT_OnCastCompleted` ⇒ `FinishCastRun` 的 Timing 分派（**只在 `ECFR_Completed` 路触发**）；
  - `ECT_OnEvent` ⇒ 总线订阅（`UTcsCostEventRelay`，见第 3 节）；
  - `ECT_Manual` ⇒ **框架零动作**（宿主自己调 `PayCost`）。
- **`ECT_OnCastStarted` 的落点判据（MUST，用户 2026-10-11 确认）**：
  ① **语义**——`OnCastStarted` = "激活事务内提交"，而"激活成功"的分界线就是**建 run**
  （建了 run 才存在"在飞的施法"，也才会有终结路径去回收它）；
  ② **失败面更干净**——若在建 run 之后提交且失败，就得**回滚一个刚建的 run**
  （池槽位、账本 `RunHandles` 回填、事件广播都要撤），而建 run 前失败只需直接返回，零残留；
  ③ **顺序与判据一致**——门禁⑤ 刚判"付得起"，紧接提交是同一事实的两次使用，TOCTOU 窗口最小。

### 2. ADDED `attribute-pipeline`（新增一个**非确保**的基值读口）

`UTcsAttributeSubsystem` MUST 提供 `TryGetBaseValue(Unit, Attribute, double& Out) -> bool`：

- **为什么必须有它**：Cost 的 `ECP_ResourceAttr` 档要"读基值 → 减 → 写回"，
  而门面此前只有 `EvaluateCurrent`（**折叠后**当前值）与 `PeekPending`（未提交候选值）
  ——**两者都读不到基值**。底层 `FTcsAttributeStore::GetBaseValue` 早已存在，缺的只是转发口；
- **为什么是 `TryGet` 形态而不是纯 `double` 口**：底层对**未持有**返回 `0.0`，与"持有且基值为 0"
  **不可区分**——而调用方**必须**区分这两件事。纯 `double` 口要求调用方**连调两次**
  （先问持有、再取值），而两次调用之间**没有任何东西保证两个答案一致**
  ——那正是本变更要防的 TOCTOU 形态。`TryGet` 一次给出两个答案、天然自洽；
- **非确保**（走 `ResolveStore` 而非 `GetStore`）：读取是正常查询路径，"单位未注册"不是契约违规
  ——调用方拿到 false 就能如实报出"该单位没有这个属性"，**不该**为此留红字；
- **MUST NOT 从 `TcsSkill` 直伸 `ResolveStore` + `FindInstance`**：跨模块直读会**击穿 M2 门面**
  （属性存储的形状从此成为跨模块契约，M2 想改 `BaseValue` 的载体就得改两个模块）
  ——判据 = **门面是模块的对外唯一入口**。

> **为什么是 ADDED 而不是 MODIFIED（一处自我订正，MUST 记）**：本提案初稿曾写
> "MODIFIED `attribute-pipeline`"——**那是错的**。判据：`attribute-pipeline` 的既有六条需求
> （五带折叠 / 按需重算与脏标记 / 值域收口 / 依赖登记 / 变更广播 / 管线可见性 / 解析点）
> **本轮一条都没改**；"新增一个门面读口"按 `openspec/AGENTS.md` 的"何时用 ADDED、何时用 MODIFIED"
> 是 **ADDED** 的形状（"引入可作为独立需求存在的新能力或子能力"）。强行写 MODIFIED 还得把某条
> 既有需求**整条粘贴**（归档器整体替换 requirement）⇒ 造出与本轮无关的巨型 delta +
> 粘贴遗漏会**静默丢失**既有场景（`TOOLS-9` 同族教训）——**风险远大于收益**。

### 3. ADDED `skill-cost` 的事件与 `OnEvent` 中继

- `TcsEvent.Cast.CostApplied` + `FTcsCostEventPayload`
  （`{EntryHandle, Unit, DefTag, ResourceAttr, Amount}`；`D5-16` 的 `SkillId` 用 `DefTag` 取代）；
- **`CanAfford` 失败 MUST NOT 发它**（`D5-16` 明文："CanAfford 失败不发——`TryActivate` 返回值覆盖"）：
  门禁拒绝面已有 `SAR_CannotAfford` 这个具名读数，再发一枚"没扣成"的事件纯属噪音；
- **只有真的扣了才发**（`Amount <= 0` / `Policy != ECP_ResourceAttr` ⇒ 零动作、零事件）；
- **`UTcsCostEventRelay`**（`ECT_OnEvent` 档的落点）：
  - 它是 `UTcsEventHandler` 派生（**裁决 2a：事件类型 → Handler CDO**），
    **一个共享 CDO + 一张等待登记簿**（订阅数 = 不同 tag 数，**不是**条目数）——
    每条目一个实例会造出 N 个行为完全相同的对象；
  - **订阅寿命（最危险的一处）**：条目被撤销 / 单位被注销 MUST **退订**，
    否则回调里的条目句柄悬空 = **use-after-free**；某个 tag 下**最后一个**等待者被摘掉时 MUST 退订；
  - **`TriggerEventTag` 无效 ⇒ 该档不成立** ⇒ 具名 `Warning` + **回退 `ECT_OnCastStarted`**。
    **回退 MUST NOT 取"什么都不做"**：那会让配错的技能**永远不扣费**（免费放技能），
    是比"提前扣"严重得多的静默漏洞；
  - **处理 MUST 两段式**：回调内 `PayCostForEntry` 会经 `SetBaseValue` 触发 M2 管线重算并**广播属性变更事件**
    ⇒ 订阅者可在其中重入、甚至撤销条目 ⇒ **先拷贝一份待处理句柄，再逐个处理**。

### 4. 为什么本提案**不改** `skill-cast-runtime`

同 `add-cooldown-multitrack` 的同名说明：`FinishCastRun` 的既有契约**逐字未动**
（本轮只在它内部新增了触发点调用）⇒ 按"不是明确修改既有需求就在 ADDED 下新增"，
本提案对终结侧触发点的判据**写进 `skill-cost` 自己的需求**，`skill-cast-runtime` **一字不改**。
强行写 MODIFIED 会造出一份与本轮无关的巨型 delta，且任何粘贴遗漏都会静默丢失既有场景。

## 明确不做（非目标）

- **不做 `ECP_CustomFragment` 真实现**：它要求宿主脚本能写属性，而 `FTcsAttrModInstance` 是
  **纯 C++ struct、非反射**（台账 `SCRIPT-9` / `R5.5-h` 均未开工）⇒ 今天**物理不可达**。
  本轮**具名未实现 + 回退 `ECP_None`**（记 `Warning`，MUST NOT 静默）。
- **不做多资源消耗**（`SPEC-04-skill` §7 明文："v1 单资源，多资源 = 未来新原语"）。
- **不做返还**（`D5-4` 已砍除 `bRefundCostOnInterrupt`，理由原文"返还是政策不是引擎语义"）
  ——这也是 `ETcsCostTiming` **刻意没有** `OnCastInterrupted` 档的原因。
- **不做 Cost 折扣系数**（台账 `COST-1`；与 `COOLDOWN-1` / `STAT-11` 同批裁）。
- **不做冷却**（`add-cooldown-multitrack` 单独一份）。

## Impact

- **Affected specs**：`skill-cost`（**ADDED** ×2 需求：消耗运行态 + 事件与 `OnEvent` 中继）、
  `attribute-pipeline`（**ADDED** ×1：属性基值的非确保读口）
  —— **两条既有能力 `skill-cast-runtime` / `attribute-pipeline` 的既有需求一字不改**（理由见第 2、4 节）
- **Affected code**（插件仓 TCS）：
  - 新：`Public/Skill/TcsCostOps.h` + `Private/Skill/TcsCostOps.cpp`、
    `Public/Skill/TcsCostEvents.h` + `Private/Skill/TcsCostEvents.cpp`、
    `Public/Skill/TcsCostEventRelay.h` + `Private/Skill/TcsCostEventRelay.cpp`
  - 改：**`Source/TcsAttribute/Public/TcsAttributeSubsystem.h` + `Private/TcsAttributeSubsystem.cpp`**
    （本轮对 `TcsAttribute` 的**唯一**改动：+1 个非确保基值读口，零新逻辑）、
    `Private/Skill/TcsCastOps.cpp`（门禁⑤ 真实现 + `OnCastStarted` 提交与发事件 +
    `OnEvent` 登记 + `DispatchCostTiming`）、`Public/Skill/TcsCastOps.h`（+`DispatchCostTiming`）、
    `Public/TcsSkillSubsystem.h`（`PayCost` / `CanAfford` / `GetCostEventRelay` + 中继成员 + 1 friend）、
    `Private/TcsSkillSubsystem.cpp`（薄壳 + 中继延迟创建 + `Deinitialize` 退订）、
    `Private/Skill/TcsSkillOps.cpp`（三处配对注销中继登记）、
    `Public/Def/TcsSkillActivateResult.h`（`SAR_CannotAfford` 注记精确化）
  - **零新模块依赖边**（`TcsSkill.Build.cs` 不改——已含 `TcsAttribute`）
- **取证**：无头 `Tcs.Test.CooldownCost.Run` **23/0**（两轮复现）+ `.Reject` **7/0**
  （四类配置边界 / 重复 `PayCost` 零半提交 / `CustomFragment` 具名降级）；
  R6 回归 **24/0**
- **台账**：不消费也不新增（`COST-1` 仍待触发；`SCRIPT-9` 仍是 `CustomFragment` 档的阻塞前置）
