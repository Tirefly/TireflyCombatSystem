# 重构实施计划：运行态基底化、`TcsBuff` 拆出、资产加载层落地

- **文档 ID**：`PLN-REFACTOR-1`
- **类型**：PLN / 计划
- **状态**：ACTIVE（2026-10-13 建立；**实施进行中**——Task 1 / Task 2 / Task 3 已交付，**下一开工点 = Task 4**。本计划是 `DEC-2026-10-13-state-module-positioning` **与** `DEC-2026-10-13-asset-butler` 两份裁决的**合并执行形状**）
- **权威范围**：**两条线合成一份执行形状**（★ 2026-10-13 合并，理由见 §0.5）——(A) 运行态**基底抽形**与**共享机制合并**、两个 payload 的拆分、**`TcsBuff` 新模块的拆出**；(B) **`TcsAssetButler` 新模块落地**、四处类型搬迁、定义库瘦身。附规格面回写与证据重取。**不在本文**——裁定本身（住两份 `DEC-*`）、关系表族语义（住 `DEC-2026-10-13-relation-*`）、`e1`/`e4` 的重写明细（各归 `PLN-R5.5` 改述后）
- **最后更新**：2026-10-14（**★ 同日第八笔 = Task 3 落地**：七个 Step 全部完成（Step 3 由 Task 1 交付、本 Task 只订正状态）+ 双配置 **0/0** + 无头装置 **11 条行为信号逐字相同**。**★ 两处形状由用户开工时改定**（取代计划原文）：① **不单开 payload 类型**（与 Task 2 同一条判据）；② **彻底一词一义**——记录 `FTcsCastRun` → **`FTcsSkillInstance`**、句柄 → **`FTcsSkillInstanceHandle`**、**方法/成员名一并改**（12 个标识符）。**11 字段去向 = 5 上提基底 + 6 留本域**（`RunSource`→`CascadeAnchor`、`PhaseExpiryEntry`→`ExpiryEntry`）。**两处用户裁定**：① 基底 `DefTag` 在技能侧**填上 + 8 处改读自己**，**但保留"条目撤销 ⇒ 不发终止事件"那处守卫**（去掉即行为变更）；② 句柄与方法名一并改。**一处过期描述订正**：计划原文把「门禁④ 改走共享决策」列为 Task 3 的 Step 并称"本重构的实质收益点"——**该收益点已在 Task 1 落地**（`git log -S` 唯一命中 `0ac43b6`），本 Task **零改动**。**一处判据的归属订正**：原验收信号"`TcsCastOps.cpp` 内零内联 Instancing 分流"已在 Task 1 满足（今日仍成立，但**不是**本 Task 的成绩）。**★ 一处真实的过程缺陷如实登记**：机械改名中途**出过一次文件损坏**（PowerShell 辅助函数参数被拍平、退化成逐字符替换 ⇒ 三个文件共 **1430 个字符**被换掉），**按 `HEAD` 逐字节还原**后改用**脚本文件 + 命中护栏 + 字符不变量核对**重做 ⇒ 立纪律「**批量替换 MUST 走脚本文件、MUST NOT 走 shell 内联；每次替换后 MUST 核对成对字符守恒**」。**一处基线选型陷阱**：Task 2 的 `03-SkillCast.Phase.log` 是**截断版**（P9 命中 0），基线 MUST 取 `03b` —— **同一坑的第二次**。**★ 一处由用户提问暴露的流程缺口**：Task 2 / Task 3 **都未开提案**（属**静默跳步**），根因 = Global Constraints 的「每个 Task 以提案开道」与 Task 5 的**批处理设计自相矛盾** ⇒ 已订正为**两条合法路径 + 选路判据**，并补记未偿欠账（主规格 **14 能力 / 52 处**引用旧类型名，归 Task 5）。**两仓零提交**。**前一笔（同日）＝第七笔 = Task 2 落地**：六个 Step 全部完成 + 双配置 **0/0** + 无头装置 **11 条读数逐字相同**。**★ 两处形状由用户开工时改定**（取代计划原文）：① **不单开 payload 类型**、字段直接平铺进派生结构；② 命名按 **Def 层同构** = 基底 `FTcsStateInstance`（**保留原名**）+ 派生 **`FTcsBuffInstance`**（**MUST NOT 含 `Entity`**——它在本仓专指"战斗实体"）。**同批记录我自己的错名**（初稿 `FTcsEntityRuntime`/`FTcsBuffRuntime` 撞上 `Entity` 专指）。**一处判据作废**：原验收信号"`FTcsStateInstance` 已不存在于代码"因基底保留原名而**不再成立**，已按实况改写。**一处未闭合**：状态侧实例 ARO 分支无头不可达（三次阴性对照空转，已如实记边界）。**两仓零提交**。**前一笔（同日）＝第六笔 = 三个待裁点裁定 + 动作 D**：① 基底只放**撤销锚点**、不放**施加方来源**（两者**角色相反**；Task 1 的表 `Source` 行写错 + `CascadeAnchor` 行漏写）；② **`Phase` 留状态侧**；③ **新立 §0.5.2 动作 D「代码文件超限走三段式」**——用户原话「**先执行重构，然后留档，在测试没问题后再拆分**」。**同批落纸《14 字段最终归属表》**（基底 6 / 状态侧独有 4 / Buff payload 4 = 14 ✓）。**前一笔（同日）＝第五笔 = Task 2 开工前置**：动作 A 过完在册 **75 条**（结论 = 本 Task **不消费任何条目**）、动作 B 现场日 **2026-10-14**（与轮次日一致、无错位）、动作 C 出**逐字段护栏表**；抓到两处缺口（基底表只表态 11 个实测 14 个 / §0.5.2 体例债表漏登记 3 个文件，其中两个正面波及 Task 2）⇒ Task 2 的 `Files` 段**原缺宿主装置侧**，实测波及约 **80 处调用点**，已补入。**前一笔（同日）＝第四笔 = Task 1 落地**：九个 Step 全部完成 + 双配置 0/0 + 无头装置 10 条读数逐字不变 + ARO 阴性对照复现（见 Task 1 末尾《实施记录》）。**两仓零提交**。**前一笔：开工门禁与日期留痕**——新增 **§0.5.2**「每个 Task 开工前 MUST 执行的固定动作」（**动作 A** 逐条对已登记债的触发条件，`MEM-20261010-08`；**动作 B** 日期取现场值，`MEM-20261004-05`；**动作 C** 合并前先出护栏表，`MEM-20261014-01`；**动作 D** 超限三段式，2026-10-14 补立）；**动作 A 首稿曾误判**（把"include 了要动的头"当成"会被改动影响"）⇒ 已在 §0.5.2 内**就地订正并留痕**，并补立「**体例债表 MUST 按 Task 重新枚举、MUST NOT 沿用上一轮**」的使用纪律。**★ 日期留痕**：本计划与两份 `DEC-2026-10-13-*`、Task 1 提案**实际创建于 2026-10-14 凌晨**，文档 ID / 文件名 / 正文日期**一律沿用轮次日 2026-10-13**（跨零点收尾，改名牵动 ID/文件名/映射表/计数 ⇒ 收益零、漂移正）⇒ **留痕不改名**（先例 `MEM-20261004-12`）。**前两笔（2026-10-13）**：**第二笔：并入资产加载层线**——`DEC-2026-10-13-asset-butler` 裁定新建 `TcsAssetButler` + 四处搬迁 + 定义库瘦身；**两条线会改同一批文件**（`TcsEffect` / `TcsIntegration` / `.uplugin` / 模块计数）⇒ 合并为本计划，顺序见 §0.5。**第一笔**：**同日修正**：初稿按裁决⑦ 把 Task 4 写成"`TcsState` → `TcsBuff` **改名**"⇒ 用户评审时提问驱动实测，**该前提被推翻**（`TcsState` 是三层：共用 Def 族 + 通用运行时机制 + Buff 专属），现行 = **拆出新模块**、基底与机制**留 `TcsState`**。详见 §0.3.0 与该裁决 §6）

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
- **规格面处置：两条合法路径，MUST 显式选一条（★ 2026-10-14 订正，见下方留痕）**
  - **路径 A（逐 Task 开提案）**：`openspec propose <id>` → `openspec validate <id> --strict --no-interactive` 通过
    → 实施 → `openspec archive <id> --yes`；归档时 MODIFIED 需求的**标题 MUST 与主规格逐字一致**。
  - **路径 B（批处理，集中到 Task 5）**：实施期**不开提案**，规格回写**集中在 Task 5** 一次做完。
  - **选路判据**：该 Task 是否**新建能力**或**改变对外语义** ⇒ **路径 A**；
    只在**既有能力内部换实现形状**（类型改名 / 字段归属迁移 / 模块搬迁）⇒ **路径 B 合法**。
  - **★ 走路径 B 时的硬要求**：该 Task 的《实施记录》MUST 写明
    ①**选了 B 及其判据**；②**待回写的能力清单与处数**（实测得出，MUST NOT 只写"规格待同步"）。
    **MUST NOT 静默跳步**——不开提案又不写处置，等于让规格与代码的偏差无人登记。

  > **★ 本条是订正后的形态（2026-10-14，MUST 读）**：原文为「**每个 Task** 的实施以 OpenSpec 提案开道」，
  > 该措辞与本计划 **Task 5（规格与能力面回写）** 的**批处理设计直接矛盾**——若每个 Task 都开提案，
  > Task 5 就无事可做；而若集中到 Task 5，原文那条就被违反。**两条并存 = 谁都不知道该听谁的。**
  > **后果实证**：Task 2 与 Task 3 两轮**都未开提案、也都没有记录这个决定**（属**静默跳步**），
  > 直到用户 2026-10-14 提问「Task 3 的执行没有创建提案吗」才暴露。
  > **订正依据**：原计划 §0.4 与 Task 5 的 Step 1 已明写"规格回写集中做"，且
  > **Task 1 走的正是路径 A**（`merge-entity-runtime-mechanisms`，含新建能力 `entity-runtime-mechanism` + 归档）
  > ⇒ 两条路径**在本计划内本来就都存在**，缺的只是把它们命名出来并给判据。
  > **⚠️ 未偿欠账（MUST 读）**：Task 2 / Task 3 走的是**路径 B 但当期未按 B 的硬要求登记清单**。
  > 已实测补记于各自《实施记录》（**14 个能力 / 52 处**引用待回写），归 **Task 5** 消费。
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

### 0.5.2 ★ 每个 Task 开工前 MUST 执行的固定动作（2026-10-14 立，可核）

> **为什么单列一节**：这些纪律都已入库，但**它们的执行动作没有落点** ——
> 纪律只以"要注意"的形态存在时，执行时不会有触发点（同 `MEM-20261006-08`）。
> ⇒ 把它写成**计划里的固定动作**，而不是留在记忆卡里靠人记得。
>
> **★ 使用纪律（2026-10-14 Task 2 开工时补立，MUST 读）**：本节的**体例债表是"现场枚举的产物"，
> 不是"本计划的清单"**——每个 Task 开工时 MUST **按自己的改动面重新枚举一遍**
> （`Source/TcsDev/**` 全量扫行数 + 扫对本 Task 改动面的引用），**MUST NOT 只复核上一轮那几行**。
> **立此条的依据 = 一次实测漏登记**：Task 2 开工枚举后发现超 800 行的实为 **6 个**（原表只列 3 个），
> 且**两个正面波及 Task 2 的文件都在漏登记之列**。**根因** = 表被"**哪一轮建的**"绑住，
> 而它本该是"**每个 Task 各自现场枚举**"的动作 —— **这是 `MEM-20261010-08` 的症状换了个载体再次出现**
> （该卡治的是"债务册只对看起来相关的那条"，此处是"体例债表只对建成那一轮的那批"）。
> ⇒ 处置 = **纪律升级为"按 Task 重枚举"**（依 `MEM-20261006-08`：复犯 MUST 升级为可执行门禁，
> **MUST NOT 再记一张同族卡**）。

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

