# 状态运行态定位裁决：`TcsState` = 共享层（定义族 + 运行机制）、Buff 专属语义拆出为 `TcsBuff` 新建模块

> **术语订正（2026-10-14）**：本文原用「单位 / `Unit`」，该词已退役，现统一为「**实体 / `Entity`**」（见 `DEC-2026-10-14-entity-vocabulary`）。**正文为当时原话，保留不改。**

> **★ 标题已于同日随 §6 修正**（原标题为"…运行态基底归 `TcsCore`、模块正名 `TcsBuff`"——两者均被实测推翻，见 §6）。

- **文档 ID**：`DEC-2026-10-13-state-module-positioning`
- **类型**：DEC / 决策记录（架构定位拍板）
- **状态**：FROZEN（决策已定稿并拍板 2026-10-13；**实现状态 = 零落地**——本裁决只定定位与形状，重构属新建计划）
- **权威范围**：`TcsState` 的**定位**（共享层 vs Buff 运行态）、**运行态基底**的形状与归属、**共享机制**的归并与落点、**Buff 专属内容的拆分**、以及本重构与 `PLN-R5.5`（`e1`/`e4`）的先后。**不在本文**——槽位竞争表自身的 Priority 语义（`PLN-R5.5` Task 4）、关系表族语义（`DEC-2026-10-13-relation-semantics`）、重构的 Task/Step 明细（属 `PLN-REFACTOR-1`）
- **最后更新**：2026-10-13

> **性质**：本裁决是用户 2026-10-13 提出的一处**架构质疑**的落地载体。质疑原文：「原始设计阶段，我就说过 `TcsState` 下属 Buff/Skill，但是现在看来，构建 `TcsState` 的过程中，好像把很多归属于 Buff 的内容放到了 State 里，比如 Duration、比如 Stack、比如 Period」。
>
> **证据标注**：【源码取证】= 本机全仓实测；【文档取证】= 仓内既有文本逐字引用；【外部取证】= 指名文件的实测统计（本会话派发的只读勘察，含逐项文件/行数/符号计数）；【设计判断】= 架构推理；【用户拍板】= 2026-10-13 本会话逐项裁定。
>
> **本轮取证边界（如实，MUST 读）**：本裁决的**全部判据均为仓内可复核**（源码符号、规格文本、`EVID` 读数、归档提案）。**未执行任何编译** ⇒ 凡"改动面"数字均为**词法计数**（文件数 / 行数 / 符号出现次数），**不是编译代价实测**。勘察中 `FTcsStateInstance::Source` / `::ExpiryEntry` 两行计数**含同名异构成员**（`FTcsAttrModDef::Source` / `FTcsCooldownTrackState::ExpiryEntry`），已标为近似。

---

## 0. 裁定总表（2026-10-13，用户逐项拍板）

| # | 议题 | 裁定 | 判据一句话 | 影响面 |
|---|---|---|---|---|
| **①** | `TcsState` 的定位 | **通用机制层**（不是 Buff 运行态） | 名字承诺通用、运行态实际装 Buff 语义 ⇒ Skill 被迫另起一套 | 架构级 |
| **②** | 运行态形状 | **基底 + 两个 payload** | 保留"每次施法可并存"与"组内一条 + 叠层"两套**基数不变量** | 两个运行态 struct |
| **③** | Buff 专属语义 | **移出基底** → Buff payload | 基底 MUST NOT 含具体域语义（5/14 字段是 Buff 专属） | `FTcsStateInstance` |
| **④** | 共享机制 | **合并为一份**，落 **`TcsState`**（~~`TcsCore`~~ —— **★ 同日修正，见 §6**）。**★ 第三笔修正（句柄，见 §8）**：**句柄 MUST NOT 合并为一个类型** | 8 条机制轴中 **7 条重复**；句柄 `IsValid` 判据还不一致。**落 `TcsState` 而非 Core** 的理由同 ⑨。**句柄例外**：合并会移除"跨桶误传编译期失败"这道现存护栏（`instance-handle-pool` 立了两条该判据），而宏生成反射结构体在本机**未核实** ⇒ 改取「**3 个反射声明 + 1 处逻辑单点**」 | 两模块 + `TcsState` |
| **⑤** | 时间语义 | 基底持**一个到期锚**；payload 各自扩展 | 两侧**已共用**同一 `UTcsClockSubsystem` + `FTcsTimeEntryHandle` | 两侧 |
| **⑥** | 重申请决策 | **并入基底**（通例）；Buff 只保留"层数" | 技能 `Instancing` 四路分流 = 状态五轴堆叠决策树（同型重复） | `TcsCastOps` 门禁④ |
| **⑦** | 模块归属 | ~~`TcsState` → `TcsBuff` 正名~~ → **★ 同日修正（见 ⑪ + §6）：`TcsState` 保留、`TcsBuff` 为新建模块** | ~~payload 化后它实际只剩 Buff 域~~ —— **该判据漏看了定义层**：`TcsState` 还持两域共用的 Def 族（473 行），而 `TcsSkill` **继承**它 | 见 ⑪ |
| **⑧** | 与 `PLN-R5.5` 的先后 | **本重构先行**；`e1`/`e4` 按新基底重写 | Task 1 Step 7 明文"主体 = `FTcsCastRun`"⇒ 先行会返工 | `PLN-R5.5` / `R6.5-g` |
| **⑨** | 基底运行态 struct 的落点 | **留 `TcsState`**（~~`TcsCore`~~ —— **★ 2026-10-13 同日修正，见 §6**） | `FTcsUnitRuntime` 装的是"**战斗单位身上的运行态**"（`Unit`/`Level`/`ParamSnapshot`/`ExpiryEntry`/`Phase`）⇒ 属**战斗域**概念。放进 Core 与"把 Buff 语义放进 State"是**同型错误**（只是换了方向）；Core 应当只放**真正无领域**的机制（它现有内容 = 池模板 / 句柄模板 / 事件总线 / 到期堆） | `TcsState` |
| **⑩** | Def 基类的落点 | **留 `TcsState`**（~~原模块 `TcsBuff`~~ —— **★ 同日修正，见 §6**），**MUST NOT** 进 Core | 它持 `TArray<FTcsNumericParamRow>`（依赖 `TcsNotation` 的 `ETcsValueConventionFlag`）+ `TArray<TSoftObjectPtr<UTcsAttrModDef>>`（依赖 `TcsAttribute`）⇒ 进 Core 造 `Core → {Notation, Attribute}` 环 | `TcsState` |
| **⑪** | **拆出方向：`TcsBuff` 是新模块，不是改名** | `TcsState` **保留**为共享层（两域共用的定义族 + 通用运行时机制）；**Buff 专属语义拆出为独立新模块 `TcsBuff`** | **★ 同日追补（用户提问驱动）**：实测 `TcsState` 55 文件 / 6,663 行是**三层**——共用 Def 族 473 行（**`TcsSkill` 继承它**）+ 通用运行时机制 956 行 + Buff 专属约 5,200 行。**原裁定⑦（"改名 `TcsBuff`"）不成立**：改名会把两域共用的 Def 族一并吞进"Buff"，而 `FTcsSkillDefData : FTcsBuffDefBase` 是**语义错误** | 新模块 + 两处改指 |
| **⑫** | `TcsEffect` 依赖边 | **拆出后可去掉** `TcsEffect → TcsState` | **★ 同日实测副产品**：`TcsState` 中 `TcsEffect` 的使用 **100% 落在 Buff 专属层**（`TcsStepApplyState` / `TcsBuffDef::Triggers` / `TcsBuffDefAsset` / `TcsStateEvents` / `TcsStateOps_Trigger`）⇒ 拆出后共享层只剩 `Core` / `Notation` / `Attribute` 三个依赖 | 依赖链 |

