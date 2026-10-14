# 重构实施计划：运行态基底化、`TcsBuff` 拆出、资产加载层落地

- **文档 ID**：`PLN-REFACTOR-1`
- **类型**：PLN / 计划
- **状态**：ACTIVE（2026-10-13 建立；**实施未开工**——本计划是 `DEC-2026-10-13-state-module-positioning` **与** `DEC-2026-10-13-asset-butler` 两份裁决的**合并执行形状**）
- **权威范围**：**两条线合成一份执行形状**（★ 2026-10-13 合并，理由见 §0.5）——(A) 运行态**基底抽形**与**共享机制合并**、两个 payload 的拆分、**`TcsBuff` 新模块的拆出**；(B) **`TcsAssetButler` 新模块落地**、四处类型搬迁、定义库瘦身。附规格面回写与证据重取。**不在本文**——裁定本身（住两份 `DEC-*`）、关系表族语义（住 `DEC-2026-10-13-relation-*`）、`e1`/`e4` 的重写明细（各归 `PLN-R5.5` 改述后）
- **最后更新**：2026-10-14（**★ 同日第四笔 = Task 1 落地**：九个 Step 全部完成 + 双配置 0/0 + 无头装置 10 条读数逐字不变 + ARO 阴性对照复现（见 Task 1 末尾《实施记录》）。**两仓零提交**。**前一笔：开工门禁与日期留痕**——新增 **§0.5.2**「每个 Task 开工前 MUST 执行的两个固定动作」（**动作 A** 逐条对已登记债的触发条件，`MEM-20261010-08`；**动作 B** 日期取现场值，`MEM-20261004-05`；**动作 C** 合并前先出护栏表，`MEM-20261014-01`）；**动作 A 已实测命中一条**（`TcsDevSkillCastProbe_Phase.cpp` 仍 856 行，且 include 了 Task 1 要动的两个句柄头 ⇒ 动它前 MUST 显式选「绕开 / 踩响」）。**★ 日期留痕**：本计划与两份 `DEC-2026-10-13-*`、Task 1 提案**实际创建于 2026-10-14 凌晨**，文档 ID / 文件名 / 正文日期**一律沿用轮次日 2026-10-13**（跨零点收尾，改名牵动 ID/文件名/映射表/计数 ⇒ 收益零、漂移正）⇒ **留痕不改名**（先例 `MEM-20261004-12`）。**非**第四笔实质修正。**前两笔（2026-10-13）**：**第二笔：并入资产加载层线**——`DEC-2026-10-13-asset-butler` 裁定新建 `TcsAssetButler` + 四处搬迁 + 定义库瘦身；**两条线会改同一批文件**（`TcsEffect` / `TcsIntegration` / `.uplugin` / 模块计数）⇒ 合并为本计划，顺序见 §0.5。**第一笔**：**同日修正**：初稿按裁决⑦ 把 Task 4 写成"`TcsState` → `TcsBuff` **改名**"⇒ 用户评审时提问驱动实测，**该前提被推翻**（`TcsState` 是三层：共用 Def 族 + 通用运行时机制 + Buff 专属），现行 = **拆出新模块**、基底与机制**留 `TcsState`**。详见 §0.3.0 与该裁决 §6）

**Goal:** 一次交付三件事——① 让 `TcsState` 与 `TcsSkill` 的**运行态共享同一基底与同一套机制**（消除 8 条机制轴中重复的 7 条）；② 把 Buff 专属语义与机制**拆出为独立新模块 `TcsBuff`**；③ 为 Def 类资产建立**独立低层加载模块 `TcsAssetButler`**（类型注册表 / 加载三策略 / 异步默认 + 同步逃生口 / 加载防重 / 编辑器期规则），并把四样走错模块的类型**搬回各自的家**。**§0.2 给出工作量预期**（★ 按 2026-10-13 用户口径：**工作量不进决策判据**，只用于排期）。

**Architecture:** 三个层次同时成型 —— **① 资产加载层（新，最低）**：`TcsAssetButler` 只收**类型擦除的键**（`FPrimaryAssetType` + `UClass*` + 路径 + 策略），**不认识任何业务 Def 类型**，故可住得比所有注册者都低（本仓已有四处同型自注册先例）；**② 共享层**：`TcsState` 保留 = 两域共用的**定义族**（473 行）+ **通用运行时机制**（956 行），运行态改为**基底 + 两个 payload**；**③ 领域层**：`TcsBuff`（Buff 专属，新建）/ `TcsEffect`（收下两个 Def 资产类）/ 接线层（收下实体组件与 PIE 查询）。

**Tech Stack:** UE 5.8 C++、UBT（Development Editor + Shipping 双配置）、`TTcsInstancePool`（代际池，已在 `TcsCore`）、`UAssetManager` / `FStreamableManager`（引擎既有，本机源码可读：`E:\UnrealEngine\UE_5.8`）、宿主装置 `Source/TcsDev/`（LAC 仓）。

**Spec:** [`DEC-2026-10-13-state-module-positioning`](../decisions/dec-2026-10-13-state-module-positioning.md)（十二项裁定 + Task 轮廓 + 风险登记）**与** [`DEC-2026-10-13-asset-butler`](../decisions/dec-2026-10-13-asset-butler.md)（八项裁定 + 四条待实测项）；执行者 MUST 先读前者的 §1/§3/§4 与后者的 §0/§2/§6。

---

## Global Constraints

- **禁 TDD**（用户级最高纪律，覆盖 `writing-plans` 的 TDD 要求）：**无失败测试步骤**；每任务验证 = UBT **双配置编译** + 定向人工检查 + 宿主装置读数。
- **提交纪律**（用户级最高优先级，覆盖任何"频繁提交"要求）：**未经用户明确授权 MUST NOT `git commit` / `push`**。本计划**不含提交步骤**；任务边界即"**停点待检查**"。**跨仓**：插件仓（TCS）与宿主仓（LAC）各自独立提交；**LAC 是混合仓，禁 `git add -A`**。
- **风格**：动 C++ 前执行 `unreal-cpp-style`——Tab 缩进、UTF-8 无 BOM、LF；`.h` 用 region、`.cpp` 扁平；单 `.cpp` ≤300 行；文件头 `// Copyright Tirefly. All Rights Reserved.`。
- **命名**：类型 = 前缀字母 + `Tcs` + 语义名；枚举值 = 枚举名缩写全大写；API 导出宏 `TCS<模块>_API`；动词纪律（`Resolve` 仅用于句柄/Id→对象）。
- **UHT 两条硬门槛**：① **全项目头文件名唯一**（同名不同目录照样失败）；② **反射类型去前缀后不得同名**（`FTcsX` vs `UTcsX` 必炸）。
- **依赖铁律（本轮改动后的目标形态）**：**三层同时成立** ——
  - **① 加载层最低**：`TcsCore ─┬─ TcsNotation` / `└─ TcsAssetButler`（**零 Tcs 业务依赖**，只认 `FPrimaryAssetType` + `UClass*` + 路径 + 策略）；
  - **② 共享层**：`TcsAttribute ─→ TcsEffect ─→ TcsState`（`TcsState` **不再依赖 `TcsEffect`**——裁定⑫）；
  - **③ 领域层**：`TcsSkill ─→ TcsIntegration`；`TcsBuff`（新）与 `TcsSkill` 平级（都依赖 `TcsState`）；**各域（含层级最低的 `TcsAttribute`）经宏向 `TcsAssetButler` 向下注册**（类型擦除 ⇒ 无环，先例 = `FTcsEffectStepExecutorRegistry` 等四处）。
  - **新增约束**：`TcsCore` / `TcsAssetButler` MUST NOT 依赖任何兄弟 Tcs 业务模块 ⇒ **本轮不把战斗域概念下移进 Core**（裁决 §6.4 修正 ⑨）。
- **注册键 MUST 类型擦除**（裁定③）：`TcsAssetButler` 的注册表**只收** `FPrimaryAssetType` + `UClass*` + 扫描路径 + 策略，**MUST NOT** 认识任何业务 Def 类型（否则它无法住低、各域无法向下注册）。
- **资产重定向纪律**：搬迁已落资产的类型（`UTcsEffectChainDef` 等，实测 **14 个资产**引用 `/Script/TcsIntegration.*`）**MUST** 同批配重定向（优先引擎自带的 `PrimaryAssetTypeRedirects` / `AssetPathRedirects`，见 §6 T2），MUST NOT 只改代码不管资产。
- **`IsDataValid` 收尾**：无错无警时 MUST 把基类返回的 `NotValidated` 提升为 `Valid`。
- **零消费者不预建**：字段可先就位，但**不得为无消费者的字段造机制**；未落地面 MUST 写进计划注记 + `LEDGER-deferred`。
- **过网结构纪律**：MUST 纯反射数据——禁 `TFunction`、禁 `TMap`/`TSet` 作 `UPROPERTY`；`FInstancedStruct` 内层须可复制。
- **规格先行**：每个 Task 的实施以 OpenSpec 提案开道（`openspec validate <id> --strict --no-interactive` 通过 → 实施 → `openspec archive <id> --yes`）；归档时 MODIFIED 需求的**标题 MUST 与主规格逐字一致**。
- **单游戏线程假设**：`TTcsInstancePool` 的每个常规进入点 `ensure(IsInGameThread())`；GC 期遍历 MUST 走 `ForEachForGC`（无线程断言、纯读）。
- 设计规格来源：`DEC-2026-10-13-state-module-positioning`（本重构的裁定）；冲突时以该裁决 + 用户拍板为准。

---

## Review Focus

> 以下五类是本重构**最容易把人咬伤**的地方，且**没有任何编译面会拦住**。每条都已在下述对应 Task 拥有读数项。

1. **`AddReferencedObjects` 漏掉新增的嵌套容器** —— 基底化后池/桶结构变了，GC 遍历路径随之改变。漏一处 ⇒ 参数快照里的对象**静默被回收**（症状 = 参数变空引用，不崩溃）。**已有先例与阴性对照**：`EVID-2026-10-08-skill-cast-runtime` 曾用"关掉 ARO ⇒ 24 PASS / 2 FAIL（存活=否）"证明过这条路径。→ Task 1 Step 6 / Task 2 Step 5 / Task 3 Step 5。
2. **`Reset` / `Deinitialize` 的清空不完整** —— 基底化后门面要清的 owner 从 3 个变 7 个（技能侧现状）。漏清 ⇒ **跨世界残影**（旧句柄在新世界被误判有效）。→ Task 1 Step 7。
3. **到期回调里沿用了失效指针** —— 基底化把"到期锚"提到基底，回调捕获方式随之改变。现状已有明文纪律（"到期回调的入口 MUST 先把自己的锚点复位"、"起链会搬移池 ⇒ `Run` 指针可能已失效"）。违反 ⇒ **悬空句柄或静默丢弃**。→ Task 2 Step 4 / Task 3 Step 4。
4. **重入广播守卫换类型时漏改判据** —— 两个守卫（`InRemovalBroadcastHandles` / `InFinishBroadcastHandles`）合并后，句柄类型变了。漏改 ⇒ **递归移除不被拦**（栈溢出或重复广播）。→ Task 1 Step 8。
5. **拆模块时漏改字符串面（非符号）** —— `TCSSTATE_API`（23 文件）/ `LogTcsState`（16 文件）**留在共享层不改**；而 `TcsStateParam`（**它是 tag 根名，MUST NOT 随模块拆动**）/ 内容资产里的 `ApplyState` 步骤名 **随 Buff 层搬家但注册名 MUST NOT 变**。改错 ⇒ 静默失联（tag 根一改，**已落资产全部找不到键**）。→ Task 4 Step 3 / Step 6。
6. **搬类型时漏配资产重定向** —— `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` 被 **14 个已落资产**以 `/Script/TcsIntegration.<类名>` 硬引用，而本仓 **`CoreRedirects` 零配置**。漏配 ⇒ **资产打开即报类找不到**（编辑器可见，但只在"有人打开那个资产"时暴露；无头装置**照样通过**）。→ Task 8 Step 2 / Step 6。
7. **加载层的模块加载阶段排后** —— `AssetManagerClassName` 指向的类若在其模块尚未加载时被解析，引擎是 **`Fatal`**（不是静默回退）。9 个既有模块全 `LoadingPhase: Default`，而 `TcsAssetButler` 是新模块 ⇒ 阶段必须实测。→ Task 8 Step 3 / §6 T3。
8. **把业务类型混进加载层** —— `TcsAssetButler` 一旦 include 任一业务 Def 头，它就必须住到那些类型之上 ⇒ **各域无法再向下注册**（本会话已实测该环）。→ Task 8 Step 4 的"零业务 include"验收。

---

## §0 开工前置

### 0.1 本轮输入（逐条给落点）