> **★★ 上表的使用纪律（2026-10-14 Task 2 开工时补立，MUST 读）**：上表是**按 Task 1 的改动面**枚举的，
> **MUST NOT 当作"本计划的体例债清单"沿用**——Task 1 与 Task 2 动的**不是同一批文件**。
> **每个 Task 开工时 MUST 按自己的改动面重新枚举一遍**（`Source/TcsDev/**` 全量扫行数 + 扫对本 Task 改动面的引用），
> **MUST NOT 只复核上表那几行**。（**该纪律已上提为 §0.5.2 的开篇使用纪律**，此处保留现场记录。）
>
> **立此条的依据 = 一次实测漏登记**：Task 2 开工枚举后发现 `Source/TcsDev/**` 超 800 行的实为 **6 个**（上表只列 3 个），
> 且**两个正面波及 Task 2 的文件都在漏登记之列**（`TcsDevSliceRig.cpp` 3552 行 / `FTcsStateInstance` 34 处；
> `TcsDevSliceRig_State.cpp` 803 行 / 8 处）。**根因** = 表被"**哪一轮建的**"绑住，
> 而它本该是"**每个 Task 各自现场枚举**"的动作——**这是 `MEM-20261010-08` 的症状换了个载体再次出现**
> （该卡治的是"债务册只对看起来相关的那条"，此处是"体例债表只对建成那一轮的那批"）。
> ⇒ 处置 = **纪律升级为"按 Task 重枚举"**（MUST NOT 再记一张同族卡，依 `MEM-20261006-08`）。
> **详见 Task 2 的《开工前置》块。**

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

**动作 D：代码文件超限时走三段式**（★ 用户 2026-10-14 立，**跨任务通用**）

> **原话**：「代码文件重构如果行数超限，**先执行重构，然后留档，在测试没问题后再拆分**」

| 段 | 动作 | MUST NOT |
|---|---|---|
| ① **先执行重构** | 该改就改，重构的正确性优先于行数 | **MUST NOT** 因"改完会超限"而回避、绕道或改写设计 |
| ② **留档** | 把"哪个文件超限、超了多少"写进该 Task 的实施记录；可核项 = **增长量** | **MUST NOT** 再把可核项写成"零增长"（会与①直接冲突） |
| ③ **测试通过后再拆分** | 拆分**不是禁止**，是**时机固定在验收之后** | **MUST NOT** 在重构改动与验收之间插入拆分（会让刚验过的执行体作废） |

> **★ 与既有裁定的关系（二者不冲突，是同一件事的两半）**：Task 1 的裁定「MUST NOT 借 Task 1 顺手拆分」
> **仍然成立**——它管的是"**这一轮不做拆分**"；**动作 D 补上它缺的另一半**——拆分**不是无限期禁止**，
> 而是**时机固定在"测试通过之后"**。⇒ 今后遇到超限，**不再需要逐次问"拆还是绕"**，按动作 D 三段式走即可。

---

## 文件结构（动工前先锁定职责边界）

### 新建（`TcsState`，共享基底与机制 · 阶段 1）

> **★ 两个文件的创建时机不同（MUST 读）**：`FTcsStateInstance`（基底 struct）的**拆分**在 **Task 2** 完成
> （Task 1 只定字段表——否则当场没有消费者，违反"零消费者不预建"）；其余四件在 **Task 1** 创建并**当场被两域采用**。
> **★ 2026-10-14 Task 2 落地后订正**：基底**不是新建文件**，而是**现有 `TcsStateInstance.h` 原地瘦身**（保留原名）；
> Buff 专属字段住**新建的 `TcsBuffInstance.h`**。**原计划的 `TcsBuffPayload.h` 已作废**（用户裁定不单开 payload 类型）。

| 文件 | 职责 |
|---|---|
| `Source/TcsState/Public/State/TcsStateInstance.h` | **基底运行态 struct**（身份/归属/快照/一个到期锚）。**★ 2026-10-14 Task 2 实测落地形态**：**不是新建文件**，而是**现有文件原地瘦身**（14 字段 → 公共 6 字段）、**名字保留**。原计划的 `Runtime/TcsEntityRuntime.h` **不创建** |
| `Source/TcsState/Public/Runtime/TcsInstanceHandleOps.h` | **判据 / 相等比较 / 哈希 / 池位模式转换的语义单点**（非反射公共件）。**★ MUST NOT 新增统一句柄类型**——三个句柄保持独立，各自委托本件（裁决 §8） |
| `Source/TcsState/Public/Runtime/TcsEntityRuntimeBucket.h` | per-entity 桶（**载体 = `TTcsInstancePool`**，用户 2026-10-13 拍板） |
| `Source/TcsState/Public/Runtime/TcsEntityRuntimeRegistry.h` | 桶容器（`TMap<Handle, TUniquePtr<Bucket>>` + 遍历） |
| `Source/TcsState/Public/Runtime/TcsEntityAdmission.h` | **重申请决策接口**（可覆写）+ 决策结果类型（合并现堆叠决策与技能 Instancing） |

### 修改（`TcsState`，共享层）

| 文件 | 改动 |
|---|---|
| `Source/TcsState/TcsState.Build.cs` | **删除 `TcsEffect` 依赖**（裁定⑫：拆出 Buff 专属后不再需要）；保留 `TcsCore` / `TcsNotation` / `TcsAttribute` |
| `Source/TcsState/Public/State/TcsStateSnapshot.h` | **保留**（不再下移 Core；断言其"参数域概念"的自述仍成立，只是落点是共享层而非 Core） |
| `Source/TcsState/Public/State/TcsStateHandle.h` / `TcsStateRegistry.h` | `TcsStateRegistry.h` 由共享层模板（`TTcsEntityRuntimeBucket` / `FTcsEntityRuntimeRegistry`）**绑定取代**（**原地替换，不跨模块搬迁**）；**★ Task 2 后其载荷已绑 `FTcsBuffInstance`**（MUST NOT 绑基底）。**`TcsStateHandle` 保留**（裁决 §8：三个句柄 MUST NOT 合并） |

### 新建（`TcsBuff`，Buff 专属模块）

| 文件 | 职责 |
|---|---|
| `Source/TcsBuff/TcsBuff.Build.cs` | 模块定义；依赖 `TcsCore` / `TcsNotation` / `TcsAttribute` / **`TcsEffect`**（链步骤）/ `TcsState`（共享层） |
| `Source/TcsBuff/Public/State/TcsBuffInstance.h` | **Buff 运行态记录**（★ 2026-10-14 Task 2 已建成，**暂住 `TcsState`** ⇒ Task 4 随模块搬入）：基底 `FTcsStateInstance` + `Handle` / `Source` / `Instigator` / `Phase` + `Stacks` / `DurationRemaining` / `PeriodRemaining` / `PeriodEntry`。**原计划的 `TcsBuffPayload.h` 已作废**（用户裁定不单开 payload 类型，字段直接平铺） |
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
| `Source/TcsSkill/Public/Skill/TcsSkillInstance.h`（新） | **★ 2026-10-14 Task 3 实测落地形态**：**`FTcsSkillInstance` 继承基底** + 6 个技能域字段（`EntryHandle` / `PhaseIndex` / `ChainRunHandle` / `ActiveChainId` / `MainChainStart` / `MainChainStartPhaseTag`）。**原计划的 `TcsCastPayload.h` 已作废**（用户裁定不单开 payload 类型，字段直接平铺） |
| `Source/TcsSkill/Public/Skill/TcsSkillInstanceHandle.h`（新） | **★ 原计划未列**：句柄随记录一并改名（`FTcsCastRunHandle` → 本名，幻影标签同改）——判据 = **彻底一词一义** |
| `Source/TcsSkill/Public/Skill/TcsCastRun.h` / `TcsCastRunHandle.h` | **删除**（被基底 + 派生取代；**`FTcsCastRun` 该名字已不存在于代码**） |
| `Source/TcsSkill/Private/Skill/TcsCastOps*.cpp` | ✅ 改读基底 + 派生；✅ 门禁④ 改走共享决策接口（**已由 Task 1 交付**） |
| `Source/TcsSkill/Public/TcsSkillSubsystem.h` | ✅ 池/注册表/ARO/重入守卫复用共享层版（`TcsState`，Task 1 + Task 2 已交付） |

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

### Task 2 开工前置（§0.5.2 固定动作的**实测记录** · 2026-10-14）
>
> **本块的性质 = 现场记录**（动作定义住 §0.5.2，此处只记**本轮实测到了什么**）。
> 本轮执行时 §0.5.2 有 A/B/C 三个动作；**动作 D 是本轮裁定③ 刚立的**，故不在本轮 A/B/C 的记录内。

**动作 A：逐条对已登记债的触发条件**（依 `MEM-20261010-08`）

台账在册 **75 条**（74 严格 + 1 历史命名 `Task-3-1`），**逐条过完**。与本 Task 改动面相交的**只有 3 条擦边者，且全部判定为不成立**：

| 条目 | 触发条件 | 实测判定 |
|---|---|---|
| **`STAT-8`** | 出现"多来源叠层 + 按来源退订 / 退层"的真实消费者 | **不成立**——本 Task 只搬字段、不改叠层语义；全仓"叠层落新来源"唯一点 `TcsStateOps_Stack.cpp:258` 的语义逐字不变 |
| **`MECH-1`** | 再有一处扁平池、或该实现需要 GC 口 / 奇偶校验 / 可注入发号 | **不成立**——它指 `TcsEffect` 侧的 `FTcsTriggerRegistry`（扁平）；本 Task 不动 `TcsEffect` 的登记表 |
| **`WAIT-13`** | buff 侧出现"按实时档求值"的需求，或**为 buff 接线数值读口时** | **不成立**——本 Task 不接线数值读口（`TcsStateParamTableReader` 面不动） |

**余 72 条逐类判据**（MUST NOT 只对"看起来相关"的那条）：
- **已闭合 / 已消费（约 22 条）**：`STAT-2`（部分）/ `STAT-5` / `ATTR-1` / `CHAIN-3` / `CHAIN-7` / `TRIG-1` / `TRIG-4` / `TRIG-6` / `DAMAGE-5` / `WAIT-6` 等，无待办面；
- **轮次归属在 R7 / R8（16 条）**：`INTEG-1/2/3`、`TOOLS-1~9`、`PRES-1`、`WAIT-10`，本 Task 不在这些轮内；
- **触发条件型、条件未成立（约 56 条）**：`STAT-3/4/6/7/9/10/11`、`CHAIN-1/2/4/5/6/8`、`DAMAGE-1~4`、`TRIG-2/3/5`、`COST-1/2/3`、`COOLDOWN-1`、`SCRIPT-2/3/9/10`、`WAIT-1~5/7~9/11/12/14`、`CORE-1`。

> **★ 结论：本 Task 不消费、不触发任何台账条目。**

**★ 但动作 A 的真实命中不在债务册，在「体例债」登记表本身**

§0.5.2 的表只登记了 3 个超限文件，**实测 `Source/TcsDev/**` 超 800 行的有 6 个**：