---

## 1. 勘察（现状是什么）

### 1.1 两个运行态逐字段对照（【源码取证】）

**`FTcsStateInstance` = 14 字段**（`Source/TcsState/Public/State/TcsStateInstance.h:FTcsStateInstance`）

| 分类 | 字段 |
|---|---|
| **通用 3** | `Unit` / `Level` / `ParamSnapshot` |
| **同类型异角色 2** | `Source`、`ExpiryEntry` |
| **Buff 专属 5** | `Stacks` / `DurationRemaining` / `PeriodRemaining` / `PeriodEntry` / `Phase`（`EStatePhase` 生命周期态） |
| **其余 4** | `DefTag` / `Handle` / `CascadeAnchor` / `Instigator` |

**`FTcsCastRun` = 11 字段**（`Source/TcsSkill/Public/Skill/TcsCastRun.h:FTcsCastRun`）

| 分类 | 字段 |
|---|---|
| **通用 3** | `Unit` / `Level` / `ParamSnapshot` |
| **同类型异角色 2** | `RunSource`、`PhaseExpiryEntry` |
| **Cast 专属 6** | `EntryHandle` / `PhaseIndex` / `ChainRunHandle` / `ActiveChainId` / `MainChainStart` / `MainChainStartPhaseTag` |

⇒ **`Unit` / `Level` / `ParamSnapshot` 三个语义完全相同**（两侧注释都自述"必须自持，不是冗余"）；`Source`/`RunSource` 与 `ExpiryEntry`/`PhaseExpiryEntry` 是**同类型、异角色**的两对。

### 1.2 机制重复面（8 轴，**7 重复**）【外部取证】

| 轴 | `TcsState` | `TcsSkill` | 份数 |
|---|---|---|---|
| 句柄 + `IsValid` | `FTcsStateHandle`（`Index>=0 && Generation>0`） | `FTcsCastRunHandle`（`Index != -1`，**判据不同**）+ `FTcsSkillEntryHandle` | **3** |
| per-unit 桶 + 代际 + 空闲栈 | `FStateBucket` | `FSkillBucket`（**3 成员 + 7 方法逐一对应**） | 2 |
| 代际发号器 | `GTcsStateRegistryNextGeneration`（+2 奇数） | `GTcsSkillRegistryNextGeneration`（+2 奇数） | 2 |
| 槽位复用 | 手写（刻意不用 `TTcsInstancePool`） | `TTcsInstancePool<FTcsCastRun,…>` + 账本侧手写 | **3 变体** |
| 世界门面 + `Deinitialize` | `UTcsStateSubsystem` | `UTcsSkillSubsystem` | 2 |
| `AddReferencedObjects` | `TcsStateSubsystem.cpp:AddReferencedObjects` | `TcsSkillSubsystem.cpp:AddReferencedObjects`（**近乎相同的快照遍历循环**） | 2 |
| 重入守卫 | `InRemovalBroadcastHandles` | `InFinishBroadcastHandles`（注释自述"体例照状态侧已验证的解法"） | 2 |
| 快照读取壳 + 绑定作用域 | `FTcsStateSnapshotScope` / `UTcsStateParamTableReader` | **复用同一份**（`TcsCastOps_Phase.cpp` / `TcsCastOps.cpp`） | **1（已共用）** |

### 1.3 ★ 一处本轮新发现的机制重复（先前无人记录）

技能门禁第 ④ 道 `Instancing` 的四路分流，**与状态五轴堆叠决策是同一个决策空间**：