| 输入 | 原归属 | 本计划落点 | 处置 |
|---|---|---|---|
| `DEC-2026-10-13-state-module-positioning` 十项裁定 | 用户 2026-10-13 拍板 | 全文 | ✅ 已消费（它是本计划的 Spec） |
| 该裁决 §5 的 Task 1~7 轮廓 | 同上 | **Task 1~7**（本计划把它展开为 Step） | 展开 |
| `STAT-7`（"层 = 独立实例"，层模型级变更） | 台账 | **Task 1 Step 4 的显式裁决点** | 见 §0.3 |
| `STAT-6`（`PerStackValue` 零行为） | 台账 | **Task 2 Step 3 的显式裁决点** | 见 §0.3 |
| `PLN-R5.5` Task 1 / Task 4（绑在 `FTcsCastRun`） | `R5.5-e` | **Task 3 的连带**（改指新基底） | 见 §0.4 |
| `R6.5-g`（三支柱之一 = `StateInstance`） | `R6.5` | **Task 5 的连带**（规格面同批改述） | 见 §0.4 |

### 0.2 工作量预期（★ 按 2026-10-13 用户口径：**只用于排期，不进决策判据**）

> **来源** = `DEC-2026-10-13-state-module-positioning` §5.2 原把"净收益可能为负"标为**高风险**；**该判已按用户口径作废**（见下"对账结论"）。本节保留两侧清单的用途是**知道要做多少事**，MUST NOT 被当作"该不该改"的论据。

**收益（可复核）**

| 项 | 量化 |
|---|---|
| 消除机制重复 | **7 / 8 轴**（句柄 3 份 → 1、桶 2 份 → 1、代际发号器 2 → 1、槽位复用 3 变体 → 1、门面 ARO 各一份 → 共享、重入守卫 2 → 1） |
| 消除**一处真机制重复** | 技能门禁④ `Instancing` 四路分流 = 状态五轴堆叠决策树（裁决 §1.3）⇒ 合并为一处决策接口 |
| **名实相符**（★ 修正后更准） | Buff 专属语义 + 机制拆出为 `TcsBuff` ⇒ `TcsState` 保留的正是它真正承担的：**两域共用的定义层与运行机制**（≈1,430 行） |
| **去掉一条依赖边**（★ 修正后新增的收益） | 拆出后 `TcsState` 只剩 `Core`/`Notation`/`Attribute` ⇒ **`TcsEffect → TcsState` 可去掉**（实测 `TcsEffect` 的使用 100% 在 Buff 专属层） |
| 基底字段复用 | `Entity` / `Level` / `ParamSnapshot` 三个语义相同字段不再各写一遍 |

**代价（可复核）**

| 项 | 量化 |
|---|---|
| 改动面 | 触达 `FTcsCastRun` **21 文件**（插件内 69 个成员点）+ 触达 `FTcsStateInstance` **24 文件**（约 148 点）+ 宿主装置 **61 文件 / 13,325 行**在波及范围内 |
| **新增模块** | 新模块 `TcsBuff`（≈5,200 行搬入）+ 模块清单三处计数（`uplugin` / `plugin-descriptor` / `project.md`）**9 → 10**（★ 方案 A：Task 4 先行；随后 Task 8 再推 10 → 11） |
| 规格面 | 约 **11 个能力**需 MODIFIED delta；最重 = `skill-cast-runtime`（明文要求 `FTcsCastRun` 非反射 + 必须经 `TTcsInstancePool` + "MUST NOT 新建池实现" + 逐条列 8 必具字段） |
| 归档面 | 约 **16 个已归档提案**的规格成为历史 |
| 证据面 | **≥11 篇 `EVID` 作废**并需重取（含 `EVID-2026-10-08-skill-cast-runtime` 的 **ARO 阴性对照**——这类"关掉机制看是否失败"的读数难以复现） |
| 回归风险 | 两个已收束轮次（R5 / R6 及其 R6.5-1）的验收面被整体重开 |

**对账结论（★ 2026-10-13 按用户口径重述，MUST 读）**：

> **用户口径（2026-10-13）**：**TCS 仍处设计开发阶段** ⇒ **文档变更、版本迭代、甚至回滚重做都是正常的**。
> ⇒ **推论：一次性返工代价在本项目不是决策依据。** 上表"代价"的用途**只是排期与心理预期**（知道要做多少事），**MUST NOT** 被用作"该不该改"的论据，**MUST NOT** 被用作把设计问题推迟的理由。

- **决策依据只剩两条**：① **设计是否正确**（名实相符 / 机制不重复 / 依赖方向干净）；② **是否可逆可验证**。**成本不进判据。**
- **收益是"结构性"的**；**代价是一次性的，且随项目推进只会变大**（R7 之后要连带 StateTree 接线与实体状态机）。
- ⇒ **顺序结论不变**（本重构 MUST 在 R7 之前），但**理由从"省代价"改为"避免在错误地基上继续施工"**。
- **唯一保留的否决条件**（已按新口径收窄）：若两个裁决点（`STAT-7` / `STAT-6`）导致**基底粒度反复**，那是**设计未想清**的信号 ⇒ 应**停下把设计想清**，MUST NOT 硬推——**这是正确性判据，不是成本判据**。

### 0.3 计划不得建在错前提上：两个裁决点 + 四处已知陷阱 + ★ 一处已修正的架构前提

**★ 0.3.0 模块归属（2026-10-13 同日修正，MUST 先读）**

本计划初稿按裁决⑦ 把"`TcsState` → `TcsBuff` 改名"写成 Task 4。**该前提已被实测推翻**（用户评审时提问驱动，修正记录见裁决 §6）：

- **实测**：`TcsState` 55 文件 / 6,663 行是**三层** —— **共用 Def 族 473 行**（`FTcsSkillDefData : FTcsStateDefBase` 与 `UTcsSkillDef : UTcsStateDef`，**技能继承它**）+ **通用运行时机制 956 行**（快照 / 参数读取壳 / 注册表 / 句柄，两域共用）+ **Buff 专属约 5,200 行**。
- ⇒ **改名会把两域共用的 Def 族吞进"Buff"**，而"技能继承 Buff 定义基类"是**语义错误**。
- ⇒ **现行形态**：`TcsState` **保留**为共享层（≈1,430 行）；**`TcsBuff` 是新建模块**（Buff 专属那部分搬过去）；**运行时机制与基底留 `TcsState`**（**不改 `TcsCore`**——基底属战斗域概念）。
- ⇒ **连带收益**：拆出后 `TcsState` 只剩 `Core` / `Notation` / `Attribute` 三个依赖，**`TcsEffect → TcsState` 这条边可去掉**（实测 `TcsEffect` 的使用 100% 在 Buff 专属层）。

**两个显式裁决点**（任务内 MUST 二选一并写清判据，MUST NOT 悬空）：

1. **`STAT-7`：层模型是否要重开？** 台账记"层 = 独立实例"属**层模型级变更**。本重构把"重申请决策"接口化 ⇒ 该变更**技术上第一次变得便宜**。**裁决点** = 是否借此机会把`组内至多一条实例`改为`组内可多条`。**倾向**：**不改**（现模型有 13 项装置读数背书；改它属独立议题），但**必须在 Task 1 Step 4 写明"本轮不改 + 理由"**，MUST NOT 留白。
2. **`STAT-6`：`PerStackValue` 的零行为是否顺手补？** 它需"层数读口 + 按层参数源"这套新机制。**倾向**：**不补**（零消费者；补它 = 为新机制开工作面，违反"零消费者不预建"）。

**三处已知陷阱**（本会话已实证，MUST NOT 重踩）：

| # | 陷阱 | 事实 |
|---|---|---|
| ① | **"技能运行态要不要并入状态运行态"的旧争点建立在一个错论据上** | 旧论据 = "状态'组内至多一条'与技能'每次并存'互斥" ⇒ **不成立**。`FTcsStateStackDecisionFragment::IsSameGroup` 是**可覆写虚函数**且 `EGB_Custom` 档真调用它，宿主样本 `FTcsDevStackDecisionAlwaysNew` 已实证"恒不同组 ⇒ 每次建新条"（PASS 读数：检查 `22m`，在册 = 2）。**「组内至多一条」是默认策略的结果，不是结构不变量。** |
| ② | **`Def` 基类 MUST NOT 进 `TcsCore`** | 它持 `TArray<FTcsNumericParamRow>`，其约定列类型 `ETcsValueConventionFlag` 住 `TcsNotation`，而 **`TcsCore` 与 `TcsNotation` 平级互不依赖**（`TcsNotation.Build.cs` 只依赖 `Core`+`CoreUObject`）⇒ 下移会造出 `Core → Notation` 的环。**只有运行态基底与机制进 Core。** |
| ③ | **`Phase` 一词在本仓有两种意思** | `EStatePhase{ESP_Inactive,ESP_Active,ESP_Expiring}` 是**实例生命周期态**（`TcsStateEnums.h`）；`FTcsCastRun::PhaseIndex` 是**施法时段游标**（`TcsCastRun.h`）。**同词不同义、互不相关。** 基底化的生命周期态取前者语义，MUST NOT 与时段游标混为一谈。 |
| ④ | **~~"Def 基类 MUST NOT 进 Core，但运行态基底可以"~~** | **★ 该措辞已随 §0.3.0 收窄**：**两者都留 `TcsState`**。`Def` 基类不能进 Core 的理由不变（依赖 `TcsNotation` / `TcsAttribute`）；运行态基底**也不进** Core（它是战斗域概念，进 Core 会重演"把域概念放进通用层"这一同型错误——只是换了个方向）。**现结论：`TcsCore` 本轮零改动。** |

### 0.4 对既有计划的连带（裁定⑧ 的执行）

| 对象 | 处置 | 何时 |
|---|---|---|
| `PLN-R5.5` 状态行 + Task 1 Step 7 / Task 4 | 已就地标注"被本重构前置"；**`e1`/`e4` 的重写落在 Task 3 之后** | Task 3 收尾 |
| `PLN-R6` 的 `R6.5-g` | 已就地标注"三支柱之一被重定义"；规格面改述落 Task 5 | Task 5 |
| `PLN-R5`《R5.5 批次表》`e` 行 | 补追注说明 `e1`/`e4` 被前置 | Task 7 |
| `SPEC-06-integration` §1 依赖行 + `DEC-00` §9 模块表 | StateTree 归属口径（裁定⑦）+ 模块表两次变更（`TcsBuff` / `TcsAssetButler`） | Task 4 Step 8 / Task 8 Step 8 |

### 0.5 ★ 为什么两条线合成一份计划（2026-10-13 第二笔）

**背景**：本计划初稿只覆盖"运行态基底化 + `TcsBuff` 拆出"（源自 `DEC-2026-10-13-state-module-positioning`）。同日 `DEC-2026-10-13-asset-butler` 追加了第二条线（`TcsAssetButler` + 四处搬迁 + 定义库瘦身）。**两条线会改同一批文件**：

| 同一批文件 / 对象 | 线一（基底化） | 线二（加载层） |
|---|---|---|
| `TcsEffect` | 收 `TcsStepApplyState` 的反向边调整（去 `TcsEffect → TcsState`） | **收下** `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` |
| `TcsIntegration` | — | **被掏空**（两 Def 类搬走、两接线类型搬走、定义库可能瘦身） |
| `.uplugin` + 模块计数三处 | `TcsBuff` 入册（**9 → 10**，★ **先发生**——方案 A） | `TcsAssetButler` 入册（**10 → 11**） |
| `DEC-00` §9 模块表 + 依赖链 | 改 `TcsState` 职责 + 去 `TcsEffect` 边 + 模块数 **11 → 12**（**先发生**） | + `TcsAssetButler`（**12 → 13**）+ 依赖链 |
| `INDEX` / `GLOSSARY` 计数 | 改 | **再改一次** |
| `plugin-descriptor` / `openspec/project.md` | 改 | **再改一次** |

⇒ **若分两份计划各跑一轮**，上表每一格都会被**改两次**（第二次还要处理第一次的中间态），且"模块计数"这类**无校验器的计数声称**在同一轮内改两次会显著提高漂移概率（本会话已实证多处计数漂移）。

**⇒ 合并为一份计划，分阶段执行**（Task 编号连续，执行顺序见下表）：

