# Change: 冷却多轨道运行态（起冷 / 判可放 / 改轨 / 到期驱动）

> 对应计划 `PLN-R6.5-1`（`Documents/combat-system-design/plans/plan-r6.5-cooldown-and-cost.md`）**Task 1 + Task 3 的冷却半 + Task 4 的三枚冷却事件**。
> 依赖 `add-cooldown-and-cost-types`（已交付数据形状）。
>
> **★ 本稿是补开的（2026-10-11，MUST 如实记录，MUST NOT 读成"当初就开过"）**：Task 1~4 的代码
> **先于本提案落地**（实现在无提案的情况下完成，属流程偏离，已登记在 `PLN-R6.5-1` 的执行 ledger）。
> 补开本提案的目的是**让规格如实反映已建成的事实**——否则 `specs/skill-cooldown` 会永久停在
> "只有数据形状"，与代码不符。**"提案先行"的审查价值已经损失**，这一点必须在收束报告里如实告知。

## Why

`add-cooldown-and-cost-types` 只交付了**数据形状**（`ETcsCooldownTiming` / `FTcsCooldownTrack` /
`FTcsCooldownPolicy`），而"技能放不出来"的第二层理由**仍未成立**——R6 留在 `TcsCastOps.cpp:163` 的
门禁③ 仍是带 `R6.5 占位` 注释的空分支（判据恒过）。本变更把那一层变成真的：

- **门禁③ 答"现在放不放得出来"**，实现单点 = `FTcsCooldownOps::IsEntryOnCooldown`；
- **多轨并存**（自身轨 + 同组共享的组轨 ⇒ **真公共冷却（GCD）的纯数据表达**）；
- **CDR 快照于触发时刻**（`D5-15` 明文"中途变化不追溯"）；
- **到期驱动**复用 `TcsCore` 的到期堆（该头注释原文即"M5 冷却的消费底座"）；
- **三枚事件**与可变性 API **成对**（`MEM-20260902-12`）。

## What Changes

### 1. ADDED `skill-cooldown`（形状 → 运行态）

`skill-cooldown` 是**新能力**（由 `add-cooldown-and-cost-types` 首次引入、与本提案同批归档）
⇒ 本提案对它的运行态面是 **ADDED**（不是 MODIFIED）：

- **起轨**：`FTcsCooldownOps::StartCooldownOnEntry`——对每条轨道取时长 → 乘 CDR →
  算 `EndTimeSeconds = Clock.Elapsed + EffectiveDuration` → `PushExpiry`；
  **定位靠 lambda 捕获句柄**（`OwnerId` 只作归属读数，**MUST NOT** 把定位职责压给 `OwnerId`
  ——那会与 R6 已确立的堆使用约定造出第二真相）；
- **判可放**：`D5-15` 原文"可施放 = **全部自身轨道清空 + 所属组槽清空**"，写成一条短路遍历；
  边界：空 `Tracks` 恒可放 / 无效 `GroupTag` = 自身轨 / 陈旧组槽视为已清并记 `Log`
  （**MUST NOT 静默保留永久堵死**）；
- **CDR**：`CooldownPct` 契约键上的折叠结果，**在起冷那一刻取一次**；
  **钳制上下界写死为 `[0, 1]`**（负值不得变成冷却延长；`>1` 不得产生负时长；非有限值记 `Warning`）；
- **到期回调**：**第一条语句即整体复位该轨状态**——`CancelExpiry` 对**悬空**句柄是 `ensureMsgf`，
  而"到期回调正在跑"那一刻它自己的条目**仍然有效**（`FireDueItem` 在回调返回后才释放）
  ⇒ 复位使后续清理路径天然跳过（同 `FTcsCastRun::PhaseExpiryEntry` 的既有处置）；
- **改轨 / 清轨**：改完 **MUST 重挂到期堆**（`CancelExpiry` 旧的 + `PushExpiry` 新的）；
  **MUST NOT** 只改 `EndTimeSeconds` 而不重挂（那会让堆里旧时刻继续生效，
  症状 = "改了却不生效"或"提前到期"）；`Total` 只增不减（维持 `Remaining <= Total` 不变量）；
- **六个公开口**（`UTcsSkillSubsystem`）：查询四个（`IsOnCooldown` / `GetCooldownRemaining` /
  `GetCooldownTotal` / `GetCooldownTrackCount`）+ 可变性两个（`AdjustCooldown` / `ResetCooldown`）
  + **`StartCooldown`**（`ECT_Manual` 档的必需品——`D5-16` 只承诺了"改"与"清"，
  **都不是"开始冷却"**，没有它手动档物理上无法兑现）；
- **组槽住单位级**（`TMap<单位, TMap<组, 槽>>`）：公共冷却的作用域是**单位**
  ⇒ 平表会让两个单位的同名组串味；
