# Change: 运行态机制合并（统一句柄 / 桶 / 注册表 / GC 口 / 重入守卫）

## Why

`TcsState` 与 `TcsSkill` 各有一套**功能相同、代码平行**的运行态机制，实测 **8 条机制轴中 7 条重复**（句柄 3 份、桶 2 份、代际发号器 2 份、槽位复用 3 变体、门面 ARO 2 份、重入守卫 2 份；仅"快照读取壳"已是单份）。两套机制的存在没有设计理由——它们的**字段集与服务对象本就相同**（`FTcsStateInstance` 与 `FTcsCastRun` 各有 `Entity` / `Level` / `ParamSnapshot` 三个语义相同的成员，两侧注释都自述"必须自持，不是冗余"）。

本变更**只合并机制、不动字段**：把句柄/桶/注册表/GC 遍历/重入守卫收敛为一份**模板化实现**，`TcsState` 与 `TcsSkill` **当场改用**（各自仍持自己的完整 struct）。字段的基底化与 payload 拆分归后续变更（`PLN-REFACTOR-1` Task 2/3）。

**依据** = `DEC-2026-10-13-state-module-positioning`（裁决④ 共享机制合并 / ⑨ 落点 / ⑫ 依赖边）+ `PLN-REFACTOR-1` Task 1（本变更的执行形状）。

## What Changes

- **统一句柄判据与转换（MUST NOT 合并类型）**：三个句柄（`FTcsStateHandle` / `FTcsCastRunHandle` / `FTcsSkillEntryHandle`）**保持为三个独立的反射类型**（幻影标签与"跨桶误传编译期失败"这道护栏**必须保留**），但**判据 / 相等比较 / 哈希 / 池位模式转换的语义收敛到一处公共件**。**判据统一为最严**（`Index >= 0 && Generation > 0`）——现状 `FTcsCastRunHandle` 判 `Index != -1`（**不看 `Generation`**），与另两者**真不一致**；对良构句柄**行为中性**。
  - **★ 为什么不合并（三条判据）**：① 合并会把"把状态句柄传给施法运行 API"从**编译期失败**降为**运行期查不到**（`instance-handle-pool` 为此立了两条判据）；② 一个类型的 `GetInner()` 只能返回**一个** `TTcsInstanceHandle<Tag>`，无法服务三个标签 ⇒ 单点转换必然退化为调用点各自 `static_cast`；③ 三个**反射身份本就必须各自声明**，实测三型代码各约 15 行（其余皆为注释）⇒ 真正的重复是**逻辑**。
  - **★ 已否决的绕过方案（MUST 记录以免重试）**：~~"一个宏生成三个类型"~~ —— 【引擎源码】实测引擎内**宏生成 `USTRUCT` 零先例**（自带 wrapper-struct 宏全部只生成普通 C++ struct），且 `GENERATED_BODY` 按 `__LINE__` 拼符号名、与 UHT 的按行生成是否对齐**本机无法确认** ⇒ 依既有纪律不建在未核实行为上。
- **统一桶与注册表**：`TTcsEntityRuntimeBucket<TPayload, THandleType>` + `FTcsEntityRuntimeRegistry<TPayload, THandleType>`，**载体 = `TTcsInstancePool`**（用户 2026-10-13 拍板）——取代"状态手写桶 + 技能池 + 技能账本手写桶"三种变体。
  - **★ 一处曾写错、已订正的表述（MUST 记录）**：初稿写"状态手写桶的自述理由是『一个池 = 一个句柄空间』，而**机制合并后该理由不再成立**"。**该表述是错的**——回读 `TcsStateRegistry.h` 原文，其自述理由是**两句并列**，第二句才是关键：**`Generation` 由本模块出线发号器统一分配不复用的奇数号**。该理由**在机制合并后依然成立**（桶是 per-entity 的、句柄不带实体段 ⇒ 各桶各自从 1 起发会**命中错实体**，是本仓 2026-10-05 实测过的缺陷）。
  - ⇒ **落法随之改为"池 + 可注入代际发号"**：给 `TTcsInstancePool` 加**第三模板参**（代际发号策略，**默认档 = 池内自增 = 既有行为逐字不变**），per-entity 桶注入**进程级**发号器。**这使 `TcsCore` 从"本轮零改动"变为"有一处加法式改动"**（计划 §0.3 的该预期已同批订正）——但**四个既有池使用者零行为差异**。
- **统一 GC 遍历口**：`ForEachForGC`（纯读、无线程断言、判据与常规遍历逐字相同）——由基底提供，两域门面的 `AddReferencedObjects` 共用。**★ 两域 ARO MUST 改走它**：状态侧的桶本轮**从手写换成池**，而池的常规遍历口都断言游戏线程 ⇒ 不改走此口会在**每次 GC** 留一条 `ensure(IsInGameThread())` 红字（技能侧 2026-10-08 已踩过同一坑）。
- **统一门面清理口**：世界反初始化清空 MUST 覆盖**技能侧现状的 7 个 owner**（不得只照状态侧的 3 个）。
- **统一重入广播守卫**：`InRemovalBroadcastHandles` / `InFinishBroadcastHandles` 合并为一份。
- **统一"重申请决策"接口**：`FTcsEntityAdmission`（`IsSameGroup` / `ShouldAccept` / `ResolveStacks`）合并现 `FTcsStateStackDecisionFragment` **与**技能 `ECastInstancing` 四路分流——**这是本变更消除的一处真机制重复**（技能在 `TcsCastOps.cpp` 内联实现了与状态五轴堆叠同型的决策树）。
- **⚠ BREAKING（仓内）**：`FStateBucket` / `FSkillBucket` / `FTcsStateRegistry` / `FTcsSkillRegistry` **被统一模板取代**；`FTcsStateStackDecisionFragment` **被统一决策接口取代**；三个句柄类型**保留**（仅判据统一 + 逻辑收敛）。消费方（含宿主装置）随批改指。
- **不做**：payload 拆分与 `FTcsEntityRuntime` 基底的创建（归 Task 2/3）；不改任何字段集；不改运行行为。

## Impact

- Affected specs: `state-instance-lifecycle`（句柄 / 桶注册表 / 门面清理 / ARO 四条需求）、`skill-registry`（账本句柄与桶）、`skill-cast-runtime`（施法运行态实例池）、**`instance-handle-pool`（★ 实测**需要**改动——池加第三模板参、`IsValid` 补奇偶校验、新增两个重载与五个按槽位下标驱动的口；初稿猜"沿用、可能无需改"是错的）**、`state-stacking-policies`（`Custom 决策 Fragment` 需求——决策接口化后形状改述；**已出 delta**）
- Affected code: `Source/TcsState/Public/Runtime/**`（新）、`Source/TcsState/Public/State/TcsStateHandle.h` / `TcsStateRegistry.h` / `TcsStateStackFragment.h`、`Source/TcsSkill/Public/Skill/TcsCastRunHandle.h` / `TcsSkillEntryHandle.h` / `TcsSkillRegistry.h`、两域门面与 `Ops`、**`Source/TcsCore/Public/Pool/TcsInstancePool.h`**（第三模板参 + 判据与口）、宿主装置 `Source/TcsDev/**`（LAC 仓——含 `FTcsDevStackDecisionAlwaysNew` 的签名跟随）
- **零新模块**（机制住共享层 `TcsState`）、**零新 tag 根**、**零行为变更**（硬判据：既有装置读数**逐字不变**；本轮已实测 10 条命令 + ARO 阴性对照复现）