| 技能档 | 状态堆叠等价档 |
|---|---|
| `InstancePerExecution`（并存） | `NewInstance`（恒建新条） |
| `InstancePerEntity` + `bRetriggerOnActive = false` | `Reject` |
| `InstancePerEntity` + `bRetriggerOnActive = true` | `Replace` |

而状态侧**已有**可覆写的三方法虚分派 API（`FTcsStateStackDecisionFragment::IsSameGroup` / `ShouldAccept` / `ResolveStacks`，`TcsStateStackFragment.h`），且 `TcsStateOps_Stack.cpp:TcsStackOps_IsSameGroup` 在 `EGB_Custom` 档**真的调用它**、宿主样本 `FTcsDevStackDecisionAlwaysNew`（`Source/TcsDev/…/TcsDevStackDecisionSample.h:FTcsDevStackDecisionAlwaysNew`）已实证"恒不同组 ⇒ 每次建新条"并有 PASS 读数（检查 `22m`：在册 = 2）。

⇒ **技能在 `TcsCastOps.cpp` 里自己又实现了一遍这个决策树**，未复用。**这一条把裁定①从"整理命名"提升为"消除一处真机制重复"。**

### 1.4 模块规模与爆炸半径【外部取证】

| 项 | 量 |
|---|---|
| `Source/TcsState` | 30 `.h` + 25 `.cpp`，**5,338 非空行** |
| `Source/TcsSkill` | 32 `.h` + 29 `.cpp`，**9,542 非空行** |
| 宿主装置 `Source/TcsDev` | 61 文件，**13,325 行**（在改动面内） |
| 触达 `FTcsCastRun` | **21 文件**（插件内 69 个成员读写点）+ 宿主 67 处 |
| 触达 `FTcsStateInstance` | **24 文件**（约 148 点，近似）+ 宿主 49 处 |

### 1.5 对排期的影响【源码/文档取证】

| 计划 | 绑定到 | 影响 |
|---|---|---|
| `PLN-R5.5` Task 1 Step 7 | "共享同一检查器（**主体 = `FTcsCastRun`**）" | 主体类型变 ⇒ 文本与新建文件均需重写 |
| `PLN-R5.5` Task 1 Step 2 | 关系门"判定**排在**五轴共存决策**之前**" | 绑定到要重构的决策 API |
| `PLN-R5.5` Task 4 | "槽位竞争表（**主体 = 施法运行**）" | 前提变 |
| `R6.5-g` | 三支柱之一 = "**阶段标识 StateInstance**" | 第一根被重定义 |
| `R6.5-e` | 布尔开关（条目/参数侧） | **不受影响** |

---

## 2. 现状证据表