| 文件 | 行数 | 对 Task 2 改动面的引用（实测） | 登记状态 |
|---|---|---|---|
| `TcsDevSliceRig.cpp` | **3552** | **`FTcsStateInstance` × 34**、`GetState` × 25、`TcsStateOps` × 5、`TcsStateSubsystem` × 17 | ★ **漏登记 · 正面波及** |
| `TcsDevSliceRig_CooldownCost.cpp` | 1671 | 零引用 | 已登记（绕开） |
| `TcsDevSliceRig_Skill.cpp` | 1129 | 零引用 | 已登记（绕开） |
| `TcsDevSkillParamChainProbe.cpp` | **938** | 零引用 | ★ **漏登记 · 零引用** |
| `TcsDevSkillCastProbe_Phase.cpp` | 856 | 零引用 | 已登记（绕开） |
| `TcsDevSliceRig_State.cpp` | **803** | **`FTcsStateInstance` × 8**、`GetState` × 10、`TcsStateSubsystem` × 8 | ★ **漏登记 · 正面波及** |

⇒ **两个正面波及的文件都是漏登记项**。**根因（正是 `MEM-20261010-08` 要治的症状，只是换了载体）**：§0.5.2 的表是**按 Task 1 的改动面**枚举的，而 Task 1 与 Task 2 动的**不是同一批文件** ⇒ 表被"哪一轮建的"绑住，**未按"每个 Task 重新枚举"使用**。⇒ **表 MUST 按 Task 重新枚举，MUST NOT 沿用上一轮的表。**

**装置侧受影响的调用点实测（拆解后逐个要动的点）**：

| 字段 | 现行表判定 | 装置侧读取点 |
|---|---|---|
| `DurationRemaining` | Buff payload | `TcsDevSliceRig.cpp` **10** 处、`TcsDevSliceRig_State.cpp` **1** 处 |
| `Stacks` | Buff payload | `TcsDevSliceRig.cpp` **7** 处、`TcsDevStackDecisionSample.cpp` **1** 处 |
| `Handle` / `Source` / `CascadeAnchor` / `DefTag` / `Entity` / `Level` / `Phase` / `ParamSnapshot` | 基底（现行表） | 合计约 **60** 处 |

⇒ **装置侧约 80 处调用点在本 Task 波及面内**，而 Task 2 的 `Files` 段**只写了 `TcsStateOps_*.cpp`（13 文件）**、**未含 `Source/TcsDev/**`** ⇒ **Files 段不完整，MUST 补齐**（见下方《Files》订正）。

**动作 B：日期取现场值**（依 `MEM-20261004-05`）

现场 `Get-Date -Format 'yyyy-MM-dd'` = **2026-10-14**，与轮次日一致（上一笔收尾于同日 12:45 前）⇒ **本轮无跨零点错位，无需留痕**。

**动作 C：「护栏表」**（依 `MEM-20261014-01`）

本 Task 的实质 = "两个域的运行态**公共字段合并进一个基底 struct**" ⇒ 属"N 个同形物合并为 1"，**动作 C 适用**。逐字段出表（一行为一物，列该物**独有**的拦截）：

| 合并候选 | 状态侧守着的拦截 | 技能侧守着的拦截 | 判定 |
|---|---|---|---|
| **`Source`** | = **施加方来源**，明文 **MUST NOT 当撤销锚点**（"按它撤销会把别条实例的条目一起摘掉"，`TcsStateInstance.h:28-34`） | 技能侧**无**"施加方来源"这一角色；其 `RunSource` **就是**撤销锚点（`CHAIN-7` 闭合依据） | ★ **MUST NOT 合并**——两者**角色相反**；合并会抹掉状态侧"撤销锚点不是它"的**字段级载体** |
| **`CascadeAnchor` / `RunSource`** | 级联撤销锚点（修正器与内联触发行按它一次摘净） | 同角色（`RemoveBySource`） | **可合并**（同角色两个名字）——**但现行表根本没有这一行** |
| **`ExpiryEntry` / `PhaseExpiryEntry`** | **时值到期**条目；与 `PeriodEntry` 分离的理由 = "两个语义可同时成立"（`TcsStateInstance.h:109-117`） | **当前时段**的取消锚点，**每段换一次**（`AdvancePhase` 首句复位，`TcsCastRun.h:178-181` 的 ensure 陷阱） | **可合并**（同一实例上两者不同时存在），**但 MUST 逐字保留 `PeriodEntry` 分离的原文理由** |
| **`Handle`** | `FTcsStateHandle`；`GetState` / `RemoveState` 收它 ⇒ 传技能句柄**编译失败** | `FTcsSkillEntryHandle`（**账本条目**句柄，非 run 句柄） | **MUST NOT 退化为单类型**——合并为一个 `Handle` 字段必选一者 ⇒ **另一侧的编译期拦截降级**（同 Task 1 裁决 §8 的三句柄负需求） |
| **`Phase`** | `EStatePhase`（`ESP_Inactive/Active/Expiring`）；`ESP_Expiring` 是 `STAT-5` 守卫的判据载体 | **无对应枚举**（技能侧 `PhaseIndex` 是**时段游标**，§0.3 ③ 明文 MUST NOT 混） | ★ **技能侧零消费者** ⇒ 与"零消费者不预建"冲突（**已裁定：留状态侧**，见下） |
| `DefTag` / `Entity` / `Level` / `ParamSnapshot` | 两域逐字同义、同角色 | 同 | **可合并**（无独有拦截） |

> **★ 本表是"裁定前"的现场记录（`Source` / `Phase` / `Handle` 三行未含裁定结论）**
> ⇒ 归属的**权威 = 下方《14 字段最终归属表》**，本表只留"当时看到什么"。
> **MUST NOT** 拿本表当落地依据。

---

### ★ 三个待裁点 —— 已裁定（2026-10-14，用户拍板）

**裁定①：基底只放「撤销锚点」，不放「施加方来源」**

| 字段 | 归属 | 技能侧对应物 | 判定 |
|---|---|---|---|
| **`CascadeAnchor`** | ✅ **进基底** | ★ **表里原先漏了这一行**——技能侧 `FTcsCastRun::RunSource` 与账本 `FTcsSkillEntry::LearnSource` **同角色**（都是"按它一次摘净"的级联锚点，`CHAIN-7` 的闭合依据） | **两域同角色 ⇒ 合并** |
| **`Source`** | ❌ **留状态侧** | **技能侧无此角色**（技能侧只有一个句柄，且就是锚点） | **角色相反 ⇒ MUST NOT 合并**（状态侧该字段的类注释明写"**MUST NOT 当撤销锚点**——按它撤销会把别条实例的条目一起摘掉"） |

> **★ 原文的错处（留痕，MUST 读）**：Task 1 的字段表把 `Source` 与"技能侧运行来源"当**同义**而写进基底
> ⇒ **两处都错**：① 状态侧 `Source` **不该**进基底；② 基底**该有**的 `CascadeAnchor` 一行**根本没写**。
> **根因 = 按名字相邻推断语义相同**（`MEM-20261013-09`）——**同一根因当日的第三次**
> （§0.5.2 初稿、Task 1 字段表、本处）。**判据已硬化为"同名 ≠ 同角色，MUST 逐字段读该字段自己的注释与全部读取点"**。

**裁定②：`Phase` 留状态侧，不进基底**

- 状态侧 `Phase`（`ESP_Inactive/Active/Expiring`）守的是**"同一条实例别被移除两次"**：移除流程中间跨越一次
  `Removed` 广播，订阅者可在广播里再调一次移除 ⇒ `ESP_Expiring` 是该窗口的判据载体。
- **技能侧有同类问题，但解法不同**：技能侧已用**重入守卫对象**（`FinishBroadcastGuard`）挡住重复终结，
  且 Task 1 刚把两侧守卫**统一成一份**（`TTcsEntityBroadcastGuard`）⇒ **技能侧对 `Phase` 零消费者**。
- ⇒ 按「**零消费者不预建**」**不进基底**。**将来若技能侧改用"阶段"判据，属独立议题**（届时它已有真实消费者，可正当入基底）。

**裁定③：代码文件超限的处置 = 三段式（★ 用户 2026-10-14 新立，跨任务通用）**

> **原话**：「代码文件重构如果行数超限，**先执行重构，然后留档，在测试没问题后再拆分**」

| 段 | 动作 |
|---|---|
| **① 先执行重构** | 该改就改，**MUST NOT 因"改完会超限"而回避或绕道**——重构的正确性优先于行数 |
| **② 留档** | 把"本 Task 使哪个文件超出上限、超了多少"**写进该 Task 的实施记录**（可核项随之改成"增长量"而非"零增长"） |
| **③ 测试通过后再拆分** | 拆分**不是禁止**，而是**排到验收之后**——**MUST NOT 在重构改动与验收之间插入拆分**（那会让刚验过的执行体作废） |

**⇒ 对上一条纪律的收窄（MUST 分清，二者不冲突）**：

- **Task 1 的裁定**"MUST NOT 借 Task 1 顺手拆分"**仍然成立**（拆分确实不在那一轮做）；
- **本裁定补上它缺的另一半**：拆分**不是无限期禁止**，而是**时机固定在"测试通过之后"**。
- **本 Task 适用**：`TcsDevSliceRig.cpp`（3552）/ `TcsDevSliceRig_State.cpp`（803）**就地改字段访问、允许增长**，
  **增长量写进实施记录**，**拆分排到 Task 2 验收之后**（若届时仍要拆）。
- **`TcsDevSliceRig_Internal.h:170` 的 `FindStateByDef` 是装置侧公共查找器** ⇒ 改名一处、传导全部调用点
  ⇒ 实际改动量**远小于 80 处**这个粗数。

---

**⇒ §0.5.2 的动作 A 结论（本 Task 实测，逐条留痕）**

**① 债务册**：在册 75 条**逐条过完**，结论 = **本 Task 不消费、不触发任何条目**（三条擦边者 `STAT-8` / `MECH-1` / `WAIT-13` 逐条判定不成立）。

**② 体例债（按裁定③的三段式留档）**：`Source/TcsDev/**` 超 800 行的实为 **6 个**（§0.5.2 原表只列 3 个）：

| 文件 | 行数 | 对 Task 2 的引用 | 处置 |
|---|---|---|---|
| `TcsDevSliceRig.cpp` | 3552 | `FTcsStateInstance` × 34、`GetState` × 25 | ① 就地改 ② 留档增长量 ③ 验收后再议拆分 |
| `TcsDevSliceRig_State.cpp` | 803 | `FTcsStateInstance` × 8、`GetState` × 10 | 同上 |
| `TcsDevSliceRig_CooldownCost.cpp` | 1671 | 零引用 | 不触碰 |
| `TcsDevSliceRig_Skill.cpp` | 1129 | 零引用 | 不触碰 |
| `TcsDevSkillParamChainProbe.cpp` | 938 | 零引用 | 不触碰 |
| `TcsDevSkillCastProbe_Phase.cpp` | 856 | 零引用 | 不触碰 |

---

**⇒ 14 字段最终归属表（Task 1 表漏 3 个，本表补全并订正 1 行）**