> **★★ 2026-10-13 第二笔订正（本表顺序的依据有错，MUST 读）**：本表初稿把 **Task 8 排为"阶段 1、MUST 最先"**，给的理由是「`TcsEffect` 收下两个 Def 类后，线一的『去反向边』才有落点」。**该理由实测不成立** —— `TcsState → TcsEffect` 这条边由 **`TcsState` 自己的 4 个文件**产生（`TcsBuffDef.h` / `TcsStateEvents.cpp` / `TcsStateOps_Trigger.cpp` / `TcsStepApplyState.cpp`），它们**全部在 Task 4 搬走**；而 Task 8 搬的是 **`TcsIntegration`** 的两个 Def 类，**两者无交集**。⇒ 那半句是**按"都在动 `TcsEffect`"的表面相邻推断因果**，属本项目反复记录的根因（按词形/表面相邻配对，见 `MEM-20261013-09`）。
>
> **实测另有两条真实约束，方向与本表初稿相反**：
> - **① Task 4 先于 Task 8 更省重定向**：Task 8 要为各域自注册 DefAsset 类型。若它先跑，登记的是住 `TcsState` 的 `UTcsBuffDefAsset`；随后 Task 4 把它搬到 `TcsBuff` ⇒ **2 个已落资产**（`DA_Check_BuffDef` / `DA_State_E2E`，实测均引用 `/Script/TcsState.TcsBuffDefAsset`）须**额外配重定向**。反之 Task 4 先行则**少这 2 处**。
> - **② 原动力优先**：本计划的**发起缘由** = 用户质疑「技能激活为何不经过 `ApplyState`」⇒ 即**技能运行态与状态运行态的统一**（= Task 1 + Task 3）。把它排在一条**后起的支线**（资产加载层）之后，与动机相悖。
>
> **⇒ 顺序已由用户拍板 = 方案 A（运行态线先行）**，见 §0.5.1。**本表下方的"阶段"标注已按 A 改写**；编号仍是"归属"、**不表示顺序**。

| 阶段 | Task | 内容 | 关键约束 |
|---|---|---|---|
| **阶段 1 · 运行态** | **Task 1 → 2 ∥ 3** | 基底抽形 + 7 轴机制合并（1）→ Buff payload（2）/ Cast payload（3） | **先做发起缘由**（技能运行态归入 State 基底） |
| **阶段 2 · 拆模块** | **Task 4** | 拆出 `TcsBuff` | 依赖阶段 1 的 payload 化完成；**先于 Task 8**（少 2 处资产重定向） |
| **阶段 3 · 加载层** | **Task 8** | `TcsAssetButler` 落地 + 四处搬迁 + 定义库瘦身 | 依赖阶段 2（`UTcsBuffDefAsset` 已在 `TcsBuff` ⇒ 少 2 处重定向）；**其 4 项待实测（§6 T1~T4）在本阶段首步** |
| **阶段 4 · 收尾** | **Task 5~7** | 规格回写 / 证据重取 / 收束 | **MUST 后于 Task 8**——规格须反映**两条线**的最终形状 |

### 0.5.1 顺序方案（★ 已拍板：**方案 A**）

| | **方案 A（✅ 已采用）：运行态线先行** | 方案 B（未采用）：加载层先行 |
|---|---|---|
| 顺序 | **Task 1 → 2 ∥ 3 → 4 → 8 → 5 → 6 → 7** | Task 8 → 1 → 2 ∥ 3 → 4 → 5 → 6 → 7 |
| **动机对齐** | ✅ **先做发起缘由**——本计划因"技能激活为何不经过 `ApplyState`"而起，Task 1+3 正是答案 | ❌ 发起缘由排在一条后起的支线之后 |
| **重定向负担** | ✅ `UTcsBuffDefAsset` 已在 `TcsBuff` ⇒ **少 2 处**（`DA_Check_BuffDef` / `DA_State_E2E`） | ❌ 需为这 2 个资产额外配重定向 |
| **地基一致性** | ⚠️ Task 8 的"各域自注册"要等到阶段后段才建立；但**它不阻塞 Task 1~4**（后者不动注册机制） | ✅ 模块拓扑一次定型，后续不再变 |
| **风险** | Task 8 的 4 项待实测（§6 T1~T4）压在后面 ⇒ **若 T1/T3 结论不利，前面已投入的工作不白费**（不依赖它们） | T1/T3 若结论不利，会影响整个后续阶段（因为它是"最低层"） |
| **关键判据** | **Task 1 与 Task 8 之间无依赖**（实测：两线改的文件集无交集）⇒ **顺序可自由选** | 同左 |

**★ 已拍板采用 A**（用户 2026-10-13），判据两条：**① 先解决发起缘由**（本计划正因"技能激活为何不经过 `ApplyState`"而起）；**② 少 2 处资产重定向**（实测）。

**⇒ 本计划的阶段标注与依赖图已按 A 同批改写完毕。**

### 0.5.2 ★ 每个 Task 开工前 MUST 执行的两个固定动作（2026-10-14 立，可核）

> **为什么单列一节**：两条纪律都已入库，但**它们的执行动作没有落点** ——
> 纪律只以"要注意"的形态存在时，执行时不会有触发点（同 `MEM-20261006-08`）。
> ⇒ 把它写成**计划里的固定动作**，而不是留在记忆卡里靠人记得。

**动作 A：逐条对「已登记债的触发条件」**（依 `MEM-20261010-08`）

开工前 MUST 把**全部**在册债的触发条件对着**本 Task 的改动面**过一遍（不是只对看起来相关的那条），
并把结论写进该 Task 的首个 Step。

> **★★ 本节初稿的判定是错的，已订正（2026-10-14，MUST 读）**：初稿写「`TcsDevSkillCastProbe_Phase.cpp`（856 行）
> **挂在波及面上**」，依据是"它 include 了 Task 1 要动的两个句柄头"。**该推断不成立** ——
> 实测该文件对 Task 1 的三个合并目标（`TcsStateRegistry` / `TcsSkillRegistry` /
> `FStateBucket` / `FSkillBucket` / `FTcsStateInstance` / `FTcsStateHandle`）**零引用**，
> 且 Task 1 对那两个句柄头的动作是**「保留类型、只把判据委托公共件 + 改注释」**
> （Step 2 明文）⇒ **类型名 / 字段 / API 全不变**，探针的 `Run.IsValid()` 行为对良构句柄**逐例不变**。
> **根因** = 把「**include 了这个头**」当成「**会被这个改动影响**」——属按表面相邻推断因果
> （同 `MEM-20261013-09` 根因，且是**同一天第二次**）。**处置 = 绕开**（见下表）。

**实测结论（逐文件核过，取代初稿）**：

| 文件 | 行数 | 是否引用 Task 1 的合并目标 | 处置 |
|---|---|---|---|
| `TcsDevSkillCastProbe_Phase.cpp` | **856** | ❌ **零引用**（只 include 两个**保留类型**的头） | ✅ **绕开**——不触碰 |
| `TcsDevSliceRig_Skill.cpp` | **1129** | ✅ include `TcsSkillRegistry.h`（Task 1 Step 3 **真重写**） | ✅ **绕开**——只改 include / 调用点，**MUST NOT** 拆分 |
| `TcsDevSliceRig_CooldownCost.cpp` | **1671** | ✅ 同上 | ✅ **绕开**——同上 |
| `TcsDevSkillRegistryProbe.cpp` | 310 | ✅ 同上（**未超限**，债不适用） | 照常改 include |

> **★ 两处「绕开」的可核项（收尾 MUST 逐条复核，MUST NOT 只写"本轮不拆"）**：
> ① `TcsDevSkillCastProbe_Phase.cpp` 行数**仍为 856**（且**内容零改动**）；
> ② 两个超限 Rig 文件行数**不因本 Task 增长**（只允许 include 行/调用点的等量替换，
> 拆分 MUST NOT 发生）。**若任一被拆，该次拆分作废其此前全部验收 ⇒ MUST 计入本 Task 的回归面并重验。**

**用户裁定（2026-10-14）**：三处**一律绕开**（`TcsDevSkillCastProbe_Phase.cpp` / 两个超限 Rig 文件），
**MUST NOT** 借 Task 1 顺手拆分——**拆分的收益是"文件更短"（无功能收益），代价是作废已验收判据的执行体。**

**动作 B：日期取现场值**（依 `MEM-20261004-05`）

写任何**文档日期 / 文档 ID / 归档目录名 / 变更记录条目**之前，MUST 先跑一次
`Get-Date -Format 'yyyy-MM-dd'`，MUST NOT 由上一批文档的日期或会话上下文推断。
**本计划的建立即跨零点**，且**本轮发生过一次复犯**（见下方变更记录 2026-10-14 条）。

> **★ 留痕规则（本轮确立，与仓内既有先例一致）**：内容按**轮次日**、归档目录按**机器日**，
> 两者错时**MUST 在错位处留痕、不改名**（先例 `MEM-20261004-12`：归档日 10-06 vs 计划预写 10-05）。
> ⇒ **本轮真正缺的是那处留痕，不是把已写的 `2026-10-13` 全部改掉。**

**动作 C：新立条款前先出「护栏表」**（依 `MEM-20261014-01`）

凡本计划涉及**合并 N 个同形物为 1**（句柄 / 桶 / 注册表 / 门面 / 守卫）的 Step，
MUST 先列一行为一物的**护栏表**（该物独有的拦截 / 拦截靠什么成立），
再判定"**只合并逻辑**"还是"**声明也合并**"——**只因为它们是不同类型而成立的拦截，合并即失效**。

---

## 文件结构（动工前先锁定职责边界）

### 新建（`TcsState`，共享基底与机制 · 阶段 1）

> **★ 两个文件的创建时机不同（MUST 读）**：`TcsEntityRuntime.h`（基底 struct）在 **Task 2** 创建（Task 1 只定字段表——否则它当场没有消费者，违反"零消费者不预建"）；其余四件在 **Task 1** 创建并**当场被两域采用**。

| 文件 | 职责 |
|---|---|
| `Source/TcsState/Public/Runtime/TcsEntityRuntime.h` | 基底运行态 struct（身份/归属/快照/一个到期锚/生命周期态）+ 生命周期枚举。**★ 该文件在 Task 2 创建**（Task 1 只定字段表，见 Task 1 Step 1） |
| `Source/TcsState/Public/Runtime/TcsInstanceHandleOps.h` | **判据 / 相等比较 / 哈希 / 池位模式转换的语义单点**（非反射公共件）。**★ MUST NOT 新增统一句柄类型**——三个句柄保持独立，各自委托本件（裁决 §8） |
| `Source/TcsState/Public/Runtime/TcsEntityRuntimeBucket.h` | per-entity 桶（**载体 = `TTcsInstancePool`**，用户 2026-10-13 拍板） |
| `Source/TcsState/Public/Runtime/TcsEntityRuntimeRegistry.h` | 桶容器（`TMap<Handle, TUniquePtr<Bucket>>` + 遍历） |
| `Source/TcsState/Public/Runtime/TcsEntityAdmission.h` | **重申请决策接口**（可覆写）+ 决策结果类型（合并现堆叠决策与技能 Instancing） |

### 修改（`TcsState`，共享层）

| 文件 | 改动 |
|---|---|
| `Source/TcsState/TcsState.Build.cs` | **删除 `TcsEffect` 依赖**（裁定⑫：拆出 Buff 专属后不再需要）；保留 `TcsCore` / `TcsNotation` / `TcsAttribute` |
| `Source/TcsState/Public/State/TcsStateSnapshot.h` | **保留**（不再下移 Core；断言其"参数域概念"的自述仍成立，只是落点是共享层而非 Core） |
| `Source/TcsState/Public/State/TcsStateHandle.h` / `TcsStateRegistry.h` | 由 `TcsEntityRuntimeHandle` / `TcsEntityRuntimeBucket`+`Registry` 取代（**原地替换，不跨模块搬迁**） |

### 新建（`TcsBuff`，Buff 专属模块）

| 文件 | 职责 |
|---|---|
| `Source/TcsBuff/TcsBuff.Build.cs` | 模块定义；依赖 `TcsCore` / `TcsNotation` / `TcsAttribute` / **`TcsEffect`**（链步骤）/ `TcsState`（共享层） |
| `Source/TcsBuff/Public/State/TcsBuffPayload.h` | **Buff payload**：`Stacks` + `DurationRemaining` + `PeriodRemaining` + `PeriodEntry` |
| `Source/TcsBuff/Public/Def/TcsBuffDef.h` / `TcsBuffDefAsset.h` / `TcsBuffDefTableRow.h` | 自 `TcsState` **搬入**（含 `FTcsEffectTriggerDef Triggers` ⇒ 这是 `TcsEffect` 依赖的落点） |
| `Source/TcsBuff/Public/State/TcsBuffStackPolicy.h` / `TcsBuffStackFragment.h` | 自 `TcsState` 搬入 |
| `Source/TcsBuff/Public/State/TcsBuffEvents.h` / `TcsBuffBehaviorFragment.h` / `TcsBuffModifierMaterializer.h` | 自 `TcsState` 搬入 |
| `Source/TcsBuff/Public/Chain/TcsStepApplyState.h` | 自 `TcsState` 搬入 |
| `Source/TcsBuff/Public/Param/TcsParamSource_StateLevel.h` / `TcsParamSource_InstigatorLevel.h` / `Host/TcsEntityLevelProvider.h` | 自 `TcsState` 搬入 |
| `Source/TcsBuff/Public/TcsBuffSubsystem.h` + 对应 `Private/` 全体 | 自 `TcsState` 搬入（`TcsStateSubsystem*` → `TcsBuffSubsystem*`；`TcsStateOps_*` → `TcsBuffOps_*`） |