| # | 判据 | 可复核锚点 |
|---|---|---|
| E1 | 两运行态字段集 | `TcsStateInstance.h:FTcsStateInstance`；`TcsCastRun.h:FTcsCastRun` |
| E2 | 三个语义相同字段 | 同上：`::Unit` / `::Level` / `::ParamSnapshot` |
| E3 | 桶结构逐项对应 | `TcsStateRegistry.h:FStateBucket` / `TcsSkillRegistry.h:FSkillBucket`（`Instances\|Entries` + `SlotGenerations` + `FreeSlots` + `MakeHandle`） |
| E4 | 代际发号器两份 | `TcsStateRegistry.cpp:GTcsStateRegistryNextGeneration` / `TcsSkillRegistry_Bucket.cpp:GTcsSkillRegistryNextGeneration`（均 +2 奇数） |
| E5 | 句柄三份、判据不一致 | `TcsStateHandle.h:FTcsStateHandle`（`Generation>0`）/ `TcsCastRunHandle.h:FTcsCastRunHandle`（`Index != -1`）/ `TcsSkillEntryHandle.h:FTcsSkillEntryHandle` |
| E6 | 快照读取壳唯一已共用 | `TcsStateParamTableReader.h:FTcsStateSnapshotScope` / `:UTcsStateParamTableReader`；消费点 `TcsCastOps_Phase.cpp`、`TcsCastOps.cpp` |
| E7 | 重复的 ARO 循环 | `TcsStateSubsystem.cpp:AddReferencedObjects` / `TcsSkillSubsystem.cpp:AddReferencedObjects`（后者经 `ForEachCastRunForGC`） |
| E8 | 重入守卫两份（技能抄状态） | `TcsStateSubsystem.h:InRemovalBroadcastHandles` / `TcsSkillSubsystem.h:InFinishBroadcastHandles` |
| E9 | **基底字段类型全在 Core** | `Handle/TcsCombatEntityHandle.h`、`Handle/TcsSourceHandle.h`、`Clock/TcsExpiryHeap.h:FTcsTimeEntryHandle` |
| E10 | Core 已承载池机制 | `TTcsInstancePool` 已在 `TcsCore`；`DEC-00-constitution` §9 明文"总线/**池**内置 `TcsCore`" |
| E11 | 堆叠决策是**可覆写虚分派** | `TcsStateStackFragment.h:FTcsStateStackDecisionFragment::IsSameGroup` / `::ShouldAccept` / `::ResolveStacks`；`TcsStateOps_Stack.cpp:TcsStackOps_IsSameGroup` 在 `EGB_Custom` 档调用它 |
| E12 | 技能侧自实现同型决策 | `TcsSkillDefData.h:FTcsSkillDefData::Instancing`（`ECastInstancing`）+ `TcsCastOps.cpp` 两处 `SAR_AlreadyActive` 分支 |
| E13 | 技能侧**零** `ApplyState` 调用 | 全 `Source/TcsSkill` 零命中；取 `UTcsStateSubsystem` 的唯一用途是参数读取壳 |
| E14 | 门面硬编码 Buff 类型 | `TcsStateSubsystem.h`：`TMap<FGameplayTag, TUniquePtr<FTcsBuffDef>>` |
| E15 | 宪法原文 | `dec-00-constitution.md` §9 模块表：`TcsState`「**Buff 子域，不独立**——TCS 双账本发散教训」；`TcsSkill` 列为**平级独立模块** |
| E16 | 宪法**非** `FROZEN` | `dec-00-constitution.md` 身份块：**状态 `ACTIVE`** ⇒ 模块表可改 |
| E17 | "不做 Instancing"**从未进锁定规格** | 仅 `spec/03-module-states.md`（§8 非目标 + 变更记录）；`openspec/specs/**` **零命中** |
| E18 | 已付费验收证据 ≥11 篇 | 状态侧：`EVID-2026-10-04-state-param-snapshot-and-level-sources`(39/0)、`EVID-2026-10-05-state-stacking-policies`(53/0+60/0，Reject 11/0)、`EVID-2026-10-05-state-behavior-fragments`(65/0+72/0)、`EVID-2026-10-05-state-layer-pie`(15/0，Reject 7/0)；技能侧：`EVID-2026-10-08-skill-cast-runtime`(26/0+10/0，含 ARO 阴性对照)、`EVID-2026-10-08-skill-registry`(27/0→45/0)、`EVID-2026-10-10-skill-cast-phases`(26/0)、`EVID-2026-10-11-skill-layer-e2e`(24/0)、`EVID-2026-10-11-r65-cooldown-cost`(18/0+21/0) |
| E19 | 需 MODIFIED 的能力约 11 个 | 最重 = `skill-cast-runtime`（明文要求 `FTcsCastRun` 非反射 + 必须经 `TTcsInstancePool` + "MUST NOT 新建池实现" + 逐条列 8 个必具字段）；另有 `skill-cast-phases` / `state-instance-lifecycle`(7 req/40 scen) / `state-stacking-policies` / `state-param-snapshot` / `state-behavior-fragments` / `state-modifier-materialization` / `skill-registry` / `skill-cooldown` / `skill-param-chain` / `skill-layer-e2e-validation` / `effect-trigger` / `effect-interpreter` / `gameplay-tag-governance` |
| E20 | 曾被拒的乙案已入册 | `deferred-inputs-ledger.md`：`STAT-7` 记"层 = 独立实例"属**层模型级变更**，**不是加一个枚举值就能解决** |
| E21 | **Def 基类依赖 Notation（挡住"进 Core"）** | `Def/TcsParamRow.h` 的 `FTcsNumericParamRow::ValueConvention` 类型 = `ETcsValueConventionFlag`，住 `TcsNotation/Public/TcsValueConvention.h`；`TcsNotation.Build.cs` 只依赖 `Core`+`CoreUObject` ⇒ **Core ↔ Notation 平级互不依赖**，Core 取该类型即成环 |
| E22 | 时间子系统两侧已共用 | 两侧均经 `UTcsClockSubsystem::PushExpiry` / `CancelExpiry`；`FTcsTimeEntryHandle` 单点声明于 `Clock/TcsExpiryHeap.h` |
| E23 | 已归档提案面约 16 个 | 技能侧 9（`2026-10-09-add-cast-run-and-gates` 38 命中 / `2026-10-11-add-cast-phases-and-interrupt` 28 / `2026-10-09-add-skill-param-chain` 11 / `2026-10-12-add-cooldown-multitrack` 7 / `2026-10-08-add-skill-registry` 7 / 另 4 个 2–3）+ 状态侧 7（`2026-10-05-add-state-chain-primitives` 11 / `2026-10-04-add-state-instance-lifecycle` 8 / `2026-10-04-add-state-param-snapshot-and-level-sources` 8 / 另 4 个 3–4） |

---

## 3. 口径裁决表（执行视角）

| # | 议题 | 裁定 | 判据 | 落点 |
|---|---|---|---|---|
| A1 | 基底内容 | **身份 + 归属 + 快照 + 一个到期锚 + 生命周期态** | 两侧取**交集**（`Unit`/`Level`/`ParamSnapshot` + `Source` 角色 + `ExpiryEntry` 角色 + `Phase`） | `TcsCore` |
| A2 | Buff payload | `Stacks` + `DurationRemaining` + `PeriodRemaining` + `PeriodEntry` | 这 4 个是 Buff 独占语义 | 原模块（`TcsBuff`） |
| A3 | Cast payload | `EntryHandle` + `PhaseIndex` + `ChainRunHandle` + `ActiveChainId` + `MainChainStart` + `MainChainStartPhaseTag` | Cast 独占语义 | `TcsSkill` |
| A4 | 重申请决策 | **基底不实现决策**，只**调用**一个可覆写的决策接口；Buff/Cast 各挂自己的默认策略 | 见 §1.3：技能 Instancing 与状态堆叠同型 ⇒ 决策归策略、机制归基底 | `TcsCore`（接口）+ 两个默认策略 |
| A5 | 层数语义 | `Stacks` 与 `ValueStack` 五轴**留在 Buff payload** | 技能无"层数"概念（只有并存/驳回/顶替） | `TcsBuff` |
| A6 | 句柄 | 收敛为**一份**（基底句柄），统一 `IsValid` 判据 | 现三份判据不一致；`FTcsCastRunHandle` 的 `GetInner`/`SetInner` 转换对随之一并整理 | `TcsCore` |
| A7 | 命名 | 模块 `TcsState` → **`TcsBuff`**；基底类型取中性名（**不**含 `State`/`Buff`/`Cast`） | 基底被两域共用 ⇒ 名字 MUST NOT 偏向任一域 | 全库 |
| A8 | Def 基类 | **留原模块**，仅随模块正名调整前缀 | E21：它依赖 Notation | `TcsBuff` |
| A9 | `ApplyState` 步骤名 | 随正名一并评估（`ApplyState` → 待定） | 它是 Buff 域步骤；但改名牵动链步骤注册名与内容资产 | 登记为 Task 4 的评估项 |