| # | 字段 | 归属 | 依据 |
|---|---|---|---|
| 1 | `DefTag` | **基底** | 两域逐字同义、同角色 |
| 2 | `Entity` | **基底** | 同上（两侧注释都自述"必须自持，O(1) 定位"） |
| 3 | **`CascadeAnchor`** | ★ **基底** | **两域同角色**（技能 `RunSource` / 账本 `LearnSource`）——**Task 1 表漏此行** |
| 4 | `Level` | **基底** | 两侧逐字相同（冻结的生效等级） |
| 5 | `ParamSnapshot` | **基底** | 两侧逐字相同 |
| 6 | `ExpiryEntry` | **基底** | 同一类型、同一角色（两侧各一枚"到期取消锚"）——**但 MUST 逐字保留 `PeriodEntry` 分离的原文理由** |
| 7 | **`Handle`** | ❌ **留各域** | **类型不同**（`FTcsStateHandle` vs `FTcsSkillEntryHandle`）⇒ 基底是**一个 struct**、只能有一个类型；且**技能施法运行态根本不持自己的句柄**（它只有 `EntryHandle` 标账本条目） |
| 8 | **`Source`** | ❌ **留状态侧** | 裁定①（角色相反） |
| 9 | **`Instigator`** | ❌ **留状态侧** | 状态侧是**存储字段**（可与 `Entity` 不同，有 4 个真实读取点：等级源入参 / 事件载荷 / 叠层更新 / 修正器物化）；**技能侧零存储消费者**（其 `MakeContext` 里 `OutContext.Instigator = Entity` 是**上下文派生值**，不是存储字段） |
| 10 | **`Phase`** | ❌ **留状态侧** | 裁定②（技能侧零消费者） |
| 11 | `Stacks` | **Buff payload** | Buff 专属 |
| 12 | `DurationRemaining` | **Buff payload** | Buff 专属 |
| 13 | `PeriodRemaining` | **Buff payload** | Buff 专属 |
| 14 | `PeriodEntry` | **Buff payload** | Buff 专属 |

**计数自洽**：基底 **6** + 状态侧独有 **4** + Buff payload **4** = **14** ✓

**Files（★ 2026-10-14 实施后按实况订正；原 Files 段已作废，保留在下）:**
- Modify: `Source/TcsState/Public/State/TcsStateInstance.h`（**拆解**：14 字段 → 公共 6 字段，**名字保留**）
- Create: `Source/TcsState/Public/State/TcsBuffInstance.h`（**★ 取代原计划的 `Runtime/TcsBuffPayload.h`**）
- Modify: `Source/TcsState/Public/State/TcsStateRegistry.h`（**桶与注册表载荷改绑 `FTcsBuffInstance`**）
- Modify: 域内文件共 18 个（`TcsStateOps_*.cpp` 9 / `TcsStateSubsystem*` 3 / 其余 6）
- Modify: 宿主装置 `Source/TcsDev/**`（**5 文件**）

> **★ 原 Files 段（已作废，留痕）**：`Create Runtime/TcsEntityRuntime.h` + `Create TcsBuffPayload.h`
> ——**两行均不成立**（用户裁定：基底**保留 `TcsStateInstance` 原名、不新建文件**；**不单开 payload 类型**）。
> **原 Interfaces 段同样作废**：`Consumes: FTcsEntityRuntime / FTcsEntityRuntimeHandle`（后者**从未存在**——
> Task 1 按裁决 §8 **刻意没有合并三个句柄**）⇒ 现行见下方。

**Interfaces:**
- Consumes: `FTcsStateInstance`（**基底**）/ `TTcsEntityRuntimeBucket` / `FTcsParamSnapshot`
- Produces: `FTcsBuffInstance`（基底 + 4 域内字段 + 4 Buff 专属字段 = **14 字段全有归属**）

**★ 宿主装置牵连（2026-10-14 动作 A 实测补入，MUST 读）**：本 Task 的消费面**第一次伸到宿主装置**
（Task 1 的两个域都在插件内）——**实测约 80 处调用点**在波及面内（`TcsDevSliceRig.cpp` 34 处类型引用 + 字段访问 17 处；
`TcsDevSliceRig_State.cpp` 8 处）。**MUST NOT 以为本 Task 不碰宿主侧**。
- **一处传导点**：`TcsDevSliceRig_Internal.h:170` 的 `FindStateByDef` 是**装置侧公共查找器**（读 `Candidate.DefTag`）
  ⇒ 一处改动传导全部调用点。**实测该字段在基底、访问写法不变**，故它**零改动**。
- **实测结果比预估好**：因字段平铺使访问写法**逐字不变**，5 个装置文件是 **52 增 52 删的等长替换**、**行数零增长**。

- [x] **Step 1：★ 按《14 字段最终归属表》落地（MUST 先做）**——Task 1 Step 1 的表只表态 **11** 个、且**错 1 行**
（`Source` 与技能侧 `RunSource` **角色相反**被当同义；真正该进基底的 `CascadeAnchor` **漏了**）
⇒ **MUST 以下方《14 字段最终归属表》为准**（基底 **6** / 状态侧独有 **4** / Buff payload **4** = **14** ✓）。
**MUST NOT** 沿用 Task 1 的旧表（否则 `Handle` / `CascadeAnchor` / `Instigator` 会在拆解时被"默认留在状态侧"，
而 `CascadeAnchor` 技能侧**确实需要**）。
- [x] **Step 2：拆 `FTcsStateInstance`**——基底字段**留原地**、Buff 专属字段迁 **`FTcsBuffInstance`**；**MUST 逐字段核对**无遗漏（**14 字段逐一有归属**，且**每个字段的"独有拦截"已过护栏表**）。
- [x] **Step 2b：★ 装置侧就地适配（居裁定③ 三段式的第①段）**——`TcsDevSliceRig.cpp` / `TcsDevSliceRig_State.cpp` 的字段访问**就地改**，
  **MUST NOT 因"改完超限"而回避或绕道**；**增量行数写进《Task 2 实施记录》**（可核项 = **增长量**，**不是**零增长）；
  **拆分排到 Task 2 验收之后**（MUST NOT 插在重构与验收之间——那会让刚验过的执行体作废）。
- [x] **Step 3：`STAT-6` 裁决点**——`PerStackValue` 是否顺手补。**判据 = 零消费者不预建**（不是"麻烦"）；写明结论 + 理由。
- [x] **Step 4：到期回调的锚点复位与悬空防护（Review Focus ③）**——MUST 保留现状纪律：回调入口**第一条语句复位自己的锚点**；捕获**世界弱引用 + 句柄**（不捕获裸指针）。
- [x] **Step 5：ARO 覆盖核验（Review Focus ①）**——payload 若含对象引用则 MUST 进 GC 遍历。**★ 实测结论**：Buff 专属 4 字段**零对象引用**（POD + 纯句柄），唯一地雷 `ParamSnapshot` 在基底、ARO 已覆盖。**但状态侧实例 ARO 分支在无头下不可达 ⇒ 该项未闭合**，边界见实施记录。
- [x] **Step 6：双配置编译 + 装置回归**——**判据 = 重构前读数逐字不变**（本 Task 仍不改行为）。

> **★ 两处与原计划的偏差（用户 2026-10-14 拍板，MUST 读，它们取代了计划原文）**：
> ① **不再单开 payload 类型**：既然已选**继承**，把 Buff 字段再包一层（`X.Payload.Stacks`）只多一层间接、
> **不增加任何隔离**——共享机制本来就只 include 基底、看不见派生字段。⇒ 字段**直接平铺**在 `FTcsBuffInstance` 里。
> ② **命名按 Def 层同构**：`FTcsStateInstance`（基底，**名字保留**）← `FTcsBuffInstance` / `FTcsSkillInstance`。
> **MUST NOT 取含 `Entity` 的名字**（如 `FTcsEntityRuntime`）——`Entity` 在本仓**专指**"承载 State/Buff/Skill/Attribute 的
> **战斗实体**"，而本类型装的是**实体身上的运行态**，用它命名属词汇二义性（同 `DEC-2026-10-14-entity-vocabulary` 所禁）。

**验收信号**：✅ `TcsState` 侧全部既有装置读数**逐字不变**（11/11）；⚠️ **`FTcsStateInstance` 并未消失**（它是**基底**、按用户裁定保留原名）⇒ 该条判据**已作废并按实况改写**（见实施记录）；✅ 14 字段全有归属；✅ **两个 Rig 文件行数零增长**（比预期好）。

### Task 2 实施记录（2026-10-14，★ 收尾 MUST 读）

**状态 = 六个 Step 全部落地**，双配置编译 `Result: Succeeded`（Dev Editor + Game Shipping，**0 error / 0 warning**），
无头装置回归 **11 条读数逐字相同**。

**★ 用户在本 Task 开工时改了两处形状（MUST 读，它们取代了计划原文）**：

| # | 计划原文 | 用户裁定（现行） | 判据 |
|---|---|---|---|
| 1 | 字段拆成「基底 + payload 成员」（`X.Payload.Stacks`） | **不单开 payload 类型**，字段**直接平铺**进派生结构 | 既然已选继承，再包一层只多一层间接、**不增加隔离**——共享机制只 include 基底、本就看不见派生字段 |
| 2 | 派生类型名**无**规定 | **`FTcsBuffInstance`**（基底 `FTcsStateInstance` 保留原名） | 与 **Def 层同构**（`FTcsStateDefBase` ← `FTcsBuffDef` / `FTcsSkillDefData`）；**MUST NOT 含 `Entity`** 一词 |

> **★ 关于裁定 2 的一处如实登记**：我初稿把基底拟为 `FTcsEntityRuntime`、派生拟为 `FTcsBuffRuntime`。
> 用户指出 `Entity` 在本仓是**专指**（承载 State/Buff/Skill/Attribute 的**战斗实体**，见 `FTcsCombatEntityHandle`），
> 而基底装的是**实体身上的运行态**、不是实体 ⇒ **属词汇二义性**，正是 `DEC-2026-10-14-entity-vocabulary` 所禁的那类错。
> **该纠正值得记一笔**：我在同一天刚做完 Entity 词汇归一，转手就造了一个同族错名 —— 说明
> "**知道该词专指什么**"与"**在取名时真的按它检查**"是两件事。

**★ 14 字段最终归属（实测落地，可复算）**

| 归属 | 字段 | 计数 |
|---|---|---|
| **基底 `FTcsStateInstance`** | `DefTag` / `Entity` / `CascadeAnchor` / `ParamSnapshot` / `Level` / `ExpiryEntry` | **6** |
| **Buff 域内 `FTcsBuffInstance`** | `Handle` / `Source` / `Instigator` / `Phase` | **4** |
| **Buff 专属**（同住 `FTcsBuffInstance`） | `Stacks` / `DurationRemaining` / `PeriodRemaining` / `PeriodEntry` | **4** |
| | | **14 ✓** |

**★ 一处关键连带（不这样改就会有静默缺陷）**：桶与注册表的**载荷类型 MUST 从 `FTcsStateInstance` 改绑 `FTcsBuffInstance`**
（`TcsStateRegistry.h`）。池按值存 `TArray<TPayload>` ⇒ **绑基底就是把 Buff 的层数 / 时值 / 周期直接丢掉**，
且因为是值截断、**多数读数仍会过**（只有读层数/时值的检查才会失败）——属最难查的一类。
已在 `TcsStateRegistry.h` 就地写明 **MUST NOT 改回**。

**★ 装置回归（判据 = 与 Task 1 基线逐字相同）**

| 装置 | 读数 | 判定 |
|---|---|---|
| `SkillCast.Run` | Passed=**26** Failed=0 在册 run=8 | ✅ |
| `SkillCast.Reject` | Passed=**16** Failed=0 在册 run=0 条目=6 | ✅ |
| `SkillCast.Phase` | `Phase` **29/0** + `PhaseDeferred` **34/0**（含 P9 段） | ✅ |
| `SkillParamChain.Run` / `.Reject` | **26/0** / **9/0** | ✅ |
| `SkillLayer.Run` / `.Reject` | **24/0**（`[复现性摘要]` 逐字相同）/ **7/0** | ✅ |
| `CooldownCost.Run` / `.Reject` | **33/0**（含跨帧尾段）/ **7/0** | ✅ |
| `State.Reject` | 通过 **6** / 失败 **1** | ✅（基线同值） |
| `Slice.Run` | 通过 **3** / 失败 **3** | ✅（基线同值，PIE-only 既有边界） |