> **★ `TcsBuffSubsystem` MUST NOT 重复声明共享层的登记口**：`RegisterStateDef` / `GetRegisteredStateDef` 收的是**两域共用**的 Def 基类 ⇒ 归共享层（`TcsState`）；`TcsBuffSubsystem` 只补 Buff 专属面向（施加 / 移除 / 堆叠）。边界判据 = **该口是否被 `TcsSkill` 用到**。

### 修改（`TcsSkill`）

| 文件 | 改动 |
|---|---|
| `Source/TcsSkill/Public/Skill/TcsCastPayload.h`（新） | **Cast payload**：`EntryHandle` + `PhaseIndex` + `ChainRunHandle` + `ActiveChainId` + `MainChainStart` + `MainChainStartPhaseTag` |
| `Source/TcsSkill/Public/Skill/TcsCastRun.h` | 删除（被基底 + payload 取代） |
| `Source/TcsSkill/Private/Skill/TcsCastOps*.cpp` | 改读基底 + payload；门禁④ 改走共享决策接口 |
| `Source/TcsSkill/Public/TcsSkillSubsystem.h` | 池/注册表/ARO/重入守卫改为复用共享层版（`TcsState`） |

### 新建（`TcsAssetButler`，资产加载层）

| 文件 | 职责 |
|---|---|
| `Source/TcsAssetButler/TcsAssetButler.Build.cs` | 模块定义；依赖 **`Core` / `CoreUObject` / `Engine` / `GameplayTags` / `AssetRegistry`**（**零 Tcs 业务模块** —— 裁定②） |
| `Source/TcsAssetButler/Public/TcsAssetButlerModule.h` + `.cpp` | 模块壳 |
| `Source/TcsAssetButler/Public/TcsAssetButlerLogChannel.h` + `Private/.cpp` | `LogTcsAssetButler`（照 D0-6 v2 通道文件惯例） |
| `Source/TcsAssetButler/Public/Asset/TcsPrimaryAssetEntry.h` | **注册项**：`FPrimaryAssetType` + `UClass*`（BaseClass）+ 扫描路径 + `bHasBlueprintClasses` + 加载策略 + `bIsEditorOnly`（**类型擦除，零业务类型**） |
| `Source/TcsAssetButler/Public/Asset/TcsPrimaryAssetRegistry.h` + `Private/.cpp` | **类型注册表**：`Register(Entry)` / `Unregister` / 遍历；**同类型重复 MUST 报错**（对齐引擎 `Only a single entry per type` 语义，**不自建第二套唯一性判据**） |
| `Source/TcsAssetButler/Public/Asset/TcsAssetButlerTags.h` | **自注册宏 `UE_DEFINE_PRIMARY_ASSET_TYPE(...)`**（照既有四枚 `UE_DEFINE_*` 形态：静态注册器 + 构造期登记） |
| `Source/TcsAssetButler/Public/Asset/TcsAssetManager.h` + `Private/.cpp` | **`UTcsAssetManager : UAssetManager`** —— 覆写 `StartInitialLoading()`：`Super::` → 遍历注册表逐一 `ScanPathsForPrimaryAssets(...)`（**绕开 ini**，见裁决 E4） |
| `Source/TcsAssetButler/Public/Asset/TcsAssetLoadPolicy.h` | **加载三策略**枚举：`PreloadAll` / `PreloadSelected` / `OnDemand` |
| `Source/TcsAssetButler/Public/Asset/TcsAssetLoadHandle.h` + `TcsAssetLoader.h` + `Private/.cpp` | **加载机制**：异步默认（`LoadPrimaryAssets*` 返 `FStreamableHandle`）+ 同步逃生口（`FStreamableManager::LoadSynchronous`）+ **加载防重**（同 `FPrimaryAssetId` 只加载一次 + 句柄计数） |
| `Source/TcsAssetButler/Public/Asset/TcsAssetButlerSettings.h` | **编辑器期规则**（`UTcsAssetButlerSettings : UDeveloperSettings`，`GetCategoryName` 返回 `"Tcs"`）—— 承载路径排除 / 策略默认值等**宿主可改**项；**红线**：值不得影响模拟语义 |

### 修改（`TcsEffect` · 收下两个 Def 资产类）

| 文件 | 改动 |
|---|---|
| `Source/TcsEffect/TcsEffect.Build.cs` | 补 `AssetRegistry`（若新位置需要按类发现） |
| `Source/TcsEffect/Public/Chain/TcsEffectChainDef.h` + `Private/.cpp` | 自 `TcsIntegration` **搬入**（改导出宏 `TCSINTEGRATION_API` → `TCSEFFECT_API`） |
| `Source/TcsEffect/Public/Trigger/TcsEffectTriggerDefAsset.h` + `Private/.cpp` | 同上 |

### 修改（接线层 · 收下两个实体类型）

| 文件 | 改动 |
|---|---|
| `Source/TcsIntegration/Public/Entity/TcsCombatEntityComponent.h` + `Private/.cpp` | **留本模块**（它就是"把 Actor 生死翻译成注册表条目"的接线，与加载无关；裁定⑤ 的"接线层"= 此处） |
| `Source/TcsIntegration/Public/Entity/TcsPieEntityQuery.h` + `Private/.cpp` | **留本模块**（PIE 专用查询实现；零资产引用） |

> **★ 裁定⑤ 的落点澄清（MUST 读）**：裁决原文写"→ **实体接线层**"。**本计划明确该"接线层" = `TcsIntegration` 自身** —— 因为这两个类型**本就在 TcsIntegration**、且其消费面（`TcsIntegration` 18/15 处 + 宿主）**都在接线语境**。⇒ **裁定⑤ 对这两个类型的实际动作 = "不搬，但明确归属声明"**（与两个 Def 类的"真搬走"不同）。**MUST NOT 读成"要新建一个接线模块"**。

### 修改（`TcsIntegration` · 被掏空后的形态）

| 文件 | 改动 |
|---|---|
| `Source/TcsIntegration/TcsIntegration.Build.cs` | 若定义库瘦身（**该裁定待定**——加载层与缓存层的边界见裁决④）则调依赖 |
| `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` + `Private/*` | **定义缓存层**（认识业务类型，住上层）：保留 `Discover*` / `Resolve*` / 缓存 / 就绪状态机 / ARO；**`Discover*` 内部改走 AssetManager 官方路径**（若 §6 T1 实测支持） |
| `SPEC-06-integration` §1 依赖行 | **重写**：去掉"全插件唯一依赖 GameplayStateTree 的模块"（裁定⑦：StateTree 归 `TcsStateTree`）；并订正该行与 as-built 的漂移（实测 `Source/` 对 StateTree **零 include、零 Build.cs 依赖**） |

### 修改（`openspec` 与设计文档）

| 文件 | 改动 |
|---|---|
| `openspec/specs/`（约 11 个能力） | MODIFIED delta（Task 5） |
| `openspec/specs/plugin-descriptor/spec.md` + `openspec/project.md` | **模块清单 9 → 10**（新增 `TcsBuff`；★ 方案 A 下 Task 4 先行，后续 Task 8 再推 10 → 11）——**计数须同批回改** |
| `Documents/combat-system-design/decisions/dec-00-constitution.md` | §9 模块表：**新增 `TcsBuff`**、`TcsState` 的职责改述为"共享层"、**去掉 `TcsEffect` 依赖边**、模块总数 **11 → 12**（Task 4 先做；随后 Task 8 再推 **12 → 13**。该文档 `ACTIVE`，可改）。**★ 两处基线不同，MUST NOT 混算**（实测）：`.uplugin` = **9** 个**已建**模块；`DEC-00` §9 表 = **11** 行（多出 `TcsCue` / `TcsEditor` 两个**未建**模块）。⇒ **两组数字各自独立递增** |
| `Documents/combat-system-design/spec/03-module-states.md` | 三层拆分改述（Task 4） |

---

## Task 1 — 基底抽形 + 共享机制合并（落 `TcsState`）

**Files:**
- Create: `Source/TcsState/Public/Runtime/TcsInstanceHandleOps.h` / `TcsEntityRuntimeBucket.h` / `TcsEntityRuntimeRegistry.h` / `TcsEntityAdmission.h`
  - **★ `TcsInstanceHandleOps.h`** = 判据 / 相等比较 / 哈希 / 池位模式转换的**语义单点**（非反射公共件）；**MUST NOT 新建统一句柄类型**（三个句柄保持独立，见 Step 2 与裁决 §8）
- **★ 不再创建 `TcsEntityRuntime.h`**（基底 struct 的创建移入 **Task 2 Step 1**——见 Step 1 的说明）
- Modify: `Source/TcsState/TcsState.Build.cs`（**预期零改动**——机制只用 `TcsCore` 已有类型）
- Modify: `Source/TcsState/Public/State/TcsStateHandle.h`（**保留类型**，判据改委托公共件 + 注释改述）；`TcsStateRegistry.h`（**改为薄别名或直接删除**，由新机制取代）；`Private/State/TcsStateRegistry.cpp`
- Modify: `Source/TcsSkill/Public/Skill/TcsCastRunHandle.h` / `TcsSkillEntryHandle.h`（**保留类型**，判据改委托 + 注释改述）/ `TcsSkillRegistry.h` + `Private/Skill/TcsSkillRegistry_Bucket.cpp`（桶同上）
- Modify: 两域门面（`TcsStateSubsystem` / `TcsSkillSubsystem`）的池/注册表/ARO/重入守卫生效面
- Test（本项目无自动化测试，改用）: 宿主装置 `Source/TcsDev/` 的既有命令读数

**Interfaces:**
- Produces:
  - `FTcsInstanceHandleOps`——`IsValid(Index, Generation)` / 位模式转换的**唯一语义点**（**不新增句柄类型**；三个既有句柄各自委托它）
  - `TTcsEntityRuntimeBucket<TPayload>`——`AllocateSlot()` / `ReleaseSlot(uint32)` / `Find(Handle)` / `ForEach` / `ForEachForGC` / `Num` / `Reset`
  - `FTcsEntityRuntimeRegistry<TPayload>`——`FindOrAddBucket(Entity)` / `FindBucket(Entity)` / `ForEachBucket`
  - `FTcsEntityAdmission`——`virtual bool IsSameGroup(Request, Existing) const` / `virtual bool ShouldAccept(...)` / `virtual int32 ResolveStacks(...)`（**合并**现 `FTcsStateStackDecisionFragment` 与技能 Instancing 分流）
- Consumes: `Source/TcsCore/Public/Pool/TcsInstancePool.h:TTcsInstancePool`（在 Core，载体即它）；`Handle/TcsCombatEntityHandle.h`、`Handle/TcsSourceHandle.h`、`Clock/TcsExpiryHeap.h:FTcsTimeEntryHandle`（均已可用）

- [x] **Step 1：★ 定基底字段集，但**只记录为设计决定**（不创建 struct）**

> **为什么本 Step 不建 `FTcsEntityRuntime`**（★ 2026-10-13 用户拍板方案 A，订正计划原文的一处自相矛盾）：原 Files 列了"创建 `TcsEntityRuntime.h`"，而本 Task 的**边界**又写"**不含 payload 拆分**——两个域暂时仍各持自己的完整 struct"。⇒ 若在此建基底，**它当场没有任何消费者**（两域都不采用），而 Task 2/3 才用得上 —— 违反本仓「**零消费者不预建**」，且留下一段**跨任务死代码**。
> ⇒ **现行切分**：**Task 1 = 纯机制**（句柄 / 桶 / 注册表 / ARO / 守卫，全部**模板化在现有两个完整 struct 上**，当场被两域采用）；**`FTcsEntityRuntime` 的创建移入 Task 2 Step 1**（与 payload 拆分同批，届时它立即有消费者）。

**本 Step 的产出 = 一份写进头注释与计划的设计决定**（字段表见下），供 Task 2/3 落地时逐字采用。**MUST NOT** 建空结构体占位。

`FTcsEntityRuntime`（**Task 2 才创建**）字段（逐条给理由，MUST 写进头注释）：

| 字段 | 类型 | 为什么在基底 |
|---|---|---|
| `DefTag` | `FGameplayTag` | 两域皆有定义身份（技能侧经 `EntryHandle` 可达，但基底直持可免一跳） |
| `Entity` | `FTcsCombatEntityHandle` | **两侧现状逐字相同**，注释都自述"必须自持，O(1) 定位" |
| `Level` | `int32` | **两侧现状逐字相同**（激活/施加瞬间冻结的生效等级） |
| `ParamSnapshot` | `FTcsParamSnapshot` | **两侧现状逐字相同** |
| `Source` | `FTcsSourceHandle` | 两侧各有一枚"来源/运行来源" |
| `ExpiryEntry` | `FTcsTimeEntryHandle` | 两侧各有一枚"到期取消锚"（同一类型、同一角色） |
| `Phase` | `ETcsEntityRuntimePhase` | 合并自 `EStatePhase`（**MUST NOT** 与时段游标混，见 §0.3 ③） |

**MUST NOT 进基底**：`Stacks` / `DurationRemaining` / `PeriodRemaining` / `PeriodEntry`（Buff payload）/ `PhaseIndex` / `ChainRunHandle` 等（Cast payload）。