---

## 4. 错前提更正

| # | 错前提 | 实况 | 处置 |
|---|---|---|---|
| ① | **（agent 提出，已撤回）**"状态'组内至多一条实例'与技能'每次并存'**互斥**，故两个运行态不能合并" | **不成立**。`FTcsStateStackDecisionFragment::IsSameGroup` 是**可覆写虚函数**，`TcsStateOps_Stack.cpp:TcsStackOps_IsSameGroup` 在 `EGB_Custom` 档**真的调用它**；宿主样本 `FTcsDevStackDecisionAlwaysNew`（恒不同组）**已实证**"每次施加独立成实例"，且有 PASS 读数（检查 `22m`：在册 = 2）。⇒「组内至多一条」是**默认策略的结果**，**不是结构不变量** | **撤回该论据**；裁定② 因此不受此阻碍。**根因** = 把默认档读成了不变量（同族教训另见 `MEM-20261013-09`） |
| ② | **（用户提出，部分更正）**`TcsState` 把 Duration / Stack / Period 放进了 **Def 基类** | **不成立**。`Def/TcsStateDefBase.h:FTcsStateDefBase` 只有 6 字段（`StatusTag`/`LevelBase`/`MaxLevel`/`Params`/`Descriptions`/`ModifierRows`），并有一条明文判据在守：**"无时值 / 无堆叠 = 基类；有时值 / 有堆叠 = 派生"**（`TcsStateDefBase.h` 类注释）⇒ Duration/Period/Stack **住 `FTcsBuffDef`（派生类）**，Def 层**没有**漂移 | **漂移在运行态与模块命名，不在 Def 继承**——但用户指出的实质问题（`State` 名实不符 ⇒ Skill 被迫另起）**完全成立**，且正是裁定① 的判据 |
| ③ | **（用户提出，半对）**原始设计 = `TcsState` 下属 `Buff` **和** `Skill` | **半对**。宪法原文（`dec-00-constitution.md` §9 模块表）为：`TcsState`「**Buff 子域，不独立**——TCS 双账本发散教训」；`TcsSkill` 列为**平级独立模块** ⇒ 原始记录是「**Buff 是 State 的子域，Skill 是兄弟**」。**但**用户指出的实质问题成立 | 按裁定① 处理；宪法模块表随之订正（E16：该文档 `ACTIVE`，可改，不受 `CONVENTION` §7 冻结纪律约束） |
| ④ | **（agent 初判，已更正）**"基底运行态与共享机制都进 `TcsCore`"是纯搬运 | **不成立（对 Def 那半）**。运行态基底**可以**进 Core（其字段类型全在 Core，E9）；但 **Def 基类不行**——它持 `TArray<FTcsNumericParamRow>`，该类型的约定列依赖 `TcsNotation` 的 `ETcsValueConventionFlag`，而 Core 与 Notation **平级互不依赖**（E21）⇒ 强行下移会造出 `Core → Notation` 的环 | 裁定⑨/⑩ 分开处理：**运行态基底进 Core，Def 基类留原模块** |
| ⑤ | **（agent 口报，已更正）**模块行数 6,663 / 11,249 | **不成立**（我先前用 `Get-Content` 计行，把空行算进了总数）。非空行实为 **5,338 / 9,542** | 已按勘察复核值订正；本节留痕以免后人据旧数估算 |

---

## 5. Task 切分（本重构的轮廓，明细属新建计划）

> 本节只给**Task 级轮廓与依赖**，供新建计划承接；**不含 Step**（那是计划的职责，同 `PLN-R5.5` 与《R5.5 批次表》的分工先例）。

| Task | 内容 | 依赖 | 备注 |
|---|---|---|---|
| **Task 0** | **定位与形状拍板**（本裁决） | — | ✅ 本轮的产出 |
| **Task 1** | 基底抽形 + **共享机制合并**（§1.2 的 7 轴：句柄 / 桶 / 代际 / 槽位复用 / 门面 / ARO / 重入守卫） | Task 0 | 落 `TcsCore`（裁定⑨）；**E19 的 `skill-cast-runtime` 明文"MUST NOT 新建池实现"须同批改述** |
| **Task 2** | **Buff payload 化**：`Stacks`/`DurationRemaining`/`PeriodRemaining`/`PeriodEntry` 迁出基底 | Task 1 | 影响 `TcsStateOps_*` 全部文件（13 文件触达实例） |
| **Task 3** | **Cast payload 化**：`FTcsCastRun` → 基底 + cast payload；门禁④ 改走**共享重申请决策**（消除 §1.3 的重复） | Task 1 | **与 Task 2 可并行**（共享基底已定） |
| **Task 4** | **模块正名** `TcsState` → `TcsBuff`（23 个 `TCSSTATE_API` 文件 + 16 个 `LogTcsState` 文件 + 6 个外部 include 点）+ `DEC-00` 模块表订正 + `ApplyState` 步骤名评估（A9） | Task 2 | 正名与 payload 化**分开做**，避免两种改动交织后难定位回归 |
| **Task 5** | **规格与能力面回写**：约 11 个能力 MODIFIED delta（E19）+ 根表/命名面 | Task 3/4 | Core 侧需同步 `instance-handle-pool` 能力 |
| **Task 6** | **证据重取**：≥11 篇 `EVID` 作废后按新形状重取（双配置编译 + PIE） | Task 5 | **甲案的主要代价所在** |
| **Task 7** | 收束（台账 / 日志 / `INDEX` / 三方向扫描） | Task 6 | 同既有收束惯例 |