⇒ **11/11 逐字相同**。**归一化口径**（既有先例）：只剔除**时间戳 / 行内帧计数 / `轮询拍数`** 三类**帧驱动量**
——它们是"跑了几帧"的痕迹，不是被测行为（先例 `EVID-2026-10-05-state-layer-pie` §6 已立此口径）。

> **★ 一处取证过程偏差（如实登记）**：首次跑 `SkillCast.Phase` 时我为省时间只等了 125 秒，
> 结果该命令的 **P9 段尚未执行**就被 Kill ⇒ 读数**不完整**（缺 `PhaseEnterDeferred` 明细）。
> 重跑给足 240 秒后补全，与基线一致。**教训 = 「读数看起来对」不等于「读数已跑完」**：
> 凡有**两拍落地**（延迟段 / 跨帧尾段）的装置，判据 MUST 先确认**该段真的出现过**，MUST NOT 只看前段计数。

**★ Step 5（ARO）取证边界 —— 一处我未能闭合的判据（如实，MUST NOT 读成已验）**

- **静态结论（已成立）**：`FTcsBuffInstance` 的 4 个专属字段**全是 POD 或纯句柄**
  （`int32` / `double` / `double` / `FTcsTimeEntryHandle`——后者只含 `TTcsInstanceHandle` + `bool`）⇒ **零对象引用**。
  全类型唯一的 GC 地雷是基底的 `ParamSnapshot`（其 `SourceRef` 可放宿主对象引用），**ARO 已覆盖它**（走 `ForEachForGC`）。
- **未能闭合的一半（如实证）**：我试了三次阴性对照，**全部空转**，逐条登记：
  | 尝试 | 结果 | 根因 |
  |---|---|---|
  | ① 关状态侧 ARO 遍历口，跑 `Tcs.Test.SkillCast.Run` | 仍 26/0 | **打错目标**——该命令的 C15 验的是**技能侧** ARO，与状态侧无关 |
  | ② 在状态 ARO 的实例分支插探针，跑 `State.Reject,SkillCast.Run` | 命中 **0** | 该日 ARO 入口被调 **9** 次，但**实例分支 0 次** |
  | ③ 在 ARO 入口插计数探针 | 桶数=**0** 实例数=**0**（×7 次） | **真因**：GC 发生时**状态侧无在册实例**——`State.Reject` 建的那条在 `CollectGarbage` 之前已被移除 |
  ⇒ **结论：状态侧实例 ARO 分支在无头路线下不可达**（需"GC 时真有状态实例在册"这一组合，而现有装置不产生它）。
  **这是取证边界，不是缺陷**——`SkillCast.Run` 的 C15 已正面证明**同一套 ARO 机制**在技能侧有效，
  且状态侧实例的 `ParamSnapshot` 处理与技能侧**是同一份代码形态**。
  **⇒ 未闭合项 = "状态侧实例的 ARO 分支"缺一条直接读数**，已按 `EVID` 惯例记为边界（不另立台账条目——它有本 Task 归属）。
- **探针已零残留**：三次探针均已在验证后移除，文件与备份**逐字节相同**（已核）。

**★ 规格面处置（走路径 B；同 Task 3，此处只登记不重复表格）**

本 Task 同样**未开提案**（判据同上：既有能力内换形状）⇒ 规格回写**归 Task 5**。
**待回写面并入 Task 3 的实测清单**（14 能力 / 52 处，两者合计计数，MUST NOT 各自另数一遍）。
**本 Task 独有的一处 MUST 写进 Task 5**：`state-instance-lifecycle`（7 req / 40 scenario，状态侧最大面）
——它的"实例"语义已从"装 14 字段的结构"变为"基底 + 派生两层"，**层数/时值/周期的归属是它的核心读数面**。

**★ 动作 D 第②段留档（裁定③，超限文件与增长量）**

| 文件 | 本 Task 前 | 本 Task 后 | 增长 |
|---|---|---|---|
| `TcsDevSliceRig.cpp` | 3552 | **3552** | **0** |
| `TcsDevSliceRig_State.cpp` | 803 | **803** | **0** |
| `TcsDevSliceRig_CooldownCost.cpp` | 1671 | 1671 | 0（未触碰） |
| `TcsDevSliceRig_Skill.cpp` | 1129 | 1129 | 0（未触碰） |
| `TcsDevSkillParamChainProbe.cpp` | 938 | 938 | 0（未触碰） |
| `TcsDevSkillCastProbe_Phase.cpp` | 856 | 856 | 0（未触碰） |

> **★ 两个正面波及文件实测「零增长」——比裁定③ 预设的"会增长"更好，根因值得记**：
> 因为**字段是平铺的**（用户裁定 1 的直接收益），**全部字段访问写法逐字不变**，改名只是
> **等长类型名替换**（`FTcsStateInstance` → `FTcsBuffInstance`，同为 18 字符）⇒ LAC 侧 5 文件
> **52 增 52 删**、行数不动。**⇒ 拆分仍按裁定③ 排在 Task 2 验收之后**（本 Task 无增长、故无新增拆分压力）。

**★ 变更规模（两仓，均未提交）**

| 仓 | 文件 | 增删 |
|---|---|---|
| TCS | 23 changed + 1 new | +406 / −158 |
| LAC | 5 changed | +52 / −52 |

**★ 一处判据作废（MUST 读，防后续误用）**：计划原验收信号写「**`FTcsStateInstance` 已不存在于代码**」——
该条**在用户裁定"基底保留原名"后即不再成立**（它现在是**基底**，不是被删类型）。
⇒ **已就地改写**为"14 字段全有归属 + 桶载荷已绑派生类型"。**MUST NOT** 再以"该类型名是否还有命中"当作完成判据。

---

## Task 3 — Cast payload 化 + 门禁④ 改走共享决策

**Files（★ 2026-10-14 实施后按实况订正；原 Files 段已作废，保留在下）:**
- Create: `Source/TcsSkill/Public/Skill/TcsSkillInstance.h`（**★ 取代原计划的 `TcsCastPayload.h`**）
- Create: `Source/TcsSkill/Public/Skill/TcsSkillInstanceHandle.h`（**★ 原计划未列**：句柄随记录一并改名）
- Delete: `Source/TcsSkill/Public/Skill/TcsCastRun.h` / `TcsCastRunHandle.h`
- Modify: 技能域全体（`TcsCastOps*.cpp` 4 / `TcsSkillSubsystem*` 3 / 域内头 15）
- Modify: `Source/TcsCore/Public/Pool/TcsInstancePool.h`（**注释一处**：改名连带）
- Modify: `Source/TcsState/Public/{Runtime,State}/*.h`（**注释三处**：改名连带）
- Modify: 宿主装置 `Source/TcsDev/**`（**6 文件**）

> **★ 原 Files 段（已作废，留痕）**：`Create TcsCastPayload.h` + `Modify TcsCastRun.h → 拆解`
> ——**两行均不成立**（用户 2026-10-14 裁定：与 Task 2 同一条形状——**不单开 payload 类型**，
> 字段**直接平铺**进继承基底后的派生结构）。**原 Interfaces 段同样作废**：`Produces: FTcsCastPayload`
> ⇒ 现行 = **`FTcsSkillInstance` 取代 `FTcsCastRun`**（继承基底 + 6 个技能域字段）。

**Interfaces:**
- Consumes: `FTcsStateInstance`（**基底**，Task 2 已建）/ `FTcsEntityAdmission`（Task 1 已建）
- Produces: `FTcsSkillInstance`（基底 6 + 技能域 6 = **原 11 字段全有归属**）；`FTcsSkillInstanceHandle`

- [x] **Step 1：定技能域字段集**——**6 个**（`EntryHandle` / `PhaseIndex` / `ChainRunHandle` / `ActiveChainId` / `MainChainStart` / `MainChainStartPhaseTag`）。**计划原文这一条是对的，零改动**。
- [x] **Step 2：拆 `FTcsCastRun`**——11 字段**逐一有归属**：**5 上提基底**（`Entity` / `Level` / `ParamSnapshot` / `RunSource`→`CascadeAnchor` / `PhaseExpiryEntry`→`ExpiryEntry`）+ **6 留本域** = 11 ✓；**另加基底 `DefTag` 的填值**（见下方裁定②）。
- [x] **Step 3：门禁④ 走共享决策接口**——**★ 已在 Task 1 落地，本 Task 零改动**（见下方《一处过期描述订正》）。
- [x] **Step 4：时段推进的锚点复位与悬空防护（Review Focus ③）**——保留现状明文纪律（"`AdvancePhase` 第一条语句即复位自己的锚点"、"起链会搬移池 ⇒ `Run` 指针可能已失效，故日志读回写前的值"）。**改名为 `ExpiryEntry` 后语义与纪律均不变**。
- [x] **Step 5：ARO（Review Focus ①）**——技能侧 ARO 遍历口改名 `ForEachSkillInstanceForGC`；**`ParamSnapshot` 仍在基底、ARO 覆盖路径不变**；实测无头下零红字。
- [x] **Step 6：双配置编译 + 技能侧装置回归**——双配置 **0/0**；无头装置 **11 条行为信号逐字相同**（范围大于计划原文列的 3 条）。
- [x] **Step 7：连带 `PLN-R5.5`**——`e1`/`e4` 的重写仍**留待后续轮**（本 Task 只改述状态，不改 `PLN-R5.5` 正文）。

> **★ 一处过期描述订正（2026-10-14，MUST 读）**：计划原文把 Step 3「门禁④ 改走共享决策接口」
> 列为本 Task 的 Step 并称"**这是本重构的实质收益点**"——**该收益点已在 Task 1 落地**
> （`git log -S 'FTcsEntityAdmissionAlwaysNew'` 唯一命中 `0ac43b6` = Task 1）。
> 该段的**现状**：只剩①**档位 → 策略的绑定**（读 `Def->Instancing` / `Def->bRetriggerOnActive` 选策略实例）
> 与②**决策结果的消费**（按 `Kind` 走驳回/顶替）——**这正是计划允许保留的两件事**，
> 决策（同组 / 接受 / 层数）全部住策略类。
> ⇒ **订正**：本 Step 的状态为"**已由 Task 1 交付**"，**MUST NOT** 再当作 Task 3 的待办；
> **验收信号**中"`TcsCastOps.cpp` 内零内联 Instancing 分流"**已由 Task 1 满足**。

> **★ 两处形状由用户 2026-10-14 开工时改定（取代计划原文，MUST 读）**：
> ① **不单开 payload 类型**——与 Task 2 同一条判据（已选继承后再包一层只多一层间接、不增加隔离）；
> ② **命名按 Def 层同构且一次改到底**：记录 `FTcsCastRun` → **`FTcsSkillInstance`**，
> **句柄 `FTcsCastRunHandle` → `FTcsSkillInstanceHandle`**（幻影标签同改），
> **方法/成员名一并改**（`GetCastRun`→`GetSkillInstance`、`CastRuns`→`SkillInstances`、
> `FinishCastRun`→`FinishSkillInstance`、`TerminateCastRun`→`TerminateSkillInstance`、
> `ForEachCastRunForGC`→`ForEachSkillInstanceForGC`、`ResolveRun`→`ResolveInstance` 等 12 个标识符）。
> **用户原话意旨**：**彻底一词一义**——"一次施法产生的记录"在全库只剩一个词。

> **★ 判据作废（MUST 读，防后续误用）**：计划原验收信号写「`TcsCastOps.cpp` 内**零**内联 Instancing 分流」——
> 该条**已在 Task 1 满足**（今日仍成立，但**不是**本 Task 的成绩）。**同段"`FTcsCastRun` 已不存在于代码"本次真的成立**
> （与 Task 2 那条不同：Task 2 的基底**保留原名**故该判据作废；本 Task 的记录类型**确实被替换**）。