- [x] **Step 2：统一句柄判据与转换（★ **MUST NOT 合并类型**，见裁决 §8）**

三个句柄（`FTcsStateHandle` / `FTcsCastRunHandle` / `FTcsSkillEntryHandle`）**保持为三个各自反射的 `USTRUCT(BlueprintType)`**，但**判据 / 相等比较 / 哈希 / 池位模式转换的语义 MUST 收敛到一处非反射公共件**（如 `FTcsInstanceHandleOps` 静态函数族），三个薄型各自委托它。

**MUST NOT 合并为一个类型**，判据三条：① 合并会把"把状态句柄传给施法运行 API"从**编译期失败**降为**运行期查不到**（`instance-handle-pool` 为此立了**两条**判据——"不同池的句柄类型不可互换" + Scenario「句柄类型隔离」→ 编译失败）；② 一个类型的 `GetInner()` 只能返回**一个** `TTcsInstanceHandle<Tag>`，无法服务三个标签 ⇒ 单点转换必然退化为调用点各自 `static_cast`（违反"唯一转换点"）；③ 三个**反射身份本就必须各自声明**，实测三型代码各约 15 行（其余皆为注释）⇒ 真正的重复是**逻辑**，收敛逻辑即达成 DRY。

**★ 已否决的绕过方案（MUST 记录以免重试）**：~~"一个宏生成三个类型"~~ —— 【引擎源码】实测：引擎 `Runtime/` 内**宏展开出 `USTRUCT(` 的用例零命中**；引擎自带的四个 wrapper-struct 宏**全部只生成普通 C++ struct**（无 `USTRUCT`、无 `GENERATED_BODY`）；且 `GENERATED_BODY` 按 `__LINE__` 拼符号名，与 UHT 的按行生成**是否对齐本机无法确认** ⇒ 依既有纪律（同类"本机不可核实"的依赖一律去掉，先例 `FGameplayTagQuery`）**不建在未核实行为上**。

**判据统一为最严**：`IsValid()` = `Index >= 0 && Generation > 0`。**被统一的真不一致**：`FTcsCastRunHandle` 现判 `Index != -1`（**不看 `Generation`**）。**行为中性论证**：对良构句柄（`Allocate` 产物 `Generation >= 1`；默认构造产物 `Index = -1`）**两套判据结论逐例相同**，仅"手工构造 `Index >= 0` 且 `Generation == 0`"（从不出现的形态）由"有效"变"无效"。**连带**：`FTcsCastRunHandle::IsValid` 的既有注释自述"**本结构只判『是否曾被赋值』**"——**MUST 改述**并写明为何现在连 `Generation` 一起判。

- [x] **Step 3：桶 + 注册表（载体 = `TTcsInstancePool`）**

`TTcsEntityRuntimeBucket<TPayload>` 内部持 `TTcsInstancePool<FTcsEntityRuntimeSlot<TPayload>, TTag>`。**为什么用池**（用户 2026-10-13 拍板）：现状三种变体（状态手写桶、技能池、技能账本手写桶），其中状态手写桶的自述理由是"一个池 = 一个句柄空间"，而基底化后**该理由不再成立**（基底就是那个句柄空间）。统一后**免费获得**代际簿记与 GC 遍历口（`ForEachForGC`）。

`FTcsEntityRuntimeRegistry<TPayload>` 照现状两份注册表的形状（`TMap<Handle, TUniquePtr<Bucket>>`——**`TUniquePtr` 间接层必须保留**，理由现两份注释都写了：`TMap` 扩容会搬移元素，按值存放会使桶地址失效）。

- [x] **Step 4：重申请决策接口（消除 §0.3 ① 的同型重复）**

把 `FTcsStateStackDecisionFragment` 的三方法与技能 `ECastInstancing` 四路分流**收敛为一个接口**：

```
virtual bool IsSameGroup(Request, Existing) const;   // 默认：同实体 + 同 DefTag
virtual bool ShouldAccept(Request, Existing) const;  // 默认：恒接受
virtual int32 ResolveStacks(Request, Existing) const;// 默认：同来源不变、异来源 +1
```

**裁决点（MUST 在实施时写明结论，MUST NOT 留白）**：
- **`STAT-7`（层模型是否重开）—— ★ 倾向已按 2026-10-13 用户口径重述**：**初稿写"倾向不改"，理由是"现模型有 13 项装置读数背书"——但该理由里混了成本考量**（改它要重取读数）。按新口径（成本不进判据）重新论证：
  - **纯设计理由**：现层模型（层数记在一条实例内、组内至多一条）与"基底 + 重申请决策接口"**不冲突** —— 决策接口化之后，两种模型都只是**不同的默认策略**。⇒ **没有设计上的强制理由要改**。
  - **反面**：`STAT-7` 记的是"跨定义共享层数组"这一**真实内容需求**尚未出现 ⇒ 按"**零消费者不预建**"（本仓既有纪律，与成本无关）**现在不该改**。
  - ⇒ **现行倾向：不改，判据 = 零消费者不预建**（**不是**"改起来贵"）。若该内容需求出现，属独立议题、独立开轮。
- **`STAT-6`（`PerStackValue` 是否顺手补）—— 同理重述**：判据 = **零消费者不预建**（"层数读口 + 按层参数源"是为一个无消费者的档位造新机制），**MUST NOT** 表述为"顺手补太麻烦"。
- 技能侧三个档的映射（`InstancePerExecution` → 恒不同组 / `InstancePerEntity`+`false` → `ShouldAccept` 拒 / `+true` → 替换）**MUST** 在本 Step 用一组默认策略类表达，MUST NOT 继续内联在 `TcsCastOps.cpp` 里。

- [x] **Step 5：快照类型原地保留**

`FTcsParamSnapshot` / `FTcsParamSnapshotEntry` **留在 `TcsState`**（★ 修正：**不再下移 Core**）。其头注释自述"它是**参数域**的概念，不是状态模块专属"——该自述**仍然成立**，但落点是**共享层 `TcsState`**（两域共用），不是 Core。**MUST** 逐字保留成员与三个方法（`TryGetNumericParam` / `Num` / `Reset`）；**MUST NOT** 改路径（**实测 6 个文件**引用该头；不改路径意味着本 Step 零改动面）。

- [x] **Step 6：GC 引用收集口（Review Focus ①）**

基底提供 `ForEachForGC`（纯读、无线程断言、判据与常规遍历逐字相同）。**验证**：MUST 复现既有阴性对照手法——**临时关掉 ARO 补引用，读数应出现失败**（先例 `EVID-2026-10-08-skill-cast-runtime`：ARO off ⇒ 24 PASS / 2 FAIL，`存活=否`）。**读数不失败 ⇒ 说明该遍历没被真正走到**，MUST 查明而非结案。

- [x] **Step 7：门面清理口（Review Focus ②）**

统一"世界反初始化确定性清空"的形状：MUST 覆盖**技能侧现状的 7 个 owner**（不能只照状态侧的 3 个）。**验证**：装置跑"建实例 → 反初始化 → 旧句柄应全部失配"。

- [x] **Step 8：重入广播守卫（Review Focus ④）**

合并 `InRemovalBroadcastHandles` / `InFinishBroadcastHandles` 为一份（句柄类型随之统一）。**验证**：装置须有"广播内递归移除被静默拒绝"的读数（先例：状态侧 `Removed` 递归移除静默拒绝）。

- [x] **Step 9：双配置编译 + 装置回归**

`-projectfiles` 刷新 + UBT Development Editor + Game Shipping **双配置**，两条日志 **error / warning 命中数 MUST 为 0**。装置：宿主既有命令（`Tcs.Test.Slice.Run` 等）读数**与重构前逐字一致**（本 Task 只搬机制、不改行为 ⇒ 这是硬判据）。

**验收信号**：双配置 0 error / 0 warning；宿主既有装置读数**逐字不变**（本 Task 是**纯搬迁**）；ARO 阴性对照可复现（关掉则失败）；旧句柄跨世界失配；**两域已改用统一句柄与桶**（新机制有真实消费者）。

**边界**：**不含** payload 拆分，**也不含 `FTcsEntityRuntime` 基底的创建**（★ 方案 A：移入 Task 2 Step 1）——本 Task 结束时两个域**仍各持自己的完整 struct**，只是**机制已共用**。

### Task 1 实施记录（2026-10-14，★ 收尾 MUST 读）

**状态 = 九个 Step 全部落地**，双配置编译 `Result: Succeeded`（Dev Editor + Game Shipping，**0 error / 0 warning**），无头装置回归 **10 条命令逐字不变**、**ARO 阴性对照已复现**。

**★ `STAT-7` / `STAT-6` 两个裁决点 = 已裁（不再是"倾向"）**：

- **`STAT-7`（层模型是否重开）→ 裁定：不改**。判据 = **零消费者不预建**（"跨定义共享层数组"这一真实内容需求尚未出现），**MUST NOT** 表述为"改起来贵"。
- **`STAT-6`（`PerStackValue` 是否顺手补）→ 裁定：不补**，同判据。
- ⇒ **连带效果**：因两者都取默认档，`FTcsStateStackDecisionFragment` **保留**（形状逐字保留、只把三方法契约上提并继承），`state-stacking-policies` 的 MODIFIED delta 已随本变更同批补出。

**★ 实施期三处偏离（均为修正错前提，MUST 读）**：

1. **给 `TTcsInstancePool` 加了第三模板参**（代际发号策略）——初稿的"载体 = 池"与状态侧"代际 MUST 进程唯一"**互斥**（池原先只能池内自增 ⇒ 各桶各从 1 起发 ⇒ 跨桶扫描**命中错实体**）。**默认档 = 池内自增 ⇒ 既有四个池使用者零行为差异**。**连带订正**：本 Task 的 Files 原写 `TcsCore` "预期零改动"——**该预期不成立**（现为加法式改动）。
2. **两域 ARO 改走 `ForEachForGC`**——状态桶换成池后，ARO 里走常规 `ForEach` 会触发池的 `ensure(IsInGameThread())`（技能侧 2026-10-08 踩过同一坑）。
3. **池的 `IsValid` 补奇偶校验**——代际相等不足以证明槽位在册（手工拼装的偶代际句柄会命中空闲槽）。

**★ 一处范围外的发现已入册**（台账 `MECH-1`）：槽位 + 代际 + 空闲栈在本仓实为**四份**而非提案所记的"三种变体"（第四份 = 扁平的 `FTcsTriggerRegistry`，**不在本轮两域 per-entity 合并面内、本轮未动**，归属 = 触发条件）。

**★ 绕开承诺的可核项（实测）**：`TcsDevSkillCastProbe_Phase.cpp` **仍 856 行且 git 无改动**；两个超限 Rig 文件（1129 / 1671 行）**均无改动**。

**★ 取证边界（如实，MUST NOT 读成全部已验）**：`State.*` / `Slice.*` 三条装置**本来就是 PIE-only**（无头下定义库 0 条 ⇒ `State.Run` 4/11、`State.Reject` 6/1、`Slice.Run` 3/3 是**既有边界**，先例 `EVID-2026-10-11-skill-layer-e2e` §5.2 同值）；本轮**未**在编辑器 PIE 下重跑这三条。

**★ Task 1 全部判据的取证收口（2026-10-14 补跑）**：**11 条读数 + 1 组阴性对照，全部与权威基线逐字相同**——
`SkillCast.Run` 26/0、`.Reject` 16/0、**`.Phase` 29/0 + `PhaseDeferred` 34/0**、`SkillParamChain.Run` 26/0、`.Reject` 9/0、
`SkillLayer.Run` 24/0（`[复现性摘要]` **逐字相同**）、`.Reject` 7/0、`CooldownCost.Run` 33/0（含跨帧尾段）、`.Reject` 7/0、
`State.Reject` 6/1、`Slice.Run` 3/3；**ARO 阴性对照**：关 `ForEachForGC` ⇒ 26/0 → **24/2**、`存活=否`，还原后回 26/0。

**★ 其中 3.7（重入守卫）已用行为级读数闭合**（先前的"归 PIE 复核面"结论**已撤回**）：
`SkillCast.Phase` 的 `P6 重入终结` 三项全 PASS（二次返回**假且静默**、`Interrupted` **1→1**、零 ensure）——
该检查的源码注释原话即"若本轮的重入守卫没落地，本项必失败（`Interrupted=2`）"
⇒ 它是"守卫合为一份实现"的**直接行为证据**。
**★ 一处基线口径订正（防误读）**：`EVID-2026-10-10-skill-cast-phases` 记的 `PhaseDeferred 35/0` 是**该提交之前**的形态
（彼时该项走 `Counter->Check`；LAC `810826e` 后改走 `ReportSkillCastProbe`、**不再计入 `Passed=`**）
⇒ **权威基线 = `810826e` 的 `29/0(+34/0)`**，实测与之**逐字相同**。