**关键路径** = Task 0 → 1 → (2 ∥ 3) → 4 → 5 → 6 → 7。

### 5.1 对既有计划的连带处置（裁定⑧）

| 对象 | 处置 |
|---|---|
| `PLN-R5.5` Task 1 Step 7（"主体 = `FTcsCastRun`"） | **按新基底重写**（主体 → 基底句柄） |
| `PLN-R5.5` Task 1 Step 2（关系门排在五轴决策之前） | **按新决策接口重写** |
| `PLN-R5.5` Task 4（槽位竞争表） | 主体与"槽"的定义**按新基底重写** |
| `R6.5-g`（多形态走查） | 三支柱之"阶段标识 StateInstance"**随正名与新基底更新** |
| `R6.5-e` | **不受影响** |
| `PLN-R5`《R5.5 批次表》`e` 行 | 补日期追注，说明 `e1`/`e4` 被本重构前置 |

### 5.2 风险（登记，不含缓解——缓解属计划的职责）

| 风险 | 判断 | 说明 |
|---|---|---|
| ~~**净收益可能为负**~~ → **★ 2026-10-13 用户口径后该判已作废** | — | 用户明示：**TCS 仍处设计开发阶段 ⇒ 文档变更 / 版本迭代 / 回滚重做都是正常的**。⇒ **一次性返工代价不是决策依据**；本节原列的代价只用于**排期与心理预期**。**该行原"高风险"判是成本视角，不再成立。** |
| **在未想清的地基上继续施工** | **高** | 真正的风险是**设计未想清而继续施工**（如基底粒度反复）⇒ 应停下想清。**这是正确性判据，不是成本判据** |
| 基底抽形抽错粒度 | 中 | 抽太细 ⇒ 两 payload 仍需各自机制；抽太粗 ⇒ 基底含域语义（正是本次要修的病） |
| `TTcsInstancePool` 与手写桶的取舍重开 | 中 | 现三种变体（E-§1.2），合并时必须一次性定一种 |
| 拆模块牵动内容资产与装置 | 中 | `TcsStateParam` tag 根 MUST NOT 改（改则已落资产找不到键）；`ApplyState` 步骤注册名同理 |
| 已付费证据需重取 | 中 | ≥11 篇，含 ARO 阴性对照 —— **是工作量，不是"不该改"的理由**（用户口径） |

---

## 6. ★ 同日修正：`TcsState` 是三层，拆出方向应为「新建 `TcsBuff`」而非「改名」

> **触发**：用户在评审 `PLN-REFACTOR-1` 时提问——「将通用基底移入 `TcsCore` 之后，`TcsState` 自身到底还剩下多少，直接全部并入 `TcsBuff` 模块是否真的合适」。
> **结论**：**用户的怀疑成立，原裁定⑦（改名）不成立**。
>
> **★ 关于本节与正文的关系（如实声明，MUST 读）**：本节按**「先改后冻」**先例（见 `MEM-20261013-04`：`CONVENTION` §7 的"交付后不再修改正文"只约束**交付之后**）——本文件与本节**同日建立、尚未交付、两仓零提交**，故本轮**直接就地订正了正文的裁定表**（④/⑦/⑨/⑩ 四行划改 + 新增 ⑪/⑫）与标题、权威范围，而**不是**只在文末追加注记。**若本文件已提交或已被下游引用，则本节 MUST 改为纯追加注记、正文一字不动。** 该判据与 `PLN-R4` 的处置同源。

### 6.1 实测：`TcsState` = 55 文件 / 6,663 行，是**三层**不是一层

对每个 Public 头逐文件统计 include 方（【源码取证】，本机全仓）：

| 层 | 内容 | 行数 | 引用方 |
|---|---|---|---|
| **共用 Def 族** | `Def/TcsStateDefBase.h` / `Def/TcsStateDef.h`+`.cpp` / `Def/TcsParamRow.h` / `Def/TcsDescriptionEntry.h` / `Param/TcsStateParamKeys.h`+`.cpp` | **473** | **`TcsState` + `TcsSkill`** |
| **通用运行时机制** | `State/TcsStateRegistry.h`+`.cpp` / `State/TcsStateHandle.h` / `State/TcsStateSnapshot.h` / `State/TcsStateParamTableReader.h` | **956** | **`TcsState` + `TcsSkill`** |
| **Buff 专属** | BuffDef 三件 / 堆叠（`StackPolicy` + `StackFragment` + `Ops_Stack`）/ 时值周期 / 行为 Fragment 五件 / 修正器物化 / `TcsStepApplyState` / 等级源两件 / 等级读口 / 六枚生命周期事件 | **约 5,200** | 仅 `TcsState`（+ LAC） |
| 模块壳 + 日志 + 跨组文件（`Ops.h` / `Subsystem.h`） | — | **约 967** | — |

**决定性事实**（反向 grep，`TcsSkill` 对 `TcsState` 类型的引用计数）：`FTcsParamSnapshot` 5 / `FTcsStateSnapshotScope` 5 / `FTcsNumericParamRow` 4 / `UTcsStateParamTableReader` 4 / `FTcsStateDefBase` 3 / `ETcsParamMode` 2 / `FTcsDescriptionEntry` 1 / `UTcsStateDef` 1。