**验收信号**：✅ 技能侧全部既有读数**逐字不变**（11/11 行为信号，含 Instancing 四态与 ARO 路径）；
✅ **`FTcsCastRun` 已不存在于代码**（判据成立）；✅ **`TcsCastOps.cpp` 内零内联 Instancing 分流**（Task 1 已满足）；
✅ 原 11 字段全有归属（5 上提 + 6 留域 = 11）；✅ 双配置 **0 error / 0 warning**。

### Task 3 开工前置（§0.5.2 四个固定动作，2026-10-14 现场执行）

> **★ 本节按 §0.5.2 的使用纪律"**每个 Task 按自己的改动面重新枚举**"执行**
> （**MUST NOT** 沿用 Task 1 / Task 2 的表——那是它们各自改动面的产物）。

**动作 A：逐条对已登记债的触发条件**（依 `MEM-20261010-08`）

在册 **74 条**（`deferred-inputs-ledger.md` 幂等 id 行实测）**逐条过完**，结论 = **本 Task 不消费、不触发任何条目**。
**三条擦边者逐条判定**：

| 条目 | 擦边原因 | 判定 |
|---|---|---|
| `CHAIN-7` | 本 Task 改的就是它的**闭合依据字段**（`RunSource` → 基底 `CascadeAnchor`） | **不触发**——它是"按来源回收的触发点缺失"，触发点已于 R6 落地；本 Task 只**改字段归属**，回收行为逐字不变（装置 `C1/C2/C3` 三路读数逐字相同即证） |
| `SCRIPT-10` | 技能账本 / 运行态的**脚本读写面缺席**，而它明文点名"`FTcsCastRunHandle` 本轮已定 `USTRUCT(BlueprintType)`" | **不触发**——判据是"出现脚本要读技能等级 / 在飞 run 的真实消费者"；本 Task **只改名不改反射性**（仍非反射 + 句柄仍反射），无新消费者。**但需同步一处措辞**：该条引用的类型名已变（归 Task 5 规格面） |
| `MECH-1` | 槽位+代际+空闲栈的**第四份实现**（扁平 `FTcsTriggerRegistry`） | **不触发**——判据是"再出现一处扁平池 / 需要 GC 口 / 奇偶校验 / 可注入发号"；本 Task **不动任何池载体**（技能侧仍用扁平的 `TTcsInstancePool`，只换载荷类型名） |

**★ 动作 A 的真实命中仍不在债务册，而在「体例债」登记表本身**（**Task 2 同类缺口的第二次**）：

| 文件 | 行数 | 对本 Task 改动面的引用 | 处置 |
|---|---|---|---|
| `TcsDevSliceRig.cpp` | **3552** | `FTcsCastRun` **0 处**（其 46 处命中全是 `RunSource`——**链侧** `FTcsEffectContext` 的字段，与本 Task 无关） | ✅ **不触碰** |
| `TcsDevSliceRig_CooldownCost.cpp` | **1671** | **0 处** | ✅ 不触碰 |
| `TcsDevSliceRig_Skill.cpp` | **1129** | `FTcsCastRunHandle` 11 + `FTcsCastRun` 1 = **12 处（正面波及）** | ✅ **就地改名**（等长替换），**MUST NOT** 拆分 |
| `TcsCooldownOps.cpp` | **1021** | **0 处** | ✅ 不触碰 |
| `TcsCastOps.cpp` | **998** | **29 处（正面波及，本 Task 主战场之一）** | ✅ 就地改（+74/−73） |
| `TcsDevSkillParamChainProbe.cpp` | **938** | `FTcsCastRunHandle` 3 + `FTcsCastRun` 2 = **5 处（正面波及）** | ✅ 就地改名 |
| `TcsDevSkillCastProbe_Phase.cpp` | **856** | `FTcsCastRunHandle` 15 + `FTcsCastRun` 1 = **16 处（正面波及）** | ✅ 就地改名 |
| `TcsSkillSubsystem.h` | **812** | `FTcsCastRunHandle` 9 + `FTcsCastRunTag` 1 + `FTcsCastRun` 4 = **14 处（正面波及）** | ✅ 就地改 |
| `TcsDevSliceRig_State.cpp` | **803** | **0 处** | ✅ 不触碰 |

> **★ 与 Task 2 的一处关键差别（MUST 读，防误判）**：Task 2 那次"漏登记"是**两个正面波及的文件未列入**，
> 本次**正面波及的 5 个超限文件全部检出了**（因新纪律已生效）。⇒ 本表**不是**又一次漏登记，
> 而是**新纪律第一次真的拦住了**（前一轮立的"MUST 按 Task 重枚举"在本轮产生效果）。
> **一处如实登记**：`TcsDevSliceRig.cpp` 的 46 处 `RunSource` 命中**极易误判为波及**——
> 它们是**链侧** `FTcsEffectContext::RunSource`（该字段**仍在、名字不变**）。判据 = **逐处核对字段归属，MUST NOT 按词形命中计数**。

**动作 B：日期取现场值**（依 `MEM-20261004-05`）

现场执行 `Get-Date -Format 'yyyy-MM-dd HH:mm:ss'` = **2026-10-14 15:51:57**，
与轮次日 **2026-10-14** 一致 ⇒ **无跨零点错位、无需留痕**。

**动作 C：「护栏表」**（依 `MEM-20261014-01`）

本 Task 的实质 = "技能运行态**并入基底** + **一次施法记录的全面改名**" ⇒ 适用。
逐字段出表（一行为一物，列该物**独有**的拦截）：

| # | 原 `FTcsCastRun` 字段 | 归属 | 该字段**独有**的拦截（合并后是否失效） |
|---|---|---|---|
| 1 | `Entity` | **基底** | 无独有拦截（两域逐字同义，注释都自述"必须自持，O(1) 定位"） |
| 2 | `Level` | **基底** | 无（冻结语义两域相同） |
| 3 | `ParamSnapshot` | **基底** | 无（GC 地雷在基底统一由 ARO 覆盖，**优于**两域各写一遍） |
| 4 | `RunSource` | **基底 `CascadeAnchor`** | 无——它的三个消费点全是"按它一次摘净"（链挂属性条目 / 本域物化参数链条目 / 传给链的锚点）⇒ **与 Buff 侧同角色**。**MUST NOT 与状态侧 `Source` 混同**（后者是"谁施加的"、明文 MUST NOT 当撤销锚点） |
| 5 | `PhaseExpiryEntry` | **基底 `ExpiryEntry`** | 无——与 Buff 侧时值到期同角色（当前计时项的取消锚）。**MUST NOT 与 `PhaseIndex` 混同**（同词根不同义：一个是下标、一个是取消锚） |
| 6 | `EntryHandle` | **本域** | **类型不同**（`FTcsSkillEntryHandle`）⇒ 基底是**一个** struct、只能有一个句柄类型；**且技能侧记录**不持自己的句柄**（扁平池，无"按自己句柄找自己"的需求） |
| 7 | `PhaseIndex` | **本域** | 与基底 `ExpiryEntry` **同词根不同义** ⇒ 上提即制造二义性 |
| 8 | `ChainRunHandle` | **本域** | 技能域专有（链运行态句柄） |
| 9 | `ActiveChainId` | **本域** | 技能域专有（"**实际**起的是哪条链"——链运行态反查不到） |
| 10 | `MainChainStart` | **本域** | 技能域专有（起链档位） |
| 11 | `MainChainStartPhaseTag` | **本域** | 技能域专有（起链档位引用的时段标识） |

**⇒ 11 = 5（上提基底）+ 6（留本域）** ✓
**另有一项**：基底 `DefTag` 在技能侧**原本不存**（每次去账本条目上取）⇒ 继承后**MUST 填**（裁定②）。
**两个句柄改名的护栏**：三个句柄 MUST 保持**独立反射类型**（`instance-handle-pool` 的编译期护栏）
⇒ `FTcsSkillInstanceHandle` 只是**改名**，**MUST NOT** 借机合并三个句柄。

**动作 D：超限文件与增长量留档**（★ 用户 2026-10-14 立，跨任务通用）

**本 Task 无新增超限文件**；正面波及的 5 个超限文件**均有增长**（如实登记，**MUST NOT** 写"零增长"）：

| 文件 | 行数（前 → 后） | 增长 |
|---|---|---|
| `TcsCastOps.cpp` | 997 → **998** | **+1** |
| `TcsDevSliceRig_Skill.cpp` | 1129 → **1129** | 0（等长替换） |
| `TcsDevSkillParamChainProbe.cpp` | 938 → **938** | 0（等长替换） |
| `TcsDevSkillCastProbe_Phase.cpp` | 856 → **856** | 0（等长替换） |
| `TcsSkillSubsystem.h` | 812 → **812** | 0（等长替换） |

**拆分时机 = 本 Task 验收之后**（动作 D 第③段）；**MUST NOT** 插在重构与验收之间。

### Task 3 实施记录（2026-10-14）

**状态 = 七个 Step 全部落地**（Step 3 由 Task 1 交付，本 Task 只订正状态），
双配置编译 `Result: Succeeded`（Dev Editor + Game Shipping，**0 error / 0 warning**），
无头装置回归 **11 条行为信号逐字相同**。

**★ 落地形状（可复算）**

```
FTcsStateInstance（基底，TcsState，Task 2 已建）
│  DefTag · Entity · CascadeAnchor · ParamSnapshot · Level · ExpiryEntry
│
├── FTcsBuffInstance    （TcsState 暂住 → Task 4 搬 TcsBuff）  8 域字段，Task 2
└── FTcsSkillInstance   （TcsSkill，本 Task 新建）              6 域字段
       EntryHandle · PhaseIndex
       ChainRunHandle · ActiveChainId · MainChainStart · MainChainStartPhaseTag
```

**★ 11 字段去向（实测落地）**：**5 上提基底** + **6 留本域** = **11 ✓**
（上提者：`Entity` / `Level` / `ParamSnapshot` / `RunSource`→`CascadeAnchor` / `PhaseExpiryEntry`→`ExpiryEntry`）

**★ 两处用户裁定（2026-10-14 开工时）**：

| # | 议题 | 裁定 | 判据 |
|---|---|---|---|
| 1 | 基底 `DefTag` 在技能侧**恒空**怎么办 | **填上 + 8 处读取点改读自己**，**但保留"条目撤销 ⇒ 不发终止事件"那处守卫** | 值等价、行为中性；而"条目撤销⇒不发事件"是**有意**语义，去掉即行为变更 |
| 2 | 记录改名后**句柄**跟不跟 | **一并改名**，**方法/成员名也一并改** | **彻底一词一义** |

> **★ 裁定 1 的实施细节（MUST 读，防误读为"只填了个字段"）**：
> 8 处原本从账本条目取定义身份（`Entry->DefTag`）改为读**运行态自己**（`Run->DefTag`）。
> **但 `Entry` 作为"存在性门禁"的用法全部保留**——5 处形如 `Entry ? GetRegisteredSkillDef(...) : nullptr`
> 的写法**只换了取值来源、没撤门禁**；载荷装配处的 `if (Entry)` 守卫**原样保留**。
> ⇒ **"条目已被撤销 ⇒ 不广播终止事件"这一语义逐字不变**（装置 `SkillCast.Reject` 16/0 与
> `SkillLayer.Run` 的 C2/C3 打断/顶替读数逐字相同即证）。
> **一处连带**：`StartMainChain` 的 `Entry` 形参因此在改读自己后**零消费者**
> ⇒ 按"零消费者不预建"**删除该形参**（含 `.h` 声明与 2 处调用点 + `@param` 文档同步）——
> **MUST NOT** 留死形参（会触发未引用形参警告，破坏零 warning 门槛）。