**⇒ Task 1 无剩余未验判据**（此前列表中的"3.7 归 PIE"是**当时的错误收窄**，已由本次补跑消除）。

---

## Task 2 — Buff payload 化

**Files:**
- Create: `Source/TcsState/Public/Runtime/TcsEntityRuntime.h`（**★ 基底 struct 在本文（Task 2）创建**——Task 1 只定字段表、不建类型，见 Task 1 Step 1）
- Create: `Source/TcsBuff/Public/State/TcsBuffPayload.h`
- Modify: `Source/TcsState/Private/State/TcsStateOps_*.cpp`（13 文件触达 `FTcsStateInstance`）
- Modify: `Source/TcsState/Public/State/TcsStateInstance.h` → 拆解

**Interfaces:**
- Consumes: Task 1 的 `FTcsEntityRuntime` / `FTcsEntityRuntimeHandle` / `TTcsEntityRuntimeBucket` / `FTcsParamSnapshot`
- Produces: `FTcsBuffPayload`——`int32 Stacks` / `double DurationRemaining` / `double PeriodRemaining` / `FTcsTimeEntryHandle PeriodEntry`

- [ ] **Step 1：定 Buff payload 字段集**
- [ ] **Step 2：拆 `FTcsStateInstance`**——基底字段迁基底、Buff 专属迁 payload；**MUST 逐字段核对**无遗漏（14 字段逐一有归属）。
- [ ] **Step 3：`STAT-6` 裁决点**——`PerStackValue` 是否顺手补。**判据 = 零消费者不预建**（不是"麻烦"）；写明结论 + 理由。
- [ ] **Step 4：到期回调的锚点复位与悬空防护（Review Focus ③）**——MUST 保留现状纪律：回调入口**第一条语句复位自己的锚点**；捕获**世界弱引用 + 句柄**（不捕获裸指针）。
- [ ] **Step 5：ARO 覆盖新增的 payload 容器（Review Focus ①）**——payload 若含对象引用，MUST 进 GC 遍历。
- [ ] **Step 6：双配置编译 + 装置回归**——**判据 = 重构前读数逐字不变**（本 Task 仍不改行为）。

**验收信号**：`TcsState` 侧全部既有装置读数（`Tcs.Test.Slice.Run` / `.Reject` / 状态层 e2e）逐字不变；`FTcsStateInstance` 已不存在于代码；14 字段全部有明确归属。

---

## Task 3 — Cast payload 化 + 门禁④ 改走共享决策

**Files:**
- Create: `Source/TcsSkill/Public/Skill/TcsCastPayload.h`
- Modify: `Source/TcsSkill/Private/Skill/TcsCastOps*.cpp`、`Source/TcsSkill/Public/TcsSkillSubsystem.h`
- Modify: `Source/TcsSkill/Public/Skill/TcsCastRun.h` → 拆解

**Interfaces:**
- Consumes: Task 1 的基底与 `FTcsEntityAdmission`
- Produces: `FTcsCastPayload`——与基底合并后构成技能运行态

- [ ] **Step 1：定 Cast payload 字段集**（6 个：`EntryHandle` / `PhaseIndex` / `ChainRunHandle` / `ActiveChainId` / `MainChainStart` / `MainChainStartPhaseTag`）
- [ ] **Step 2：拆 `FTcsCastRun`**（11 字段逐一定归属）
- [ ] **Step 3：门禁④ 改走共享决策接口**——删除 `TcsCastOps.cpp` 内联的 Instancing 分流，改挂 Task 1 的三个默认策略。**这是本重构的实质收益点**（裁决 §1.3 的重复在此消除）。
- [ ] **Step 4：时段推进的锚点复位与悬空防护（Review Focus ③）**——保留现状明文纪律（"`AdvancePhase` 第一条语句即复位自己的锚点"、"起链会搬移池 ⇒ `Run` 指针可能已失效，故日志读回写前的值"）。
- [ ] **Step 5：ARO（Review Focus ①）**
- [ ] **Step 6：双配置编译 + 技能侧装置回归**——`Tcs.Test.SkillLayer.Run`（基线 24/0）、`Tcs.Test.CooldownCost.Run`（18/0 + 21/0）、`.Reject`（7/0）。
- [ ] **Step 7：连带 `PLN-R5.5`**——按新基底改述其 Task 1 Step 7（"主体 = `FTcsCastRun`" → 基底句柄）与 Task 4（槽位竞争表主体）。

**验收信号**：技能侧全部既有读数逐字不变（含 Instancing 四态）；`FTcsCastRun` 已不存在于代码；`TcsCastOps.cpp` 内**零**内联 Instancing 分流。

---

## Task 4 — 拆出 `TcsBuff` 新模块（★ **不是改名**，见 §0.3.0）

**Files:**
- Create: `Source/TcsBuff/**`（模块壳 + 从 `TcsState` 搬入的 Buff 专属全体）
- Modify: `Source/TcsState/**`（只留共享层：Def 族 + 通用运行时机制）
- Modify: `Source/TcsState/TcsState.Build.cs`（**去掉 `TcsEffect`**）
- Modify: `TireflyCombatSystem.uplugin`（模块条目 **9 → 10**）
- Modify: `openspec/specs/plugin-descriptor/spec.md` + `openspec/project.md`（**模块清单 9 → 10**）
- Modify: `Documents/combat-system-design/decisions/dec-00-constitution.md`（§9 模块表）
- Modify: `Documents/combat-system-design/spec/03-module-states.md`
- Modify: 宿主装置 `Source/TcsDev/**`（include 路径改指）

**Interfaces:**
- Consumes: Task 2 的 payload 化结果；共享层（`TcsState`）的基底与机制
- Produces: 新模块 `TcsBuff`（`TCSBUFF_API` / `LogTcsBuff` / `UTcsBuffSubsystem` / `FTcsBuffOps`）；共享层 `TcsState` 保留原名与 `TCSSTATE_API`

- [ ] **Step 1：定拆分清单并机械核对（MUST 先做）**——按裁决 §6.1 的三层表逐文件分类：**留 `TcsState`** = 共用 Def 族 473 行（`TcsStateDefBase` / `TcsStateDef` / `TcsParamRow` / `TcsDescriptionEntry` / `TcsStateParamKeys`）+ 通用运行时机制 956 行（`TcsStateSnapshot` / `TcsStateParamTableReader` / 新基底与注册表）；**搬 `TcsBuff`** = BuffDef 三件 / 堆叠三件 / 时值周期 / 行为 Fragment 五件 / 修正器物化 / `TcsStepApplyState` / 等级源两件 / 等级读口 / 六枚事件 / `TcsStateSubsystem*` / `TcsStateOps_*`。**判据 = 该文件是否被 `TcsSkill` 引用**（实测引用面见裁决 §6.1）。
- [ ] **Step 2：模块壳 + 依赖声明**——`TcsBuff.Build.cs` 依赖 = `TcsCore` / `TcsNotation` / `TcsAttribute` / **`TcsEffect`** / `TcsState`。**UHT 两条硬门槛**：① 新模块的头文件名 MUST 全项目唯一；② 反射类型去前缀后 MUST NOT 与既有同名 ⇒ **搬入的文件若沿用原名会与 `TcsState` 遗留者撞名**，故 `TcsStateSubsystem`→`TcsBuffSubsystem`、`TcsStateOps`→`TcsBuffOps`、`TcsStateInstance`→（已被基底+payload 取代）。
- [ ] **Step 3：★ `TcsStateParam` tag 根 MUST NOT 改名（Review Focus ⑤）**——它是**宿主词表根名**，与模块名无关；改名会让**已落内容资产全部找不到键**。**同批复核**：`gameplay-tag-governance` 根表行数与 `## Purpose` 数字**不变**（本轮不开根、不删根）。
- [ ] **Step 4：共享层与 Buff 层的登记口边界**——`RegisterStateDef` / `GetRegisteredStateDef` 收**两域共用**的 Def 基类 ⇒ **留共享层**；`TcsBuffSubsystem` 只补 Buff 专属面（施加 / 移除 / 堆叠 / 时值）。**判据 = 是否被 `TcsSkill` 用到**（`UTcsStateDef` 1 处、`FTcsStateDefBase` 3 处 ⇒ 二者留共享层）。
- [ ] **Step 5：类型前缀的取舍**——`FTcsStateDefBase` / `UTcsStateDef` **保持原名**（判据：它们是**两域共用的定义基类**，名字里的 "State" 在此读作"**状态定义**"，不是模块名；改名会传导到 `TcsSkill` 全部 Def 文件而收益仅为字面）。**MUST 写明本结论 + 理由**。
- [ ] **Step 6：`ApplyState` 步骤名评估**——它是**链步骤注册名**，随 Buff 专属层搬到 `TcsBuff`，**注册名本身 MUST NOT 改**（已落内容资产引用它）。**MUST 写明结论**。
- [ ] **Step 7：`TcsState.Build.cs` 去掉 `TcsEffect`**（裁定⑫）——**这是本轮实质架构收益之一**。复核：`TcsState` 侧 grep `TcsEffect` / `Chain/` / `Trigger/` **零命中**。
- [ ] **Step 8：`DEC-00` 模块表 + `SPEC-02-states` 三层改述**（**`DEC-00` §9 表 11 → 12**，`TcsState` 职责改述为"共享层"；★ 该序列的起点是 **11**（含未建的 `TcsCue` / `TcsEditor`），**方案 A 下 Task 4 先行** ⇒ 本 Step 是 **11 → 12**，随后 Task 8 再推 **12 → 13**。**MUST NOT 与 `.uplugin` 的 9 → 10 混算**）。
- [ ] **Step 9：双配置编译 + 装置回归**——宿主装置 include 路径改指；**判据 = 读数逐字不变**。

**验收信号**：`Source/TcsBuff/` 存在且编译通过；`TcsState` 侧 grep `TcsEffect` **零命中**（依赖边已去掉）；`TcsStateParam` tag 根**未改**且根表计数不变；模块清单计数 **9 → 10** 三处（`uplugin` / `plugin-descriptor` / `project.md`）一致；`DEC-00` §9 表 **11 → 12**；双配置 0/0；装置读数不变。

---

## Task 5 — 规格与能力面回写

**Files:**
- Modify: `openspec/specs/` 约 11 个能力（见下方清单）
- Modify: `openspec/specs/instance-handle-pool/spec.md`（若被本轮改变）

**Interfaces:** Consumes Task 1~4 的最终形状；Produces 与实现一致的生效规格。

- [ ] **Step 1：按 `DEC-…-state-module-positioning` §附录 E19 的清单逐个起草 delta**，其中**最重的三份**须逐条处理：
  - `skill-cast-runtime`——其需求**逐条写死了** `FTcsCastRun` 非反射 + **必须经 `TTcsInstancePool`** + **"MUST NOT 新建池实现"** + **8 个必具字段**。⇒ 四句全部需要改述（新形状 = 基底 + payload，池经 Core 的统一桶）。
  - `state-instance-lifecycle`（7 req / 40 scenario，状态侧最大面）
  - `state-stacking-policies`——其 `## Purpose` 与《组键解析》需求内的"**组内至多一条实例**"文本 MUST 与 Task 1 Step 4 的裁决结论一致（若裁决不改层模型，则此文本**不变**，但需注明"决策已上提为基底接口"）。
- [ ] **Step 2：`gameplay-tag-governance` 的交涉**——其 `TcsStateParam` 根的依据句把 `FStateInstance` / `FCastRun` 并列为"两个运行身份"；基底化后该句 MUST 改述（**根名不变**，但"两个运行身份"的论证基础变了）。
- [ ] **Step 3：`effect-trigger` / `effect-interpreter`**——两处硬编码 `FTcsStateInstance::CascadeAnchor` 作撤销锚 ⇒ MUST 改指新基底（**`CascadeAnchor` 的归属见 Task 2 Step 2 的裁决**；若它迁入基底，两处规格随之改述）。
- [ ] **Step 4：`R6.5-g` 的规格面连带**（`PLN-R6` 已就地标注）。
- [ ] **Step 5：门禁**——`openspec validate --all --strict --no-interactive` 全绿；**归档时 MODIFIED 需求标题 MUST 与主规格逐字一致**（先例：改标题会被拒为 `omits scenario(s)`）。
- [ ] **Step 6：能力计数回写**——若新增能力（如基底相关）则同步 `INDEX` §3 计数与 `openspec/project.md`；若未新增则**逐处复算确认**（MUST NOT 只声称不变）。

**验收信号**：`validate --all --strict` 全绿；**逐能力零丢失**（比对各能力的 req / scenario 计数与重构前，只应有本 Task 有意增删的差）；`changes/` 归档后归零。

---

## Task 6 — 证据重取

**Files:**
- Create/Modify: `Documents/combat-system-design/evidence/2026-10-14-*.md`（新证据档，逐个覆盖作废读数）

**Interfaces:** Consumes Task 5 的生效规格；Produces 与新形状一致的可复核读数。

- [ ] **Step 1：列出作废清单（≥11 篇）并逐篇给"重取什么"**：