且**继承关系是硬的**：`FTcsSkillDefData : public FTcsStateDefBase`、`UTcsSkillDef : public UTcsStateDef`。

### 6.2 为什么"改名 `TcsBuff`"不成立

| 去处 | 为什么不成立 |
|---|---|
| 共用 Def 族进 `TcsCore` | **造依赖环**：`FTcsNumericParamRow` 持 `ETcsValueConventionFlag`（住 `TcsNotation`）、`FTcsStateDefBase` 持 `TArray<TSoftObjectPtr<UTcsAttrModDef>>`（住 `TcsAttribute`）⇒ Core **不许**依赖这两者（`TcsCore.Build.cs` 仅引擎五件） |
| 随模块改名 `TcsBuff` | **`FTcsSkillDefData : FTcsBuffDefBase`** —— 技能继承"Buff 基类"是**语义错误** |
| 让技能自定义一份 | 要拆掉 R6 交付的继承契约 ⇒ **等于重开一次设计** |

⇒ **`TcsState` 不只承担"Buff 运行态"，它还承担"两域共用的定义层"**。原裁定⑦ 的判据（"payload 化后只剩 Buff 域"）**只看了运行层、漏了定义层**——与本轮已记录的"按一层看就下结论"属同型错误。

### 6.3 修正后的三层形态

```
TcsCore        引擎基础 + 无领域机制（池模板 / 句柄模板 / 事件总线 / 到期堆）
   ↑
TcsState       ★ 共享层：状态定义族(473) + 通用运行时机制(956) ≈ 1,430 行
   ↑                        ↑                    ← 拆出后不再依赖 TcsEffect（见 ⑫）
TcsBuff              TcsSkill
（新模块，≈5,200 行）   （已有）
```

**`TcsState` 的定位因此改写为**：不是"通用机制层"（太泛），而是「**两域共用的定义层与运行机制**」。Buff 专属语义被请出去之后，**它的名字反而变准了**。

### 6.4 撤销与修正清单（原裁定 → 现行）

| 原裁定 | 现行 | 依据 |
|---|---|---|
| ④ 共享机制落 `TcsCore` | **落 `TcsState`** | 同 ⑨：机制服务于"战斗单位运行态"，非无领域机制 |
| ⑦ `TcsState` → `TcsBuff` 正名 | **撤销**；`TcsState` 保留，**`TcsBuff` 为新建模块** | §6.1/§6.2 |
| ⑨ 基底运行态落 `TcsCore` | **落 `TcsState`** | `FTcsUnitRuntime` 装"战斗单位身上的运行态"（`Unit`/`Level`/`ParamSnapshot`/`ExpiryEntry`/`Phase`）⇒ **战斗域概念**。放进 Core 与"把 Buff 语义放进 State"是**同型错误**（换了方向）；Core 应只放真正无领域的机制 |
| ⑩ Def 基类留"原模块 `TcsBuff`" | **留 `TcsState`** | 落点名称随 ⑦ 修正 |
| ② 基底 + 两个 payload | **不变**（继续有效） | — |
| ⑪ 新增：`TcsBuff` 是新建模块 | — | §6.1 |
| ⑫ 新增：拆出后可去掉 `TcsEffect → TcsState` | — | 实测：`TcsState` 中 `TcsEffect` 的使用 **100% 在 Buff 专属层**（`TcsStepApplyState` / `TcsBuffDef::Triggers` / `TcsBuffDefAsset.cpp` / `TcsStateEvents.cpp` / `TcsStateOps_Trigger.cpp`） |

**仍未动摇的判据**：裁定① 的**病因**（Buff 专属语义压在通用命名之下、技能被迫另起一套）依然成立，且 §1.3 的**真机制重复**（技能 `Instancing` 四路分流 = 状态五轴堆叠决策树）依然要消除。**本次修正的是"怎么治"，不是"有没有病"。**

---

## 8. ★ 第三笔修正：三个运行态句柄 **MUST NOT** 合并为一个类型

> **触发**：为 `PLN-REFACTOR-1` Task 1 起草 openspec 提案时，按裁定④"共享机制合并"的**字面**把三个句柄（`FTcsStateHandle` / `FTcsCastRunHandle` / `FTcsSkillEntryHandle`）写成"**收敛为 `FTcsUnitRuntimeHandle` 一个类型**"。做**符号方向扫描**时发现它与一条**现存的编译期护栏**冲突。

### 8.1 冲突（实测）

`instance-handle-pool` 规格**立了两条"混用必须编译失败"**：

| 锚点 | 原文 |
|---|---|
| 该能力「强类型实例句柄」需求 | 「`TTagType` MUST 为**编译期幻影标签**（空结构，仅作类型区分、零运行时开销）——**不同池的句柄类型不可互换**」 |
| Scenario「句柄类型隔离」 | **THEN** **编译失败**（"跨池误用在编译期拦截，**无运行期成本**"） |

而现状**三个句柄各持自有幻影标签**（`FTcsStateTag` / `FTcsCastRunTag` / …），且 `FTcsCastRunHandle` **各自**提供 `GetInner()` / `SetInner()` **唯一转换对**（其头注释明文：留该对是为"**避免各处自行拼装出语义不一致的句柄**"）。

⇒ **合并为一个类型后**：① "把状态句柄传给施法运行 API"从**编译期错误**降为**运行期查不到**（正是本仓反复拦的"静默错配 + 误导性症状"）；② 一个类型的 `GetInner()` **只能返回一个** `TTcsInstanceHandle<Tag>`，无法服务三个标签 ⇒ 单点转换**必然退化**为调用点各自 `static_cast`。