**★ 一处过期描述订正**：计划原文把「门禁④ 改走共享决策接口」列为 Task 3 的 Step 并称"本重构的实质收益点"
——**该收益点已在 Task 1 落地**（`git log -S` 唯一命中 `0ac43b6`）。现状只剩"档位→策略绑定"与"决策结果消费"
（计划允许保留的两件事）。⇒ 本 Task **零改动**，已在该 Step 就地标注。

**★ 改名规模（可复算，TCS）**

| 层 | 内容 | 量 |
|---|---|---|
| 类型名 | `FTcsCastRun` / `FTcsCastRunHandle` / `FTcsCastRunTag` | 78 / 120 / 11 处 |
| 方法 / 成员 | 12 个标识符（`GetCastRun` / `CastRuns` / `FinishCastRun` / `TerminateCastRun` / `ForEachCastRunForGC` / `ResolveRun` / `CountRuns` / `FindActiveRun` / `TerminateRun` / `CastRunGC` / `ClearActiveRuns` …） | 约 150 处 |
| 文件 | 2 个头 `git mv` + 删除 | 2 |
| 文件路径引用 | `#include "Skill/TcsCastRun.h"` 一类 | 含在类型名内 |

**改动规模**：TCS **28 文件**（26 改 + 2 删 + 2 新增 = 净 +0 文件），LAC **6 文件**（装置跟随改名）。

**★ 装置回归（判据 = 与 Task 2 基线逐字相同）**

| 装置 | 读数 | 判定 |
|---|---|---|
| `SkillCast.Run` | Passed=**26** Failed=0 在册 run=8 | ✅ |
| `SkillCast.Reject` | Passed=**16** Failed=0 在册 run=0 条目=6 | ✅ |
| `SkillCast.Phase` | `Phase` **29/0** + `PhaseDeferred` **34/0** + `PhaseEnterDeferred`（P9 三段齐） | ✅ |
| `SkillParamChain.Run` / `.Reject` | **26/0** / **9/0** | ✅ |
| `SkillLayer.Run` / `.Reject` | **24/0**（`[复现性摘要]` 逐字相同）/ **7/0** | ✅ |
| `CooldownCost.Run` / `.Reject` | **33/0**（含跨帧尾段）/ **7/0** | ✅ |
| `State.Reject` | 通过 **6** / 失败 **1** | ✅（基线同值） |
| `Slice.Run` | 通过 **3** / 失败 **3** | ✅（基线同值，PIE-only 既有边界） |

**★ 对比方法（MUST 读，防后人误判为"放过差异"）**：
逐字对比 = **行为信号行**（`LogTcs*` 前缀）**归一后** `Compare-Object`，归一两类：

1. **环境噪声**：时间戳 / PID / CrashGUID / ini 加载顺序与耗时 / 内存数 / 日志路径；
2. **帧驱动量**（依本仓既有读法纪律）：`轮询拍数=N`、`轮询 N 拍`、战斗时钟读数、发号器 `Id` / `Owner` /
   `来源` 数值、槽位 `A/B`、浮点读数、**时钟泵触发条数**。
3. **改名产物**：`TerminateCastRun` → `TerminateSkillInstance`（装置日志文本内嵌函数名）——
   **非行为差异**，已归一后比对。

**★ 三处如实登记的边界（MUST NOT 读成已验）**：

| # | 边界 | 实况 |
|---|---|---|
| 1 | **基线选型陷阱** | Task 2 的 `03-SkillCast.Phase.log` 是**截断版**（P9 段未执行、`P9` 命中 **0**），完整版是 `03b`。**若按 `03` 作基线会误判"本轮多了 16 行 = 行为差异"**。⇒ 基线 MUST 取 `03b`；**这已是同一坑的第二次**（Task 2 首跑即因等待不足截断） |
| 2 | **时钟泵条数差** | `SkillLayer.Run` 泵触发 39→37 条、`CooldownCost.Run` 164→163 条。**定性 = 帧驱动**：两轮"条件成立"行号差恰为 2、总行数差亦为 2 ⇒ **停帧时刻差**，非行为差。**判据**：泵的最后一条 `DueTime` 分别 2.0399 / 2.0104（≈一拍） |
| 3 | **一处措辞订正的有意字符变化** | `TcsCastOps.cpp` 反引号数 594→592：原文"宿主自存 `RunSource`"改为文字表述（该字段名已不存在）⇒ **有意**，非损坏 |

**★ 一处真实的过程缺陷（如实登记，MUST 读）**：
本轮机械改名**中途出过一次文件损坏** —— 我用的 PowerShell 辅助函数在**只传一对替换**时参数被拍平，
`$pair[0]` 退化取到**单个字符**，于是把 `TcsCastOps.h` 的 **1075 个星号**换成空格、
`TcsParamChain.h` 的 **65 个左括号**换成「或」、`TcsNumericParamModifier.h` 的 **290 个反引号**换成 `F`。
**检出方式** = 逐文件"成对字符计数 vs `HEAD`"（星号 / 反引号 / 括号）。
**处置** = 三文件按 `HEAD` **逐字节还原**后，改用**脚本文件 + 命中次数护栏 + 字符不变量核对**重做；
最终全量核对仅剩 1 处反引号差（即上表边界 3，**有意**）。
**⇒ 纪律（本仓首次登记）**：**批量文本替换 MUST 走脚本文件，MUST NOT 走 shell 内联**；
每次替换后 **MUST 核对"成对字符计数守恒"**（`*` / 反引号 / 括号），**MUST NOT 只看替换目标词是否命中**。
**根因** = shell 参数展开把"一元素数组"与"字符串"混同（`MEM-20261013-09` 的同族：
**按表面形态推断数据结构**）。

**★ 规格面处置（走路径 B，依 Global Constraints 的「规格面处置」条）**

**选路判据**：本 Task **不新建能力**、**不改变对外语义**——只在既有能力内部**换实现形状**
（类型改名 + 字段归属迁移）⇒ **路径 B（批处理，集中到 Task 5）合法**。

**待回写清单（实测，MUST NOT 只写"规格待同步"）**：主规格中引用旧类型名的**14 个能力、52 处**：

| 类型名 | 处数 | 说明 |
|---|---|---|
| `FTcsCastRun` | **31** | **该类型已从代码中消失**（被 `FTcsSkillInstance` 取代） |
| `FTcsStateInstance` | **21** | 类型仍在，但**语义已变**（从"装 14 字段"变为"基底 6 字段"） |

**按能力分布**（处数降序）：`skill-cast-runtime` **13** · `entity-runtime-mechanism` 7 ·
`state-instance-lifecycle` 6 · `state-modifier-materialization` 5 · `state-stacking-policies` 4 ·
`skill-cast-phases` 4 · `skill-param-chain` 3 · `skill-layer-e2e-validation` 2 ·
`skill-registry` 3 · `effect-interpreter` 1 · `effect-trigger` 1 · `state-behavior-fragments` 1 ·
`state-param-snapshot` 1 · `skill-cooldown` 1。

**★ 其中最重的一处（Task 5 起草时 MUST 优先处理）**：`skill-cast-runtime` **逐条写死了**
`FTcsCastRun` 非反射 + 必须经 `TTcsInstancePool` + **"MUST NOT 新建池实现"** + **8 个必具字段**
——四句全部需要改述（新形状 = 基底 + 派生，池经 Core 的统一桶）。该能力的清单**计划 Task 5 Step 1 已预列**。

**两仓零提交**（提交须用户显式授权）。

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
   │      └─→ Task 3（Cast payload 化 ✅ + 门禁④ 改共享决策 ✅[Task 1 已交付]）┘ ← "技能运行态归入 State"在此完成
   │                                                            │
   │                                                     Task 8（阶段 3：TcsAssetButler + 四处搬迁）
   │                                                            │
   └────────────────────────────────────────────────────────────┴─→ Task 5（规格面）→ Task 6（证据重取）→ Task 7（收束）