- **生命周期配对**：`ReleaseEntryCooldowns` / `ReleaseUnitCooldowns`——
  冷却是本仓**第一个在账本条目上挂外部资源**（到期堆条目 + 单位级组槽）的机制
  ⇒ 撤销路径 MUST 在释放槽位**之前**撤锚点，否则锚点丢失 + 组槽永久堵死两条静默泄漏；
  **组槽刻意不随条目撤销而摘**（替它摘会替同组的其它技能解除公共冷却）。
- **终结侧触发点**：`DispatchCooldownTiming` 挂在 `FTcsCastOps::FinishCastRun`
  （四路终结的唯一汇聚点），分派表见 delta 的同名需求。

### 2. 冷却事件（三枚，同为 ADDED `skill-cooldown`）

新增 `TcsEvent.Cast.CooldownStarted` / `.CooldownUpdated` / `.CooldownEnded`
（**`On` 前缀刻意去掉**——`TcsEvent.Cast` 根下既有四枚零 `On` 前缀，按同族对称）
+ `FTcsCooldownEventPayload`（三枚共用一种形状；`D5-16` 的 `SkillId` 用 `DefTag` 取代——
`EffectiveDefId` 已整体移除，**MUST NOT 为它发明类型**）。

**根数不增**（仍 12——落既有 `TcsEvent` 根，"增根不增层"）；全部带 `TCSSKILL_API` 导出宏
（裸 `extern` 会 `LNK2001`）。

### 3. 为什么本提案**不改** `skill-cast-runtime`（一处刻意的自我订正，MUST 记）

本提案初稿曾声明"MODIFIED `skill-cast-runtime`（终结路径的冷却触发点）"——**那是错的，已删除**。
判据两条：

1. **`skill-cast-runtime` 是已归档的既有能力**，而本轮**没有改变它的任何既有需求**：
   `FinishCastRun` 的"四路汇聚 / 唯一分岔 / 回收动作"契约**逐字未动**，本轮只是在它内部**新增了两个
   触发点调用**。按 `openspec/AGENTS.md` 的"何时用 ADDED、何时用 MODIFIED"——**不是明确修改既有需求
   就在 ADDED 下新增**；而"新增一个触发点"正是 **ADDED** 的形状。
2. **若强行写 MODIFIED**，就得把 `skill-cast-runtime` 的既有需求**整条粘贴**（归档器整体替换
   requirement）——那会造出一份**与本轮无关的巨型 delta**，且任何粘贴遗漏都会**静默丢失**既有场景
   （`TOOLS-9` 同族教训）。**风险远大于收益。**

⇒ 终结侧触发点的**全部判据都写进 `skill-cooldown` 的同名需求**（见 delta），
`skill-cast-runtime` **本提案一字不改**。**这不是漏做**——是本仓"同一主题只允许一个载体"的落点。

## 明确不做（非目标）

- **不做 Cost**（`add-skill-cost` 单独一份）。
- **不做 UI 径向表现**（`SPEC-04-skill` §7 非目标）——本轮只保证"真值在 Entry、可查"。
- **不做网络复制姿态**（§6：接口位本期不实现）。
- **不做 `Duration` 的 `Param` 形态**（台账 `COOLDOWN-1`，与 `COST-1` / `STAT-11` 同批裁）。

## Impact

- **Affected specs**：`skill-cooldown`（**ADDED** ×2 需求：冷却多轨运行态 + 终结侧触发点与时机分派；再加事件需求 ×1）——
  **`skill-cast-runtime` 一字不改**（理由见"What Changes"第 3 节）
- **Affected code**（插件仓 TCS）：
  - 新：`Public/Skill/TcsCooldownOps.h` + `Private/Skill/TcsCooldownOps.cpp`、
    `Public/Skill/TcsCooldownEvents.h` + `Private/Skill/TcsCooldownEvents.cpp`
  - 改：`Public/TcsSkillSubsystem.h`（组槽 + 七公开口 + 1 friend）、`Private/TcsSkillSubsystem.cpp`、
    `Private/Skill/TcsCastOps.cpp`（门禁③ 真实现 + Timing 分派 + `OnCastStarted` 起冷）、
    `Public/Skill/TcsCastOps.h`（+`DispatchCooldownTiming`）、
    `Private/Skill/TcsSkillOps.cpp`（撤销/级联/注销三处配对清理）、
    `Public/Def/TcsSkillActivateResult.h`（`SAR_OnCooldown` 占位注记改为"已转真"）
  - **零新模块依赖边**（`TcsSkill.Build.cs` 不改）
- **取证**：无头 `Tcs.Test.CooldownCost.Run` **23/0**（两轮复现）；R6 回归 **24/0**
- **台账**：不消费也不新增（`COOLDOWN-1` 仍待触发）