**边界（如实）**：那两条 Scenario 管的是 `TTcsInstanceHandle<TagType>` **模板句柄**（它不受影响）；本条说的是**移除了一道今天存在的护栏** —— 而本仓在句柄上**专门立了两条**该判据，说明它被当回事。

### 8.2 为什么不用"一个宏生成三个类型"绕过

初判想把重复收敛为**一个宏**（`UE_DECLARE_UNIT_RUNTIME_HANDLE(...)` 生成三个仍带幻影标签的类型）。**实测否决**（【引擎源码】，`E:\UnrealEngine\UE_5.8`）：

| 检查 | 结果 |
|---|---|
| 引擎 `Runtime/` 内"宏展开出 `USTRUCT(`"的用例 | **零命中** |
| 引擎自带 wrapper-struct 宏（`DEFINE_INTERPCURVE_WRAPPER_STRUCT` / `DEFINE_INTERVAL_WRAPPER_STRUCT` / `DEFINE_RANGE_WRAPPER_STRUCT` / `DEFINE_RANGEBOUND_WRAPPER_STRUCT`，实测 4 个） | **全部生成普通 C++ struct**——体内**无 `USTRUCT`、无 `GENERATED_BODY`** |
| `USTRUCT(...)` 在引擎宏里的角色 | `ObjectMacros.h` 里被定义成**空**（`#define USTRUCT(...)`，"给编译器看的占位"），**不是** UHT 的展开入口 |
| **结构性障碍** | `ObjectMacros.h:GENERATED_BODY` = `BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY)` ⇒ **按 `__LINE__` 拼符号名**，而 UHT **按扫描到的行**生成对应符号 ⇒ 外层宏包裹后**行号语义是否仍对齐，本机无法确认** |

**依本仓既有纪律**（`FGameplayTagQuery` 与"槽位竞争"两次都因**本机不可核实**而去掉依赖）⇒ **不把 Task 1 建在未核实的 UHT 行为上**。

### 8.3 现行方案：**3 个反射声明 + 1 处逻辑单点**

| 面 | 处置 |
|---|---|
| **类型声明** | **保留三份**（`FTcsStateHandle` / `FTcsCastRunHandle` / `FTcsSkillEntryHandle` 各一个 `USTRUCT(BlueprintType)`）——它们是**三个不同的反射身份**，**本就必须各自写**；**幻影标签与编译期隔离不动** |
| **逻辑** | **收敛到一处非反射公共件**（如 `FTcsUnitRuntimeHandleOps` 静态函数族）：`IsValid` 判据、`operator==`、`GetTypeHash`、池位模式转换的**语义**单点定义；三个薄型各自**委托**它 |
| **判据统一** | **统一取最严**（`Index >= 0 && Generation > 0`）——实测 `FTcsCastRunHandle` 现为 `Index != -1`（**不看 `Generation`**），与另两者不同，属**真不一致**，该统一。**行为中性论证**：对良构句柄（`Allocate` 产物 `Generation >= 1`；默认构造产物 `Index = -1`）**两套判据结论逐例相同**，仅"手工构造 `Index >= 0` 且 `Generation == 0`"这一**从不出现的形态**由"有效"变"无效" |
| **受影响的既有注释** | `FTcsCastRunHandle::IsValid` 现注释自述"**因其代际由池在解析/释放时校验，本结构只判『是否曾被赋值』**"——统一后该注释 **MUST 改述**，并说明为何现在连 `Generation` 一起判 |

**净效果**：判据统一 + 转换/哈希**语义单点** + **编译期隔离保留** + **零未核实依赖**。**代价**：三个反射声明仍在（但那是必需的，不是重复——实测三型各约 15 行代码，其余皆为注释）。

---

## 变更记录

- 2026-10-13 建立：用户提出架构质疑（`TcsState` 是否混入了 Buff 专属内容）⇒ 经两轮勘察（其间**撤回 agent 自己的两处错论据**，见 §4 ①④⑤）后逐项拍板十项裁定；产出本载体 + Task 轮廓。**同批记录一处本轮新发现的机制重复**（技能 `Instancing` 四路分流 = 状态五轴堆叠决策树，§1.3），它是裁定①的实质依据之一。
- 2026-10-13 **同日第二笔修正（§6，用户评审计划时提问驱动）**：实测 `TcsState` 为**三层**（共用 Def 族 473 + 通用运行时机制 956 + Buff 专属约 5,200）⇒ **撤销裁定⑦（改名 `TcsBuff`）**、**修正裁定④/⑨/⑩ 的落点**（一律从 `TcsCore` 改回 `TcsState`）、**新增裁定⑪**（`TcsBuff` 为**新建模块**）与**⑫**（拆出后可去掉 `TcsEffect → TcsState` 边）。**撤销理由**：原判据"payload 化后只剩 Buff 域"**只看了运行层、漏了定义层**，而 `FTcsSkillDefData : FTcsStateDefBase` 的继承是硬的。**裁定①②③⑤⑥⑧ 不受影响。**
- 2026-10-13 **同日第三笔修正（§8，为 Task 1 起草提案时符号扫描驱动）**：把三个句柄**合并为一个类型**会**移除一道现存的编译期护栏**（`instance-handle-pool` 立了两条"混用必须编译失败"）⇒ **改取「3 个反射声明 + 1 处逻辑单点」**；同时**否决**"一个宏生成三型"的绕过方案（引擎内**宏生成 `USTRUCT` 零先例**，且 `GENERATED_BODY` 的 `__LINE__` 耦合**本机无法确认**）⇒ 依既有纪律不建在未核实行为上。**裁定④ 的"句柄 3 份 → 1"据此改述为"3 个反射声明 + 1 处逻辑单点"。**