```

**关键路径** = Task 1 → 2 → 4 → **8** → 5 → 6 → 7。**Task 2 与 Task 3 可并行**（payload 各自独立、共享基底已定）——**两者均已交付**。
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
- 2026-10-14 **Task 2 开工前置（§0.5.2 三个固定动作，第一次真的按"每个 Task 各跑一遍"执行）**：
  动作 A 把在册 **75 条**逐条过完（结论 = **本 Task 不消费、不触发任何条目**；三条擦边者 `STAT-8` / `MECH-1` / `WAIT-13` 逐条判定**不成立**）；
  动作 B 现场日 = **2026-10-14**，与轮次日一致，**无跨零点错位、无需留痕**；
  动作 C 出了**逐字段护栏表**（6 行）。
  **★ 抓到两处实质缺口（均在纸上，未动代码）**：
  ① **基底字段表不全**：Task 1 Step 1 的表只表态 **11** 个字段，而 `FTcsStateInstance` **实有 14 个**
  ⇒ **`Handle` / `CascadeAnchor` / `Instigator` 无归属**。**其中 `Source` 行写错了**——它把状态侧 `Source`（**施加方来源、明文 MUST NOT 当撤销锚点**）
  与技能侧 `RunSource`（**就是撤销锚点**）当同义 ⇒ 两者**角色相反，MUST NOT 合并**。
  **根因 = 按名字相邻推断语义相同**（`MEM-20261013-09`）——**这是同一根因当日的第三次**（Task 1 §0.5.2 初稿为第二次）。
  ② **§0.5.2 的体例债表漏登记 3 个文件**，且**两个正面波及 Task 2 的都在漏登记之列**
  （`TcsDevSliceRig.cpp` 3552 行 / `FTcsStateInstance` 34 处；`TcsDevSliceRig_State.cpp` 803 行 / 8 处）
  ⇒ **Task 2 的 `Files` 段原缺宿主装置侧**，实测波及**约 80 处调用点**，已补入。
  **根因** = 该表被"**哪一轮建的**"绑住，而它本该"**每个 Task 现场重新枚举**"
  （`MEM-20261010-08` 的症状换了载体）⇒ 处置 = **纪律升级为"按 Task 重枚举"**（依 `MEM-20261006-08`，**不另立同族卡**）。
  **⇒ 新增三个待裁点**（见 Task 2《开工前置》末段）：① `Source` 行订正；② `Phase` 在技能侧**零消费者**、进不进基底；③ 装置侧 80 处调用点的处置（**正面波及、不可绕开**）。
- 2026-10-14 **Task 2 三个待裁点已裁定 + 新增动作 D**（用户拍板）：
  **① 基底只放「撤销锚点」（`CascadeAnchor`）、不放「施加方来源」（`Source`）**——两域共有的是前者
  （技能 `RunSource` / 账本 `LearnSource` 同角色），而后者两域**角色相反**（状态侧明文"**MUST NOT 当撤销锚点**"）。
  **Task 1 的表两处都错**：`Source` 不该进基底 + `CascadeAnchor` **漏了**。**根因 = 按名字相邻推断语义相同**（`MEM-20261013-09`，**当日第三次**）。
  **② `Phase` 留状态侧、不进基底**——它守的是"同一条实例别被移除两次"，而技能侧同类问题**已由 Task 1 统一的重入守卫解决** ⇒ 技能侧零消费者。
  **③ 新立 §0.5.2 动作 D「代码文件超限时走三段式」**（★ 跨任务通用，用户原话：
  「**先执行重构，然后留档，在测试没问题后再拆分**」）⇒ ① 该改就改（**MUST NOT** 因会超限而绕道）；
  ② 增量写进实施记录，可核项 = **增长量**（**MUST NOT** 再写"零增长"）；③ 拆分**不是禁止**、是**时机固定在验收之后**。
  **★ 该动作补上了 Task 1 裁定缺的另一半**：Task 1 的"MUST NOT 顺手拆分"管的是"这一轮不做"，
  动作 D 补上"**不是无限期禁止、时机 = 测试通过后**" ⇒ **今后遇超限不必再逐次问"拆还是绕"**。
  **同批落纸**：**14 字段最终归属表**（基底 **6** / 状态侧独有 **4** / Buff payload **4** = 14 ✓），订正 Task 1 的旧表并补全它漏的 3 个字段。
- 2026-10-14 **Task 2 落地（第六笔之后的执行）**：六个 Step 全部完成（见 Task 2 末尾《实施记录》）。
  **★ 两处形状由用户在本 Task 开工时改定**（取代计划原文）：① **不单开 payload 类型**（字段直接平铺进派生结构——
  已选继承后再包一层只多一层间接、不增加隔离）；② 派生类型名 = **`FTcsBuffInstance`**、基底 **`FTcsStateInstance` 保留原名**
  （与 **Def 层同构**；**MUST NOT 含 `Entity`** ——它在本仓专指"战斗实体"，而基底装的是"实体身上的运行态"）。
  **同批记录一处我自己的错名**：初稿拟 `FTcsEntityRuntime`/`FTcsBuffRuntime`，**正好撞上 `Entity` 的专指**，
  属当天刚做完 Entity 词汇归一后**同族错名复发**（判据见 `DEC-2026-10-14-entity-vocabulary`）。
  **落地要点**：14 字段 = 基底 6 + 域内 4 + Buff 专属 4；**桶/注册表载荷 MUST 改绑 `FTcsBuffInstance`**
  （绑基底会把层数/时值**静默丢掉**——池按值存，属最难查的一类）；改动 23+1 文件（TCS，+406/−158）与 5 文件（LAC，+52/−52）。
  **门禁**：双配置 **0 error / 0 warning**；无头装置 **11 条读数逐字相同**。
  **★ 两处如实登记的边界**：① 首次跑 `SkillCast.Phase` 等待不足、**P9 段未执行**即被 Kill ⇒ 读数不完整，重跑补全
  （**教训**：有两拍落地段的装置，MUST 先确认该段真的出现过）；② **状态侧实例的 ARO 分支在无头下不可达**
  （三次阴性对照全空转，根因 = GC 时状态侧无在册实例）⇒ 该项**未闭合**、记为边界，MUST NOT 读成已验。
  **★ 动作 D 留档**：两个正面波及的超限 Rig 文件实测**零增长**（3552 / 803 不变）——因字段平铺使访问写法不变、改名是等长替换。
- 2026-10-14 **Task 3 开工前置（§0.5.2 四个固定动作；动作 A/B/C 现场执行，动作 D 为新增）**：动作 A 把在册 **74 条**逐条过完（结论 = **本 Task 不消费、不触发任何条目**；三条擦边者 `CHAIN-7` / `SCRIPT-10` / `MECH-1` 逐条判定**不成立**）；动作 B 现场日 = **2026-10-14**，与轮次日一致、**无跨零点错位**；动作 C 出**逐字段护栏表**（11 行 + 句柄改名护栏）；动作 D 留档 **无新增超限文件**、正面波及 5 个超限文件（4 个等长替换 + `TcsCastOps.cpp` **+1**）。**★ 与 Task 2 的关键差别（MUST 读）**：Task 2 那次"漏登记"是**正面波及的文件未列入**，本次**正面波及的 5 个超限文件全部检出** ⇒ **上一轮立的"按 Task 重枚举"纪律第一次真的拦住了**。**一处极易误判处已就地登记**：`TcsDevSliceRig.cpp` 的 46 处 `RunSource` 命中是**链侧** `FTcsEffectContext::RunSource`（该字段仍在、名字不变）——判据 = **逐处核对字段归属，MUST NOT 按词形命中计数**。
- 2026-10-14 **Task 3 落地**：七个 Step 全部完成（见 Task 3 末尾《实施记录》）。**★ 两处形状由用户开工时改定**：① **不单开 payload 类型**（`TcsCastPayload.h` 不创建，字段直接平铺进 `FTcsSkillInstance`）；② **彻底一词一义**——记录 / 句柄 / 方法 / 成员名**一次改到底**。**★ 两处用户裁定**：① 基底 `DefTag` 在技能侧**填上 + 8 处改读自己**，**但 `Entry` 的"存在性门禁"用法全部保留**（5 处 `Entry ? … : nullptr` 只换取值来源、载荷装配的 `if (Entry)` 守卫原样保留）⇒ **"条目撤销 ⇒ 不发终止事件"语义逐字不变**；一处连带 = `StartMainChain` 的 `Entry` 形参因此**零消费者** ⇒ 删除（含 `.h` + 2 调用点 + `@param`），**MUST NOT** 留死形参；② 句柄与方法名一并改。**★ 一处过期描述订正**：「门禁④ 改走共享决策」**已在 Task 1 落地**（`git log -S` 唯一命中 `0ac43b6`），本 Task **零改动** ⇒ 该 Step 状态改"已由 Task 1 交付"，**MUST NOT** 再当作 Task 3 的待办；**同批订正一条判据的归属**：原验收信号"`TcsCastOps.cpp` 内零内联 Instancing 分流"**已在 Task 1 满足**。**落地要点**：11 字段 = **5 上提基底**（`Entity` / `Level` / `ParamSnapshot` / `RunSource`→`CascadeAnchor` / `PhaseExpiryEntry`→`ExpiryEntry`）+ **6 留本域**；改动 TCS **28 文件**、LAC **6 文件**。**门禁**：双配置 **0 error / 0 warning**；无头装置 **11 条行为信号逐字相同**；`validate --all --strict` TCS **43/0**、LAC **4/0**。**★ 三处如实登记的边界（MUST NOT 读成已验）**：① **基线选型陷阱**——Task 2 的 `03-SkillCast.Phase.log` 是**截断版**（`P9` 命中 0），完整版是 `03b`，按 `03` 作基线会误判"本轮多 16 行 = 行为差异"（**同一坑的第二次**，首次即 Task 2 首跑等待不足）；② **时钟泵条数差**（39→37 / 164→163）定性 = **帧驱动**，判据 = 两轮"条件成立"行号差恰为 2、总行数差亦为 2；③ **一处有意字符变化**（`TcsCastOps.cpp` 反引号 594→592，因字段名已不存在而改措辞）。**★ 一处真实的过程缺陷（本仓首次登记，MUST 读）**：机械改名中途**出过一次文件损坏**——PowerShell 辅助函数在**只传一对替换**时参数被拍平、`$pair[0]` 退化取到**单个字符**，于是 `TcsCastOps.h` 的 **1075 个星号**→空格、`TcsParamChain.h` 的 **65 个左括号**→「或」、`TcsNumericParamModifier.h` 的 **290 个反引号**→`F`（三文件共 **1430 个字符**）。**检出方式** = 逐文件"成对字符计数 vs `HEAD`"；**处置** = 按 `HEAD` **逐字节还原**后，改用**脚本文件 + 命中次数护栏 + 字符不变量核对**重做。**⇒ 立纪律**：**批量文本替换 MUST 走脚本文件，MUST NOT 走 shell 内联；每次替换后 MUST 核对"成对字符计数守恒"（`*` / 反引号 / 括号），MUST NOT 只看替换目标词是否命中**。**根因** = shell 参数展开把"一元素数组"与"字符串"混同（`MEM-20261013-09` 同族：**按表面形态推断数据结构**）。**★ 规格面处置（走路径 B，依 Global Constraints 的「规格面处置」条）**

**选路判据**：本 Task **不新建能力**、**不改变对外语义**——只在既有能力内部**换实现形状**
（类型改名 + 字段归属迁移）⇒ **路径 B（批处理，集中到 Task 5）合法**。

**待回写清单（实测，MUST NOT 只写"规格待同步"）**：主规格中引用旧类型名的**14 个能力、52 处**：

| 类型名 | 处数 | 说明 |
|---|---|---|
| `FTcsCastRun` | **31** | **该类型已从代码中消失**（被 `FTcsSkillInstance` 取代） |
| `FTcsStateInstance` | **21** | 类型仍在，但**语义已变**（从"装 14 字段"变为"基底 6 字段"） |

**按能力分布**（处数降序）：`skill-cast-runtime` **13** · `entity-runtime-mechanism` 7 ·
`state-instance-lifecycle` 6 · `state-modifier-materialization` 5 · `state-stacking-policies` 4 ·
`skill-cast-phases` 4 · `skill-param-chain` 3 · `skill-layer-e2e-validation` 2 ·
`skill-registry` 3 · `effect-interpreter` 1 · `effect-trigger` 1 · `state-behavior-fragments` 1 ·
`state-param-snapshot` 1 · `skill-cooldown` 1。

**★ 其中最重的一处（Task 5 起草时 MUST 优先处理）**：`skill-cast-runtime` **逐条写死了**
`FTcsCastRun` 非反射 + 必须经 `TTcsInstancePool` + **"MUST NOT 新建池实现"** + **8 个必具字段**
——四句全部需要改述（新形状 = 基底 + 派生，池经 Core 的统一桶）。该能力的清单**计划 Task 5 Step 1 已预列**。

- 2026-10-14 **规格面处置条款订正（★ 用户提问驱动的缺口暴露，MUST 读）**：用户问「**Task 3 的执行没有创建提案吗**」——实测 **Task 2 与 Task 3 都没开提案**（`openspec/changes/` 零活动提案；而 **Task 1 那次是真开了的**：`merge-entity-runtime-mechanisms`，含新建能力 `entity-runtime-mechanism` + 归档）。**根因 = 计划自相矛盾**：Global Constraints 原文写「**每个 Task** 的实施以 OpenSpec 提案开道」，而 **Task 5（规格与能力面回写）** 的设计是**批处理**——两者不可能同时成立。**后果 = 静默跳步**：Task 2 / Task 3 两轮既没开提案、也没记录这个决定（**没有任何检查会因它失败**）。**订正** = 把该条改为**两条合法路径 + 选路判据**（新建能力或改对外语义 ⇒ A；既有能力内换实现形状 ⇒ B），并给路径 B 加硬要求（《实施记录》MUST 写明选了 B 及判据 + **待回写能力清单与处数**）。**依据** = 两条路径**在本计划内本来就都存在**（Task 1 走 A、Task 5 走 B），缺的只是把它们命名出来并给判据。**★ 同批补记未偿欠账（实测，MUST NOT 只写"规格待同步"）**：主规格中引用旧类型名的 **14 个能力、52 处** —— `FTcsCastRun` **31 处**（**该类型已从代码中消失**）+ `FTcsStateInstance` **21 处**（类型仍在、**语义已变**：从"装 14 字段"变为"基底 6 字段"）；按能力分布最重者 = `skill-cast-runtime` **13**（它逐条写死了 `FTcsCastRun` 非反射 + 必须经 `TTcsInstancePool` + "MUST NOT 新建池实现" + 8 个必具字段 —— **四句全部要改述**）。该清单已同时写进 **Task 2 / Task 3 的《实施记录》**，归 **Task 5** 消费。**两仓零提交**（提交须用户显式授权）。**下一步 = Task 4（拆出 `TcsBuff` 新模块）**；⚠️ **Task 3 遗留一处未闭合**：`PLN-R5.5` 的 `e1`/`e4` 改述（Step 7 只改述状态、正文未动）。