| 作废证据 | 原读数 | 重取要点 |
|---|---|---|
| `EVID-2026-10-08-skill-cast-runtime` | 26/0 + 10/0（含 **ARO 阴性对照**） | **阴性对照 MUST 重做**（关 ARO ⇒ 应失败） |
| `EVID-2026-10-08-skill-registry` | 27/0 → 45/0 | 账本桶改 Core 统一桶后重取 |
| `EVID-2026-10-10-skill-cast-phases` | 26/0，在册 run=8 | 时段游标移入 payload 后重取 |
| `EVID-2026-10-11-skill-layer-e2e` | 24/0，两轮 PIE 逐字相同 | 端到端重取（**两轮 PIE 世界逐字相同**这条判据保留） |
| `EVID-2026-10-11-r65-cooldown-cost` | 18/0 + 21/0 + 7/0 | 冷却自有第 4 个槽位消费者 ⇒ 一并复核 |
| `EVID-2026-10-04-state-instance-lifecycle` | 26/0 + 7b PASS + 7/7 | 状态实例改基底后重取 |
| `EVID-2026-10-04-state-param-snapshot-and-level-sources` | 39/0 | 快照留共享层后重取（等级源搬入 `TcsBuff`） |
| `EVID-2026-10-05-state-stacking-policies` | 53/0 + 60/0 + 11/0（13 检查 22a–22m） | **决策上提为基底接口后重取**（含 22m 恒不同组） |
| `EVID-2026-10-05-state-behavior-fragments` | 65/0 + 72/0 + 11/0 | 同上 |
| `EVID-2026-10-05-state-layer-pie` | 15/0 + 7/0 | PIE 面重取 |
| `EVID-2026-10-05-state-chain-primitives` | 含 `const_cast<FTcsStateInstance*>` 先例 | 类型改为基础+payload 后重取 |
- [ ] **Step 2：双配置编译证据**（Development Editor + Game Shipping，两日志 0 error / 0 warning）。
- [ ] **Step 3：PIE 面取证**——MUST 保留"**两轮独立 PIE 世界逐字相同**"这一既有判据（它是本仓复现性纪律）。
- [ ] **Step 4：真实性自检**——每个数字 MUST 在落笔前**当场重新测量**（先例教训：产物类读数随每次重建漂移，凭记忆填的数字会过期）。
- [ ] **Step 5：失败现场分开命名**——若重取过程中出现坏态，日志 MUST 用 `-RED-` / `-GREEN-` 或轮次后缀命名，**MUST NOT 复用文件名**（后写的会覆盖失败现场）。

**验收信号**：≥11 篇新证据齐备；ARO 阴性对照可复现；双配置 0/0；两轮 PIE 逐字相同。

---

## Task 7 — 收束

- [ ] **Step 1：设计文档回写**——`SPEC-02-states`（**三层拆分 + `TcsBuff` 拆出 + payload 化 + 层模型注记**）、`SPEC-04-skill`（同款）、`DEC-00` 模块表**复核**（★ **本 Step 只复核、不再递增**：两组计数已由 **Task 8**（`.uplugin` 9→10 / `DEC-00` 11→12）与 **Task 4**（`.uplugin` 10→11 / `DEC-00` 12→13）各自改到位 ⇒ 本 Step MUST 逐处**复算确认**，MUST NOT 再改一次）。
- [ ] **Step 2：台账同步**——`STAT-7` / `STAT-6`（两个裁决点的结论就地登记）；若产生新落空面则入册并同步三处计数（身份块 / `INDEX` §4.5 / `INDEX` §2 行）+ 机械复核"表行数 = 唯一 id 数"。
- [ ] **Step 3：计划面同步**——`PLN-R5.5`（`e1`/`e4` 重写状态）、`PLN-R6`（`R6.5-g` 依赖）；`PLN-R5`《R5.5 批次表》补追注。
- [ ] **Step 4：`INDEX` / `GLOSSARY` 同步**——`PLN-REFACTOR-1` 入表；`GLOSSARY` §5.1 映射表新增行；**目录树计数逐处复算**。
- [ ] **Step 5：两册日志**——`LOG-DECISIONS` 补本轮（架构裁决 + 实施）；`LOG-IMPLEMENTATION` 补实施记录；`最后更新` 同批改。
- [ ] **Step 6：三方向扫描**（依 `MEM-20260930-05` / `MEM-20261004-16` / `MEM-20261013-03`）：
  - **符号方向**：被改/删符号（`FTcsStateInstance` / `FTcsCastRun` / `FTcsStateHandle` / `TCSSTATE_API` / `LogTcsState` 等）全库 grep，每处显式归类"引用 / 理由 / 历史留痕"；
  - **计数方向**：所有"共 N 个根 / N 条条目 / N 个能力 / N 篇文档"grep 全集逐点对齐；
  - **被取代的结论方向**：用**否定式** grep（`未定稿` / `零消费者` / `未开工` / `已验证` / `不做 Instancing`），按产物时间戳分"写下即为假"与"当时为真"两类处置。
- [ ] **Step 7：门禁**——`openspec validate --all --strict --no-interactive` 全绿 + `changes/` 零活动提案 + 两处 `## Purpose` 手工补正（若新增能力）。
- [ ] **Step 8：检查点卡**——按 `harness-retro` 门控刷新（**不自动写卡**）。

**验收信号**：门禁全绿；台账计数 = 行数；四处文档一致；三方向扫描**每处命中都被显式归类**（完成标志不是"grep 零命中"）。

---

## Task 8 — `TcsAssetButler` 落地 + 四处搬迁（★ **阶段 3**，见 §0.5.1）

**Files:**
- Create: `Source/TcsAssetButler/**`（模块壳 + 日志通道 + `FTcsPrimaryAssetEntry` + `FTcsPrimaryAssetRegistry` + 自注册宏 + `UTcsAssetManager` + 三策略 + `FTcsAssetLoader` / `FTcsAssetLoadHandle` + `UTcsAssetButlerSettings`）
- Modify: `Source/TcsEffect/**`（收下两个 Def 资产类）+ `TcsEffect.Build.cs`
- Modify: `Source/TcsIntegration/**`（两 Def 类搬出、两接线类型留原地并声明归属、定义库改走 AssetManager 路径）
- Modify: `TireflyCombatSystem.uplugin`（模块条目 **10 → 11**，★ 方案 A：Task 4 已把 9 → 10）
- Modify: `Config/DefaultEngine.ini`（**宿主仓**：`AssetManagerClassName=/Script/TcsAssetButler.TcsAssetManager`）
- Modify: `openspec/specs/plugin-descriptor/spec.md` + `openspec/project.md`（模块清单 **10 → 11**，★ 方案 A：Task 4 已把 9 → 10）
- Modify: `Documents/combat-system-design/decisions/dec-00-constitution.md`（§9 模块表 + 依赖链）
- Modify: `Documents/combat-system-design/spec/06-module-integration.md`（§1 依赖行重写）
- Modify: 宿主装置 `Source/TcsDev/**`（include 路径改指 + 资产重定向验证）

**Interfaces:**
- Produces:
  - `FTcsPrimaryAssetEntry`——`FPrimaryAssetType Type` / `UClass* BaseClass` / `TArray<FString> ScanPaths` / `bool bHasBlueprintClasses` / `ETcsAssetLoadPolicy Policy` / `bool bIsEditorOnly`（**零业务类型**）
  - `FTcsPrimaryAssetRegistry`——`Register(const FTcsPrimaryAssetEntry&)` / `Unregister(FPrimaryAssetType)` / `ForEach`；**同类型重复 MUST 报错**
  - `UE_DEFINE_PRIMARY_ASSET_TYPE(...)`——各域在其 `.cpp` 内自注册
  - `UTcsAssetManager : UAssetManager`——覆写 `StartInitialLoading()`
  - `ETcsAssetLoadPolicy{ PreloadAll, PreloadSelected, OnDemand }`
  - `FTcsAssetLoader`——`LoadAsync(FPrimaryAssetId)` / `LoadSync(FPrimaryAssetId)` / 防重 + 句柄计数
- Consumes: 引擎 `UAssetManager` / `FStreamableManager` / `UAssetManagerSettings`（本机源码可读：`E:\UnrealEngine\UE_5.8`）；各域现有 Def 资产类

- [ ] **Step 1：★ 先做 §6 的 T1/T3 两项实测（MUST NOT 跳过）**——① 建一个最小 `UTcsAssetManager` 覆写 `StartInitialLoading()`：`Super::` 后补调 `ScanPathsForPrimaryAssets(...)`，**观察是否错过 bulk scan 批次 / 是否延误 `OnCompletedInitialScan`**（裁决 E3 的时序疑点）；② 确认 `AssetManagerClassName` 指向的类能被解析（**加载不到是硬 `Fatal`**）⇒ 定 `TcsAssetButler` 的 `LoadingPhase` 取值。**两项结论 MUST 写进本计划注记**，MUST NOT 带疑问继续。
- [ ] **Step 2：模块壳 + 依赖声明**——`TcsAssetButler.Build.cs` **MUST 零 Tcs 业务依赖**（只用 `Core`/`CoreUObject`/`Engine`/`GameplayTags`/`AssetRegistry`）；日志通道照 D0-6 v2（`Public/TcsAssetButlerLogChannel.h` + `Private/.cpp`，`LogTcsAssetButler`）。
- [ ] **Step 3：注册项与注册表（类型擦除）**——`FTcsPrimaryAssetEntry` / `FTcsPrimaryAssetRegistry`；**同类型重复 MUST 报错**（对齐引擎语义，**不自建第二套唯一性判据**——裁决 E5）。
- [ ] **Step 4：自注册宏**——`UE_DEFINE_PRIMARY_ASSET_TYPE(...)`，形态**照既有四枚** `UE_DEFINE_EFFECT_STEP_EXECUTOR` 一族（静态注册器 + 构造期登记）。**验收**：全模块 grep 确认 `TcsAssetButler` 的 `Public/` 头**零业务 Def include**（Review Focus ⑧）。
- [ ] **Step 5：`UTcsAssetManager`**——覆写 `StartInitialLoading()`：`Super::` → `ForEach` 注册表 → `ScanPathsForPrimaryAssets(Entry.Type, Entry.ScanPaths, Entry.BaseClass, Entry.bHasBlueprintClasses, Entry.bIsEditorOnly)`。**MUST NOT 用 ini 提供类型清单**（裁决 §0 采用代码注册：类型名走 `StaticClass()`，**零字符串面**，规避"改名漏改 ini"陷阱）。
- [ ] **Step 6：加载三策略 + 异步 + 同步逃生口 + 防重**——`ETcsAssetLoadPolicy` + `FTcsAssetLoader`：异步走 `LoadPrimaryAssets*`（持 `FStreamableHandle`）、同步走 `FStreamableManager::LoadSynchronous`、**防重 = 同 `FPrimaryAssetId` 只加载一次 + 句柄计数**。
- [ ] **Step 7：四处搬迁（★ 含资产重定向，Review Focus ⑥）**——
  - **搬** `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` → `TcsEffect`（改导出宏；`TcsEffect.Build.cs` 补 `AssetRegistry` 若需要）；
  - **留** `UTcsCombatEntityComponent` / `UTcsPieEntityQuery` 于 `TcsIntegration`，**并就地声明归属**（裁定⑤ 的"接线层"落点澄清，见「文件结构」节的注）；
  - **配重定向**：`PrimaryAssetTypeRedirects` / `AssetPathRedirects`（裁决 E7）**或** `CoreRedirects` —— **先做 §6 T2 实测选定**；**14 个已落资产 MUST 逐个验证可打开**（`DA_Chain_E2E_*` / `DA_Check_*` / `DA_SliceChain` / `DA_FormulaChain` 一族）。
- [ ] **Step 8：定义库改走官方路径 + 模块表回写**——`Discover*` 视 T1 结论决定是否改走 `UAssetManager` 官方发现（`GetPrimaryAssetIdList` / `GetPrimaryAssetPath`）；`DEC-00` §9 模块表 + `plugin-descriptor` + `openspec/project.md` **三处计数同批改 10 → 11**（★ 方案 A：Task 4 已把 `.uplugin` 推到 10、`DEC-00` 推到 12；**本 Step 只动 `.uplugin` 序列 → 11 与 `DEC-00` → 13**）；`SPEC-06-integration` §1 依赖行**重写**（去 StateTree 声称，裁定⑦）。
- [ ] **Step 9：双配置编译 + 装置回归**——宿主装置 include 路径改指；**判据 = 既有读数逐字不变**（本 Task 对运行态是**纯搬迁**）。

**验收信号**：`Source/TcsAssetButler/` 存在且双配置 0/0；**该模块 `Public/` 头零业务 Def include**（`grep` 复核）；`AssetManagerClassName` 生效且 Def 类型被扫到（就绪日志出计数）；**14 个已落资产逐个可打开**（重定向生效）；`TcsEffectChainDef` 的类路径已变更为 `/Script/TcsEffect.*`；模块清单三处计数 **10 → 11** 一致（`DEC-00` §9 表 **12 → 13**）；装置读数不变。

---

## 依赖与关键路径

```
Task 0（两份裁决，✅ 已完成：state-module-positioning + asset-butler）
   │
   ├─→ Task 1（阶段 1：基底 + 7 轴机制合并，纯搬迁）        ← 先做发起缘由
   │      │
   │      ├─→ Task 2（Buff payload 化）
   │      │      └─→ Task 4（阶段 2：拆出 TcsBuff）────────────┐
   │      └─→ Task 3（Cast payload 化 + 门禁④ 改共享决策）────┘   ← "技能运行态归入 State"在此完成
   │                                                            │
   │                                                     Task 8（阶段 3：TcsAssetButler + 四处搬迁）
   │                                                            │
   └────────────────────────────────────────────────────────────┴─→ Task 5（规格面）→ Task 6（证据重取）→ Task 7（收束）
```

**关键路径** = Task 1 → 2 → 4 → **8** → 5 → 6 → 7。**Task 2 与 Task 3 可并行**（payload 各自独立、共享基底已定）。
**顺序硬约束（实测 + 已拍板，MUST 遵守）**：
- **Task 1 MUST 先于 2/3**（基底未定则 payload 无处挂）；
- **Task 4 MUST 先于 Task 8**（✅ 方案 A：`UTcsBuffDefAsset` 届时已在 `TcsBuff` ⇒ **少 2 处资产重定向**：`DA_Check_BuffDef` / `DA_State_E2E`）；
- **Task 8 与 Task 1~4 之间无依赖交集**（实测：两线改的文件集不相交）⇒ 该顺序是**优化**而非硬依赖，但**MUST NOT 让两线交错改同一文件**（`TcsEffect` / `.uplugin` / 模块计数三处）；
- **Task 5 MUST 后于 3/4/8**（规格须反映**两条线**的最终形状，否则返工）；
- **Task 6 MUST 后于 5**（证据对规格）。

---

## 非目标（本轮不做）

- **不改层模型**（`STAT-7` 的"组内可多条"）——**判据 = 零消费者不预建**（该内容需求未出现），**MUST NOT** 表述为"改起来贵"；是否改由 Task 1 Step 4 显式裁决并写明理由；
- **不补 `PerStackValue`**（`STAT-6`）——零消费者；
- **不改 `TcsStateParam` tag 根名**（宿主词表，与模块名无关）；
- **不合并** `TcsBuff` 与 `TcsSkill` 的 Def 层（`FTcsSkillDefData : FTcsStateDefBase` 的继承关系**保持**）；
- **不动** `FTcsExpiryHeap` / `UTcsClockSubsystem`（两侧已共用，无需改）；
- **不做** `e1`/`e4` 的实施（本重构是它们的前置；重写落在 Task 3 之后的 `PLN-R5.5`）；
- **不做** R7 的 M6 集成层内容（实体状态机 / StateTree 接线 / DefLibrary 门禁）；
- **不做** `TcsStateTree`（裁定⑦：StateTree 将来**单独成模块**，本轮只登记归属口径，MUST NOT 顺手建）；
- **不做** 定义库的"注册表 + 缓存"是否**进一步上移/另立模块**（裁定④ 确立的是**两层**：加载层低、缓存层住现处；缓存层的最终归属留后续）；
- **不做** 编辑器期跨资产解析机制（裁定⑧：需求已被 `FGameplayTag` 消除，**明确不建**）；
- **不做**网络操作复制（`WAIT-3`）、Mass 适配。

---

## 风险与裁决点

| 风险 | 判断 | 处置 |
|---|---|---|
| ~~**净收益可能为负**（裁决 §5.2 标为高风险）~~ → **该判已作废**（★ 2026-10-13 用户口径） | — | **TCS 处设计开发阶段 ⇒ 一次性返工不是决策依据**。§0.2 的代价表只用于排期预期。**原判是成本视角，已不成立**；真正该盯的是"设计是否正确" |
| **在未想清的地基上继续施工** | **高** | 两个裁决点若导致基底粒度反复 ⇒ **停下把设计想清**（**正确性判据，非成本判据**） |
| 基底粒度抽错（太细 / 太粗） | 中 | 太细 ⇒ payload 仍需各自机制；太粗 ⇒ 基底含域语义（正是本次要修的病）。Step 1 的字段表逐条给理由，MUST NOT 凭直觉加字段 |
| Task 1 声称"纯搬迁"但实际改了行为 | **高** | 硬判据 = **宿主既有装置读数逐字不变**。任何读数变化 MUST 查明原因后再继续，MUST NOT 归为"等价改动" |
| 三种槽位变体合并时重开 `STAT-7` 争议 | 中 | Task 1 Step 4 的裁决点；**判据 = 零消费者不预建**（不是成本） |
| 拆模块时误改 `TcsStateParam` tag 根 | **高** | 改它 ⇒ **已落内容资产全部找不到键**；Task 4 Step 3 单列 + 验收含"根表计数不变" |
| 模块吐故时 UHT 撞名 | 中 | 新模块头文件名 MUST 全项目唯一、反射类型去前缀后 MUST NOT 同名；`TcsStateSubsystem`→`TcsBuffSubsystem` 等随之（Task 4 Step 2） |
| 拆模块后共享层与 Buff 层的登记口重复声明 | 中 | 判据 = **该口是否被 `TcsSkill` 用到**；`RegisterStateDef` / `GetRegisteredStateDef` 归共享层（Task 4 Step 4） |
| 证据重取时凭记忆填数字 | 中 | Task 6 Step 4：每个数字落笔前当场重测（先例教训：产物读数随重建漂移） |
| 已付费证据作废（含 ARO 阴性对照） | **高** | Task 6 Step 1 逐篇列出重取要点；阴性对照 MUST 重做 |
| **搬类型漏配重定向 ⇒ 资产失联** | **高** | 14 个已落资产硬引用 `/Script/TcsIntegration.*`，本仓 `CoreRedirects` 零配置；**Task 8 Step 7 单列 + Step 1 先测 T2 选定手段 + 逐个资产验开**（Review Focus ⑥） |
| **`TcsAssetButler` 的 `LoadingPhase` 排后 ⇒ 硬 `Fatal`** | **中** | 类加载不到不是静默回退；**Task 8 Step 1 先测 T3**（Review Focus ⑦） |
| **业务 Def 类型混进加载层 ⇒ 各域无法向下注册** | **高** | `TcsAssetButler` 一旦 include 业务头即住高、注册环重现；**Task 8 Step 4 验收 = `Public/` 头零业务 include**（Review Focus ⑧） |
| **两条线分两次改同一批文件 ⇒ 计数漂移** | **中** | §0.5 合并成一份计划 + 分阶段；模块计数**一次改到位**（Task 7 Step 1 / Task 8 Step 8 各改一次但**分属不同阶段**，MUST NOT 同轮重复改） |
| `StartInitialLoading` 覆写后补调扫描的时序 | **中** | 裁决 §6 T1 **未实测**；Task 8 Step 1 必做，**结论前 MUST NOT 继续** |

---

## 变更记录

- 2026-10-13 建立：`DEC-2026-10-13-state-module-positioning` 的执行形状。Task 1~7 展开为 Step；**§0.2 给出工作量预期 + ★ 按用户口径声明"工作量不进决策判据"**；**§0.3 登记两个裁决点**（`STAT-7` 层模型 / `STAT-6` 按层取值）与**四处已实证陷阱**。**本计划不含提交步骤**（用户级 SourceControl 纪律为最高优先级）。
- 2026-10-13 **同日修正**（用户评审时提问「`TcsState` 剩下多少、全部并入 `TcsBuff` 是否合适」驱动）：初稿的 Task 4 = "改名 `TcsState` → `TcsBuff`"**被实测推翻** ⇒ 现行 Task 4 = **拆出新模块 `TcsBuff`**；基底与运行时机制**留 `TcsState`**（**不再下移 `TcsCore`**，**`TcsCore` 本轮零改动**）。新增 **§0.3.0**（模块归属前提修正）+ 陷阱 ④（两条下移禁令合并）。**收益面新增一条**（去掉 `TcsEffect → TcsState`）。修正依据 = 裁决 §6。
- 2026-10-13 **第二笔：并入资产加载层线**（`DEC-2026-10-13-asset-butler`）——新增 **Task 8**（`TcsAssetButler` 落地 + 四处搬迁 + 定义库瘦身）、**§0.5**（两条线合并的理由）、**Review Focus ⑥⑦⑧**（重定向漏配 / 加载阶段 / 业务类型混入）、**文件结构**三个新节（`TcsAssetButler` 新建、`TcsEffect` 收两类、接线层落点澄清）、风险表四行、依赖图重画。**★ 同批澄清裁定⑤ 的落点**：两接线类型**留 `TcsIntegration`**（"接线层"即它自身），MUST NOT 读成"新建接线模块"。
- 2026-10-13 **同日第三笔：订正 §0.5 的排序依据（用户追问「Skill 流程归入 State 什么时候做」驱动）**——初稿把 **Task 8 排为"阶段 1、MUST 最先"**，理由是「`TcsEffect` 收下两个 Def 类后，线一的『去反向边』才有落点」。**该理由实测不成立**：`TcsState → TcsEffect` 边由 **`TcsState` 自己的 4 个文件**产生（全在 Task 4 搬走），与 Task 8 搬的 **`TcsIntegration`** 文件**无交集** ⇒ 属"按表面相邻推断因果"（同 `MEM-20261013-09` 根因）。**实测两条真实约束方向相反**：① Task 4 先行**少 2 处资产重定向**（`DA_Check_BuffDef` / `DA_State_E2E`）；② 本计划的**发起缘由**就是"技能运行态与状态运行态统一"（Task 1 + 3）⇒ 排在支线之后与动机相悖。**⇒ 用户拍板方案 A（运行态线先行）**，阶段标注 / 依赖图 / Task 8 标题同批改写为 **阶段 1 运行态 → 阶段 2 拆模块 → 阶段 3 加载层 → 阶段 4 收尾**；**关键路径 = Task 1 → 2 → 4 → 8 → 5 → 6 → 7**。
- **2026-10-14 补记（★ 日期留痕，MUST 读）**：本计划与两份 `DEC-2026-10-13-*`、Task 1 提案
  **实际创建于 2026-10-14 凌晨（机器日，实测新文件创建时间 01:23–02:57）**，而全部文档 ID / 文件名 /
  正文日期**一律沿用 2026-10-13（轮次日）**——**这是有意的，不是笔误**：本轮工作起于 10-13、跨零点收尾，
  改名会同时牵动文档 ID、文件名、`GLOSSARY` 映射表与 `INDEX` 计数（收益为零、漂移风险为正）。
  按仓内既有先例（`MEM-20261004-12`：归档日 10-06 vs 计划预写 10-05）**留痕而不改名**。
  **同一处如实登记一次流程偏差**：本轮起草全程**未跑过 `Get-Date`**，
  到 retro 才取到机器日 —— 即 `MEM-20261004-05`「文档日期先取系统时间、跨零点会话尤其危险」的**原样复犯**。
  ⇒ 处置 = **新增 §0.5.2 动作 B**（把该纪律写成计划里的固定动作与可核项），
  **不另立同族记忆卡**（依 `MEM-20261006-08`：复犯 MUST 升级为可执行门禁，MUST NOT 再记一张同族卡）。
- 2026-10-14 **Task 1 落地（第三笔之后的执行）**：九个 Step 全部完成（见 Task 1 末尾《实施记录》）。
  **提案面**随批补齐 **3 份 delta**：`instance-handle-pool`（★ 实施期发现池**真的要改**——第三模板参 /
  `IsValid` 奇偶校验 / 两个重载 / 五个按槽位下标的口；初稿猜"沿用、无需改"是错的）、
  `state-stacking-policies`（`Custom 决策 Fragment` 改述为"统一接口的状态侧薄型"——**初稿登记的"已知缺口"就此闭合**）、
  以及 `entity-runtime-mechanism` 内的代际发号要求。
  **门禁**：`validate --all --strict` = **43 passed / 0 failed**；双配置 **0 error / 0 warning**；
  无头装置 **10 条命令逐字不变**（`SkillCast.Run` 26/0、`.Reject` 16/0、`SkillParamChain.Run` 26/0、`.Reject` 9/0、
  `SkillLayer.Run` 24/0 + `[复现性摘要]` **逐字相同**、`.Reject` 7/0、`CooldownCost.Run` 33/0、`.Reject` 7/0、
  `State.Reject` 6/1、`Slice.Run` 3/3）；**ARO 阴性对照**：关 `ForEachForGC` ⇒ 26/0 → **24/2**、`存活=否`，还原后回 26/0。
  **两仓零提交**（提交须用户显式授权）。**下一步 = Task 2（Buff payload 化）**；Task 1 残留两个 PIE 复核点（见《实施记录》末段）。
