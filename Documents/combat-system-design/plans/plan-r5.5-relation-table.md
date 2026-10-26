# R5.5 实施计划：关系表族语义与检查器（批次 `e`）

- **文档 ID**：`PLN-R5.5`
- **类型**：PLN / 计划
- **状态**：ACTIVE（2026-10-13 建立；**实施未开工**——本计划是 `R5.5-e` 的开工收窄轮产物，Task 0 已由 `DEC-2026-10-13-relation-semantics` 完成）。**★ 2026-10-13 第三笔：`e1`/`e4` 被 `DEC-2026-10-13-state-module-positioning` 前置**（该裁决裁定运行态基底重构先行）⇒ **本计划 Task 1 / Task 4 的开工前提已变**，`e2`/`e3` 不受影响。**★★ 2026-10-15 更新：该前置已解除**——`PLN-REFACTOR-1` **全轮完成**（Task 1~8 落地）⇒ **`e1`/`e4` 可按新基底重写后开工**（新旧名对照见下方《最后更新》与 Task 1 Step 7 的追注）
- **权威范围**：`R5.5-e` 批次的执行形状（怎么做、怎么验）——字段语义落地与检查器 / 类目降维层 / 移除级联重评 / 槽位竞争表四批的 Task 切分、验收信号与边界。**不在本文**——《R5.5 批次表》本体（住 `PLN-R5`）、`R5.5` 其余批次 `a`~`d` / `f`~`h` 的实施（各自归属不变）、状态槽映射选型与工具排期（见 `DEC-2026-08-31-state-relation` §7 另两框）
- **最后更新**：2026-10-16（**★ `e0`（新增前置批）已落地并归档**——原「第六项」（`StatusTag` → `CategoryTags` 合并）经用户 2026-10-16 裁定**拆为独立前置 `e0`**，现已完成：字段合并 + 6 资产迁移 + 选择器侧消歧改名 + 双配置编译 0/0 + **PIE 四轮全绿**（`State.Run` 15/0 ×2 复现、`Reject` 7/0、`Slice.Run` 73/0；检查 18 新判据实测 `CategoryTags 含定义词=是`）。**变更 `merge-status-tag-into-category-tags` 已归档**（`openspec/changes/archive/2026-10-16-…`）、`validate --all --strict` = **44 passed / 0 failed**、`changes/` 零活动提案。**台账 `STAT-10` 并入 `STAT-4`**（75 → 74 条）。**⇒ 下接 = Task 1（`e1` 关系表检查器）**，其范围已较原计划扩大（用户 2026-10-16 裁定「一步到位」）：关系表检查器 + 跨域运行态访问契约位（N1b 双向注册）+ 技能侧「按实体枚举在飞运行态」查询口 + `TagsToBlock` / `TagsToCancel` 双向 + `ECFR_Interrupted` → `ECFR_Canceled` 一族改名（含 `IsInterruptibleNow()` → `CanBeCanceled()`、`bInterruptible*` → `bCancellable*`、`ECT_OnCastInterrupted` → `ECT_OnCastCanceled`）+ 技能载荷补 `Reason` 字段 + 技能侧 `CategoryTags` 校验补缺。**征地**：`EVID-2026-10-16-merge-status-tag`。**前次最后更新（2026-10-15）**：（**前置解除**：`PLN-REFACTOR-1` **全轮完成**（Task 1~8；见 [该计划](plan-refactor-1-runtime-base.md)）⇒ 本文 Task 1（`e1`）/ Task 4（`e4`）的**开工前提已满足**，可按**新基底**重写后开工。**重写时的两处新旧名对照**：① 施法运行态 = **`FTcsSkillInstance`**（~~旧名 `FTcsCastRun`~~ 已从代码中消失）；② 状态/Buff 运行态 = **`FTcsBuffInstance`**（基底 `FTcsStateInstance`），域侧实现名 = **`FTcsBuffRegistry`** / **`FTcsBuffOps`** / **`UTcsBuffSubsystem`**（原 `FTcsStateRegistry` / `FTcsStateOps` / `UTcsStateSubsystem`）。**本追注不改 Task 拆分与 Step 草案**（其判据面不受重构影响，受影响的只有名与落点）。）**前次最后更新（2026-10-13）**：（**第三笔：被 `DEC-2026-10-13-state-module-positioning` 前置**——用户同日提出架构质疑（`TcsState` 是否混入 Buff 专属内容）⇒ 裁定运行态**基底 + 两 payload**、**Buff 专属语义拆出为新建模块 `TcsBuff`**（`TcsState` 保留为共享层）、**本重构先行**。**本计划 Task 1（`e1`）与 Task 4（`e4`）的产出物明文绑在 `FTcsCastRun` 上**（Task 1 Step 7"主体 = `FTcsCastRun`"、Task 4"主体 = 施法运行"）⇒ 按裁定⑧ **`e1`/`e4` 须按新基底重写**，MUST NOT 按现形状开工。**Task 2（`e2`）/ Task 3（`e3`）不受影响**（前者是类目声明面、后者是级联重评）。详见该裁决 §5.1、§1.5 与 §6（同日修正）。**前一笔（2026-10-13）**：**第二笔：吸收 Lyra 行形态**——用户提出 `ULyraAbilityTagRelationshipMapping` 先例 ⇒ 开取代裁决 [`DEC-2026-10-13-relation-row-shape`](../decisions/dec-2026-10-13-relation-row-shape.md)（取代前文裁决②③），本计划同步：**行形态 = 一行一个主体词 + 四容器**、**匹配机制 = 容器 + `MatchesTag` 层级匹配（不用 `FGameplayTagQuery`）**、**字段改名 `AppliesToTag`/`RequiredTags`/`ForbiddenTags`/`TagsToBlock`/`TagsToCancel`**、**撤销一条 Error 规则**、**新增 `TagsToBlock` 预防能力**。**★ 同日第一笔**：建立——Task 0（语义拍板）完成 + `e` 拆 `e1`~`e4`）

**Goal:** 让关系表族的字段**第一次有真实消费者**——施加态与施法态的共存冲突有确定、可配、可复现的结果，且冲突的**事实源唯一**（不靠补丁顺序、不靠遍历顺序）。**★ 2026-10-13 第二笔后，字段集 = 四个容器（见 §0.2 Q-7）；"三个字段"是首轮的措辞，已被取代。**

**Architecture:** 关系表族住 `TcsState`（施加态语义：`FTcsBuffDef`）+ `TcsSkill`（施法语义：`FTcsSkillDefData`），两族**共用同一检查器**（主体类型化：状态实例 / 施法运行）。载体 = **中心 `UDataAsset`（Lyra 行形态：一行一个主体词 + 四容器，兑现 `D3-16` 既有的"统一 DataAsset 行"）** + **`FGameplayTag::MatchesTag` 层级匹配**（**不用 `FGameplayTagQuery`**，见 §0.2 Q-3）；**零新模块、零新 tag 根**（词归宿主）。

**Tech Stack:** UE 5.8 C++、GameplayTags（宿主 ini 词 + **容器 + 层级匹配**）、UBT（Development Editor + Shipping 双配置）、宿主侧装置（LAC `Source/TcsDev/`）。

---

## 0. 开工前置（先读：本轮输入、收窄结论、已拍板口径）

### 0.1 本轮输入（逐条给落点）

| 输入 | 原归属 | 本计划落点 | 处置 |
|---|---|---|---|
| `DEC-2026-08-31-state-relation` §7 待办第 2 条（`Priority` 语义 / 互斥组结构 / 族规则表达式） | R5.5-e 第 1 步 | **Task 0**（已完成） | ✅ 消费 → `DEC-2026-10-13-relation-semantics`（裁决①④⑤⑥）+ **同日第二笔取代裁决** `DEC-2026-10-13-relation-row-shape`（裁决②③） |
| `FTcsBuffDef` / `FTcsSkillDefData` 四字段（形状已落、检查器零行） | R5 遗留 | **Task 1 / 2** | 消费 |
| 台账 `STAT-4`（`StatusTag` 的 tag 根归属） | R5.5-e | **Task 1**（检查器是它的第一个解析消费者） | 消费（届时按"一角色一根"定根） |
| 台账 `STAT-10`（类别标识容器 + `ByCategoryTags` 档，三项） | 独立变更 | **Task 2** | 判为**同批裁**（`DEC-2026-10-13-relation-semantics` 裁决③） |
| `EStateRemoveCause::ESRC_Cancelled` 无内建产生者 | R5.5-e | **Task 1**（`Cancels` 胜出路径） | 消费 |
| 打断来源优先级仲裁 | R5.5-e | **Task 4** | 改判至槽位竞争表（非关系表） |
| 《R5.5 批次表》`e` 行（四步一格） | PLN-R5 | **Task 1~4**（重切四批，见 §0.2） | 重切 |

### 0.2 收窄轮的口径裁决（2026-10-13，用户逐项拍板）

> 完整裁决与判据住 [`DEC-2026-10-13-relation-semantics`](../decisions/dec-2026-10-13-relation-semantics.md)（裁决①④⑤⑥ 有效）与 [`DEC-2026-10-13-relation-row-shape`](../decisions/dec-2026-10-13-relation-row-shape.md)（**取代前者裁决②③**：行形态 / 字段命名 / 匹配机制 / 抢占策略表达）。本表只列执行口径。

| # | 议题 | 裁定 | 影响面 |
|---|---|---|---|
| Q-1 | `Priority` 的语义与归属 | **移出关系字段族** → 归**槽位竞争表**（主体 = 施法运行/动作状态，独立表）；与 M2 `OverridePriority` **判为两个无关概念**（裁决键类型不同：M2 是数值比较，本族无数值可比） | 两处 `Priority` 字段的**语义归属**改判；物理去留归 Task 4。**★ 2026-10-13 加强**：Lyra 亦无 `Priority`，其顶替仲裁住 ASC 侧 `ActivationGroup` ⇒ 两份独立实现同向 |
| Q-2 | 互斥组的结构 | **混合两层**：逐对 = 特例层（Task 1）；类目降维 = 通例层（Task 2）。~~`Blocks` = 退让声明 / `Cancels` = 强夺声明~~ → **★ 2026-10-13 修正配对**：真正的配对是 **`Blocks` / `Requires`**（前置条件的"必须缺席"vs"必须存在"），`Cancels` 与它们**正交**（对外副作用）。字段命名见 Q-7 | `SPEC-02-states` §3.4 / `SPEC-04-skill` §2 |
| Q-3 | 族规则表达式 | ~~类目容器（宿主词汇）+ `FGameplayTagQuery` 对另一侧容器求值 + 一族规则行~~ → **★ 2026-10-13 取代**：**Lyra 行形态 = 一行一个主体词（单 tag）+ 四个容器**；匹配走**容器 + `FGameplayTag::MatchesTag` 层级匹配**，**不引入 `FGameplayTagQuery`**。`STAT-10` 仍**同批裁**（声明面同一个字段、两个消费者） | Task 2；`STAT-10` 三项 |
| Q-4 | `State.Control.*` / `Action.*` | **不建**（Q-3 已绕开开根需求——层级匹配的父词即可覆盖"一族"） | `DEC-2026-08-31` §4 举例作废 |
| Q-5 | `PriorityOnly` / `PreemptionPolicy` | **不作为既有设计照搬**（依据库外不可复核）；`PreemptionPolicy` **不建字段** | Task 4 重新推导 |
| Q-6 | 批次重切 | `e` 拆 **Task 1~4**（`e1`~`e4`），判据见 §0.4 | 原 `e` 行 |
| **Q-7** | **字段命名**（2026-10-13 第二次拍板） | **`AppliesToTag`**（主体词，单 tag）/ **`RequiredTags`** / **`ForbiddenTags`** / **`TagsToBlock`** / **`TagsToCancel`**。**`Blocks` → `ForbiddenTags`（受动/前置），MUST NOT 映射到 `TagsToBlock`（施动/对外）**——两格词形极近而语义方向相反，是**静默语义反转**陷阱 | Task 1 / Task 2；两个 Def 头（改名归 Task 1） |
| **Q-8** | **主体侧匹配面**（2026-10-13） | **`{StatusTag} ∪ CategoryTags`**（甲案）；**不含 `DefTag`**（避免同一语义两条寻址路径）；主体侧 `HasTag(Row.AppliesToTag)` 层级匹配、客体侧 `HasAny(SubjectTags)` | Task 2 |<br>**★ 2026-10-24 订正**：用户裁定 **`DefTag` 与 `CategoryTags` MUST 为并集** ⇒ 最终式 = `CategoryTags` ∪ `{DefTag}`（`StatusTag` 已随 `e0` 删除）。「不含 `DefTag`」与「避免同一语义两条寻址路径」**均作废**——真正的风险是**两条路径**而非并集。见 `DEC-2026-10-13-relation-row-shape` 实施注记。
| **Q-9** | **`TagsToBlock`（预防）是新增能力** | 与 `TagsToCancel`（终止）的分工 = "目标还没起"vs"目标已在跑"；TCS 今天表达不了。**归 Task 1**（它属检查器的特例层能力） | Task 1 Step 5 |

### 0.3 现状证据（2026-10-13 全仓实测，逐条已核）

> 完整证据表（20 条，含锚点）住 [`DEC-2026-10-13-relation-semantics`](../decisions/dec-2026-10-13-relation-semantics.md) §3。本表只列**影响计划的**部分。

| 事实 | 证据 | 对计划的影响 |
|---|---|---|
| 四字段形状已落、**检查器零行** | `TcsBuffDef.h:FTcsBuffDef::Blocks` 等 8 处声明；全仓成员访问点零命中 | Task 1 是**从零造检查器**，不是接线 |
| **外部先例：Lyra 的 `ULyraAbilityTagRelationshipMapping`**（【外部源码】） | 公开镜像 `BulkheadLtd/Lyra_5_4` 的三处文件；**八行机制对照表**见 `DEC-2026-10-13-relation-row-shape` §2 | 同构四项 / **缺失一项（`TagsToBlock`）** / 反向支持一项（无 `Priority`） |
| 类目容器字段**不存在于定义基类** | `TcsStateDefBase.h:FTcsStateDefBase`（六字段无类目） | Task 2 是**造字段**；形状必须在计划里给草稿（§Task 2） |
| `FTcsSkillEntrySelector::CategoryTags` 已落空壳、零消费者 | `TcsSkillEntrySelector.h` 头注释自述"本轮预留字段，零消费者" | Task 2 的**匹配面 B** 已有载体，只缺声明面 |
| `FGameplayTagQuery` 是引擎既有可反射类型 | `GameplayTagContainer.h:FGameplayTagQuery`（`USTRUCT(BlueprintType)` + 编辑器定制） | Task 2 **零新自定义类型的匹配载体** |
| 宿主词汇根先例 = `DamageCategory`（声明方 = 宿主 ini，插件零具体词） | `openspec/specs/gameplay-tag-governance/spec.md`「根段注册表」`DamageCategory` 行 + 其共享说明 | Task 2 **不开新根**的依据 |
| `State.Control.*` / `Action.*` 两根本不存在 | `Source/` 全仓零命中；LAC ini 11 根无此二者 | 原"族规则降维"举例不可直接实施（§0.4 ②） |
| `StatusTag` 今天**零解析消费者** | `gameplay-tag-governance` 根段注册表：「状态词本轮 MUST NOT 为它开根」 | Task 1 落地时该条件**成立**（检查器即解析消费者）⇒ 届时应定根 |
| 槽位竞争语义**库外不可复核** | `spec/03-module-states.md` §3.4 引 `TCS 报告 06`；同文件 §12.4 自我登记"不可复核" | Task 4 须**从判据重新推导**，不得照搬 `PriorityOnly` / `PreemptionPolicy` |
| 打断来源优先级**无载体** | `TcsCastOps.h:FTcsCastOps::CancelCast` 头注释自述 | Task 4 的范围之一 |
| R6 施法运行**已落地**（槽位竞争的主体已在） | `Source/TcsSkill/`（`FTcsCastRun` / 四路终结 / `CancelCast`） | Task 4 **与 Task 1~3 正交**、可独立开工 |
| **外部先例：Lyra 的 `ULyraAbilityTagRelationshipMapping`**（【外部源码】，见取代裁决 §1.2 取证边界） | 公开镜像 `BulkheadLtd/Lyra_5_4`：`.h`（结构定义）/ `.cpp`（三个求值函数）/ `LyraAbilitySystemComponent.cpp`（消费点） | **Task 2 的行形态与匹配机制据此改判**；Task 1 的字段改名据此定案 |
| 该先例的**四处同构**与**一处缺失** | 见 `DEC-2026-10-13-relation-row-shape` §2 的八行对照表 | `TagsToBlock`（预防）是**唯一真新增能力**（Q-9） |
| TCS 活跃提案 0 个、`PLN-R5.5` 原不存在 | `openspec/changes/` 仅存 `archive/`；`plans/` 无 `r5.5` 文件（**本日已建**） | 本计划是**首个**拥有 Task 骨架的 `R5.5` 文档；开提案须新建 |

### 0.4 计划不得建在错前提上的三条更正

> 完整更正（含锚点）住 `DEC-2026-10-13-relation-semantics` §4。此处只记对**排期**的影响。

① **"族规则降维依赖 `State.Control.*` 阻断 `Action.*`"= 依赖两个不存在的根**。机制（N×M 压 K 条）成立，**载体改判**为**宿主词汇类目容器 + Lyra 行形态（一行一个主体词）**（Q-3/Q-4）。⇒ Task 2 **不含任何开根步骤**（主体词填父词即覆盖一族，**层级匹配天然承担"域间"这一层**）。

② **"槽位竞争已验证（`TCS 报告 06`）"不成立**——该报告库外不可复核（`GLOSSARY` §6 第 10 条记该系列全域不存在）。⇒ Task 4 的输入**只有名字**，MUST 重新论证；`PreemptionPolicy` 不建字段。

③ **《R5.5 批次表》`e` 行"四步一格"掩盖了依赖不对称**：
- 第 1+2 步（语义拍板 → 检查器）**串行**；
- 第 3 步（级联重评）**串行于**检查器（要重评的正是它的判定）；
- 第 4 步（槽位竞争表）**与前三步正交**（主体是施法运行，R6 已交付）。

⇒ 按"验收信号纯净"的既有切分传统重切为四批（§0.2 Q-6、§5）。

④ **一处配对错误（本轮自行抓出并更正）**：首轮裁决把 `Blocks` 与 `Cancels` 配成"退让 vs 强夺"一对。**精确回读 `TcsBuffDef.h` 的字段注释**后确认：真正的配对是 **`Blocks` / `Requires`**（前置条件的"必须缺席"vs"必须存在"），`Cancels` 与它们**正交**（对外副作用）。⇒ 连带**撤销**首轮写下的一条 `IsDataValid` 规则（`A.Blocks ∋ B` + `A.Cancels ∋ B` ⇒ 自相矛盾），因按修正后的语义**两者同真合法**、且是一种抢占策略。**根因** = 按词形（Block/Cancel 都是"拦"）而非字段注释配对。**外部对照佐证**：Lyra 把这两个方向**明确分成两个字段**（`ActivationBlockedTags` 受动/前置 vs `AbilityTagsToBlock` 施动/对外）——详见 `DEC-2026-10-13-relation-row-shape` §3.2。

⑤ **一处"本机不可核实"的依赖被去掉**：首轮裁决③ 选了 `FGameplayTagQuery`，而该引擎类型在本机**无法核实**（~~无引擎源码树~~ → **★ 2026-10-13 订正：该前提为假**，引擎源码在 `E:\UnrealEngine\UE_5.8`，`FGameplayTagQuery` **确认存在**于 `GameplayTagContainer.h`）。Lyra 用同一个需求（域间阻断）证明**容器 + 层级匹配**即可 ⇒ 匹配机制改判，连带去掉一个未核实的依赖（取代裁决 §3.1）。**★ 该改判的结论不受本次订正影响**——去掉它的理由是"容器 + 层级匹配已足够"，**不是"它不存在"**。

---

## Global Constraints

- **禁 TDD**（用户级最高纪律）：无失败测试步骤；每任务验证 = UBT 编译通过 + 定向人工检查。
- **提交纪律**：任何 `git commit` 仅在用户明确授权后执行；计划不含自动提交步骤，任务边界即"停点待检查"。**跨仓注意**：插件仓（TCS）与宿主仓（LAC）各自独立提交；**LAC 是混合仓，禁 `git add -A`**，提交一律按显式路径逐个 `add`。
- **风格**：创建/修改 C++ 前执行 `unreal-cpp-style` 技能——Tab 缩进、UTF-8 无 BOM、LF；`.h` 用 region、`.cpp` 扁平；单 `.cpp` ≤300 行；文件头 `// Copyright Tirefly. All Rights Reserved.`。
- **命名**：类型 = 前缀字母 + `Tcs` + 语义名；枚举值 = 枚举名缩写全大写；API 导出宏 `TCS<模块>_API`；动词纪律（`Resolve` 仅用于句柄/Id→对象）。
- **UHT 两条硬门槛**：① **全项目头文件名唯一**（同名不同目录照样失败）；② **反射类型去前缀后不得同名**（`FTcsX` vs `UTcsX` 必炸）。
- **依赖铁律**：`TcsCore ← {TcsNotation, TcsAttribute} ← TcsEffect ← {TcsDamage, TcsTargeting, TcsState} ← TcsSkill`。检查器**共用** ⇒ 住 `TcsState`（两族的共同下层），`TcsSkill` **依赖**它，**MUST NOT 反向**（`TcsState` MUST NOT include `TcsSkill`）。
- **`IsDataValid` 收尾**：无错无警时 MUST 把基类返回的 `NotValidated` 提升为 `Valid`。
- **零消费者不预建**：字段可先就位，但**不得为无消费者的字段造机制**；未落地面 MUST 写进计划注记 + `LEDGER-deferred`。
- **过网结构纪律**：MUST 纯反射数据——禁 `TFunction`、禁 `TMap`/`TSet` 作 `UPROPERTY`；`FInstancedStruct` 内层须可复制。
- **规格先行**：每个 Task 的实施以 OpenSpec 提案开道（`openspec validate <id> --strict --no-interactive` 通过 → 实施 → `openspec archive <id> --yes`）；归档时 MODIFIED 需求的标题 MUST 与主规格逐字一致。
- **tag 治理**：**不开新根**（Q-4）；类目词一律**宿主 ini 声明**，插件 MUST NOT 声明任何具体分类词（先例 `DamageCategory`）；`StatusTag` 定根时按「一角色一根」查表并同步根表行数。
- 设计规格来源：`SPEC-02-states` §3.4（形状与执行时机的唯一权威处）、`SPEC-04-skill` §2（同形状映射）、`DEC-2026-10-13-relation-semantics`（本批裁决）；冲突时以设计文档 + 用户拍板为准。

---

## 勘察（五节之一）

> 本节回答"现状是什么"。完整证据表住 `DEC-2026-10-13-relation-semantics` §3（20 条锚点），此处给**结论级**摘要。

### A. 什么是形状（已落）

- **状态侧**：`FTcsBuffDef` 四字段——`Blocks` / `Requires` / `Priority` / `Cancels`，其中前两个与第四个是 `TArray<FGameplayTag>`、`Priority` 是 `int32`。
- **技能侧**：`FTcsSkillDefData` 同四字段、同类型（注释自述"与状态族同形"）。
- **执行时机已有规格**（`SPEC-02-states` §3.4）：apply 时校验；**任何状态移除后对依赖它的存活状态做事件驱动重评**（脏标记，批量移除合并一次）。
- **载体侧已有空壳**：`FTcsSkillEntrySelector::CategoryTags`（`FGameplayTagQuery`，零消费者）。

### B. 什么是空的

- **关系字段检查器零行**——四字段**零消费者**（"形状已落" ≠ "语义已定"）。
- **类目容器字段不存在于定义基类**（`FTcsStateDefBase` 六字段无类目）。
- ~~**族规则行不存在**~~ → **关系行载体不存在**（承载"一行一主体词 + 四容器"的中心 DataAsset 与行结构都没有；`D3-16` 只写了它"应该长这样"）。
- **`State.Control.*` / `Action.*` 两根不存在**。
- **打断来源优先级无载体**。

### C. 什么是库外不可复核的

- **槽位竞争语义**（`PriorityOnly` / `PreemptionPolicy` / 同优先级策略）的唯一依据 `TCS 报告 06` 库外、已不在库内 ⇒ **今天只有名字**。

### D. 现状事实对排期的影响

1. Task 1 / 2 是**造机制**（不是接线），形状必须在计划里给草稿；
2. Task 2 **不含开根步骤**（Q-4）；
3. Task 4 的输入须**重新论证**（`TCS 报告 06` 不可复核）；
4. Task 4 **与 Task 1~3 正交**（主体 R6 已交付）。

---

## 口径裁决表（五节之二）

> 裁决本体住 `DEC-2026-10-13-relation-semantics` §0 / §2；本节按**执行视角**给"做什么、影响谁"。

| # | 议题 | 裁定 | 判据（一句话） | 影响面 |
|---|---|---|---|---|
| **A1** | `Priority` 归属 | 移出关系字段族 → 槽位竞争表 | 原裁决自己写的是"**抢同一槽**时"（`DEC-2026-08-31` §4 第 2 条），且 `SPEC-02-states` §3.4 明写两表主体不同 | 两处 `Priority` 语义改判；物理去留归 Task 4 |
| **A2** | `Priority` 与 M2 的关系 | **两个无关概念** | 裁决键**类型不同**：M2 是"优先级 → 打平按策略**比数值**"（三级阶梯），本族候选**无数值可比** | 禁止复用 `ETcsAttrOverrideTieBreak`；仅沿用"大者胜"方向约定 |
| **A3** | 同优先级决胜 | **本轮不裁**，归 Task 4 | 属槽位竞争的问题；今天裁会造出两个互相依赖的悬空裁定 | Task 4 |
| **A4** | 互斥组结构 | **两层**：逐对（特例，Task 1）+ **中心 DataAsset 行（通例，Task 2）** | `DEC-2026-08-31` §4 第 1 条的降维对策保留、载体改判为 **Lyra 行形态** | `SPEC-02-states` §3.4 / `SPEC-04-skill` §2 |
| **A5** | ~~`Blocks` vs `Cancels`~~ → **配对更正** | **同轴者 = `RequiredTags` / `ForbiddenTags`**（前置条件的"必须存在"vs"必须缺席"）；**`TagsToCancel` 与它们正交**（对外副作用） | 前者描述"我的施加**条件**"（失败 ⇒ 拒），后者描述"我的施加**副作用**"（成功 ⇒ 清）。**★ 首轮误把 `Blocks` 与 `Cancels` 配成一对，已更正** | 检查器两路分支；`ESRC_Cancelled` 的产生者 |
| **A6** | 真冲突判定 | **两条**作者期规则（**原三条中的第一条已撤销**）：① 双向 `TagsToCancel` = 无确定赢家 ⇒ Error；② `ForbiddenTags` 自指 ⇒ 永不可满足 ⇒ Error | **★ 撤销原"`A.Blocks∋B` + `A.Cancels∋B` 同真 = 自相矛盾"**——按修正后的语义两者同真**合法且是一种抢占策略**（见 A14） | `IsDataValid` 两条 Error |
| **A7** | ~~族规则表达式（Query × Query）~~ → **关系行形态** | **Lyra 行形态**：一行 = **一个主体词（单 `FGameplayTag`）+ 四个容器**；载体 = 中心 `UDataAsset`；匹配 = 主体侧 `HasTag(AppliesToTag)` **层级匹配** + 客体侧 `HasAny` | **★ 2026-10-13 取代**：Lyra 用同一需求证明**容器 + 层级匹配**即可（主体词填父词 ⇒ 覆盖一族），**不需要 `FGameplayTagQuery`** | Task 2 |
| **A8** | `STAT-10` | **同批裁**（同一套机制的两半） | 声明面同一个字段、两个消费者（关系行 + 条目选择器）。**只换匹配机制，不换该判断** | Task 2 含 `STAT-10` 三项 |
| **A14** | **抢占策略的表达** | **"填哪几个字段"即策略**：`TagsToCancel` 单独 ⇒ 后到者赢；`ForbiddenTags` + `TagsToCancel` ⇒ 在位者赢 | 两者同真**合法**（撤销原 Error 规则）；**不需要 `Priority` 数字** | Task 1 Step 6 的检查时机 + 验收 ⑥ |
| **A15** | **`TagsToBlock`（预防）** | **新增能力**：我在跑期间目标无法开始且保持不能开始 | 与 `TagsToCancel`（终止）的分工 = "目标还没起"vs"目标已在跑" | Task 1 Step 5 |
| **A16** | **字段命名** | `AppliesToTag` / `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel` | **`Blocks` → `ForbiddenTags`（受动/前置），MUST NOT → `TagsToBlock`（施动/对外）** | Task 1 Step 1 |
| **A17** | **主体侧匹配面** | `{StatusTag} ∪ CategoryTags`（甲案），不含 `DefTag` | 关系表约束语义、不管资产打包；避免同一语义两条寻址路径 | Task 2 |<br>**★ 2026-10-24 订正**：用户裁定 **`DefTag` 与 `CategoryTags` MUST 为并集** ⇒ 最终式 = `CategoryTags` ∪ `{DefTag}`（`StatusTag` 已随 `e0` 删除）。「不含 `DefTag`」与「避免同一语义两条寻址路径」**均作废**——真正的风险是**两条路径**而非并集。见 `DEC-2026-10-13-relation-row-shape` 实施注记。
| **A9** | `TcsGameplayTag` 模块 | **不另立** | 零消费者（今天只有一处筛选需求） | 触发条件 = 出现"第二个域也要按类目筛选" |
| **A10** | `State.Control.*` / `Action.*` | **不建** | 受"一角色一根 + 零消费者不预建根"约束；Q-3 已绕开 | `DEC-2026-08-31` §4 举例作废（原文保留 + 注记） |
| **A11** | `PreemptionPolicy` | **不建字段** | 零消费者，且其唯一依据库外不可复核 | Task 4 重新推导 |
| **A12** | 状态槽映射选型 | **本轮不裁** | `DEC-2026-08-31` §7 第 1 条，StateTree **承载形态**问题（§6：每槽一个 StateTree 实例 vs 单树级联），门 = "槽间级联的实际需求确认"。**★ 2026-10-13 追注（一处待确认的疑似交叠，MUST NOT 直接当依赖用）**：`e4` Step 1 要定义的"槽"（受限身体资源）与本条的"槽"（StateTree 流转维度）**共用同一个词**，而 `SPEC-02-states` §3.4 把槽位竞争的主体写作"**施法运行/动作状态**"——**两者是否同一个槽，本轮未取证**。**依 `MEM-20261001-16`**（命名争议 = 真实语义边界未知的信号）：`e4` Step 1 **MUST 先显式判定"本条是否约束它"**，判定为同一物 ⇒ 先裁 A12；判定为不同物 ⇒ 各自独立、并写明区分判据。**MUST NOT** 在未判定时据词形把 A12 当硬前置（同"按词形配对"的既有根因，见 §0.4 ④） | 独立议题；**与 `e4` Step 1 的关系待 `e4` 自行判定** |
| **A13** | 工具排期 | **本轮不裁** | 同 §7 第 3 条 | R8（`TOOLS-*`） |

---

## 现状证据表（五节之三）

> 引用格式照 `CONVENTION` §5 第 4 行：源码用 `路径:符号名`（**优先符号名，禁行号**）；设计文档用文档 ID + 节号。**完整 20 条住** `DEC-2026-10-13-relation-semantics` §3，此处列与 Task 直接相关的子集。

| # | 判据 | 可复核锚点 | 支撑哪个 Task |
|---|---|---|---|
| E1 | 状态侧四字段形状 | `Source/TcsState/Public/Def/TcsBuffDef.h:FTcsBuffDef::Blocks` / `::Requires` / `::Priority` / `::Cancels`（**改名映射见 A16**） | Task 1 |
| E2 | 技能侧同款四字段 | `Source/TcsSkill/Public/Def/TcsSkillDefData.h:FTcsSkillDefData::Blocks` / `::Requires` / `::Priority` / `::Cancels` | Task 1 |
| E3 | **检查器零行** | 全仓 `Blocks` / `Requires` / `Cancels` 成员访问点零命中 | Task 1 |
| E4 | 类目字段不存在于基类 | `Source/TcsState/Public/Def/TcsStateDefBase.h:FTcsStateDefBase` | Task 2 |
| E5 | 两表主体分工 | `SPEC-02-states` §3.4「组织与解析栈（D3-16）」 | Task 1 / 4 |
| E6 | 原裁决把 Priority 挂在"槽"上 | `DEC-2026-08-31-state-relation` §4 第 2 条 | Task 4 |
| E7 | M2 的裁决键是数值比较 | `Source/TcsAttribute/Public/Attribute/TcsAttributeBandFold.h:IsStrongerTcsOverride`；`TcsAttrModInstance.h:ETcsAttrOverrideTieBreak` | Task 4 |
| E8 | 全仓"谁更强"方向 = 大者胜 | `Source/TcsEffect/Public/Trigger/TcsEffectTrigger.h:FTcsEffectTriggerDef::Priority` 头注释 | Task 1 / 4 |
| E9 | 打断来源优先级无载体 | `Source/TcsSkill/Public/Skill/TcsCastOps.h:FTcsCastOps::CancelCast` 头注释 | Task 4 |
| E10 | `ESRC_Cancelled` 已预留读法 | `Source/TcsState/Public/State/TcsStateEnums.h:EStateRemoveCause::ESRC_Cancelled` | Task 1 |
| E11 | ~~`FGameplayTagQuery` 引擎既有（引证，非本机实测）~~ → **本 Task 不再依赖它** | **★ 2026-10-13 取代**：匹配机制改为**容器 + `FGameplayTag::MatchesTag` 层级匹配**（该函数本机有既有用法先例：`TcsParamChain.cpp`）⇒ 原先"引擎类型本机不可核实"的缺口**随之消失**。〔**同日二订**：该"不可核实"本身也是假前提 —— 引擎源码在 `E:\UnrealEngine\UE_5.8`；`FGameplayTagQuery` **确认存在**、`MatchesTag` 的**父词命中子词**语义已逐字读源复核（`GameplayTagContainer.h` 头注释）。⇒ **"去掉该依赖"这个结论仍然成立**（理由是容器+层级匹配已足够），只是**不再是"因为无法核实"**。〕 | Task 2 |
| E12 | 宿主词汇根先例 | `openspec/specs/gameplay-tag-governance/spec.md`「根段注册表」`DamageCategory` 行 | Task 2 |
| E13 | 插件 MUST NOT 声明具体分类词 | 同规格 `DamageCategory` 共享说明 | Task 2 |
| E14 | 类目扩到定义级的同族论证 | `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h` 头注释 | Task 2 |
| E15 | 空壳字段已在、零消费者 | `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h:FTcsSkillEntrySelector::CategoryTags` | Task 2 |
| E16 | 侧向匹配已有实现形态 | `Source/TcsEffect/Public/Trigger/TcsTriggerCondition.h:FTcsTriggerCondition_HasAllTags` | Task 2 |
| E17 | 开根受"零消费者不预建根"约束 | `gameplay-tag-governance`「根段注册表」+「新建根前先查表」 | Task 1 / 2 |
| E18 | 两根不存在 | `Source/` 零命中；LAC `Config/DefaultGameplayTags.ini` 11 根无此二者 | Task 2 |
| E19 | 槽位竞争依据不可复核 | `SPEC-02-states` §3.4 引 `TCS 报告 06`；同文件 §12.4 自我登记 | Task 4 |
| **E20** | 库外报告系列确不在库内 | `GLOSSARY.md` §6 第 10 条 | Task 4 |
| **E21** | **Lyra 先例的形状**（【外部源码】，取证边界见取代裁决 §1.2） | 公开镜像 `BulkheadLtd/Lyra_5_4`：`LyraAbilityTagRelationshipMapping.h`（`FLyraAbilityTagRelationship` 五字段）+ `.cpp`（三求值函数）+ `LyraAbilitySystemComponent.cpp`（`ApplyAbilityBlockAndCancelTags` 消费点） | Task 1 / 2 |
| **E22** | **Lyra 的匹配机制 = 容器 + 层级匹配**（无查询语言） | 同上 `.cpp`：`AbilityTags.HasTag(Tags.AbilityTag)` + `Tags.AbilityTagsToCancel.HasAny(AbilityTags)` | Task 2 |
| **E23** | **Lyra 亦无 `Priority`**（顶替仲裁住 ASC 侧） | 同上：`ELyraAbilityActivationGroup` + `IsActivationGroupBlocked` / `AddAbilityToActivationGroup` | Task 4（佐证 Q-1） |

---

## 错前提更正（五节之四）

| # | 错前提 | 实况 | 处置 |
|---|---|---|---|
| ① | `DEC-2026-08-31` §4 的"族规则降维"以 **`State.Control.*` 阻断 `Action.*`** 举例，读起来像是已定的落地方案 | 这两根**在全仓不存在**（E18）；该举例是**依赖未建前提的示意** | **机制保留、载体改判**（宿主词汇类目容器 + **Lyra 行形态**，A7/A10）。原文（`FROZEN`）不改，由 `DEC-2026-10-13-*` 两份载体记更正 |
| ② | `SPEC-02-states` §3.4 写"槽位竞争（`PriorityOnly` + `PreemptionPolicy` + 同优先级策略，`TCS 报告 06` **已验证**）" | 该报告**库外不可复核**（E19/E20）；同文件 §12.4 已自我登记"语义拍板时 MUST 重新论证" | **"已验证"不成立**；MUST NOT 照搬两词；`PreemptionPolicy` 不建字段（A11）；Task 4 从判据重新推导 |
| ③ | `PLN-R5`《R5.5 批次表》`e` 行把四步写在**一格**，读起来像"一个批次、顺序做完" | 依赖不对称：检查器串行于语义拍板、级联重评串行于检查器、**槽位竞争表与之正交** | 重切四批（`R5.5-e1`~`e4`），见 §5 |

---

## Task 切分（五节之五）

> 每 Task 给**目标 / 交付物 / 验收信号 / 边界**。批次号沿用《R5.5 批次表》的 `e` 族：`e1`~`e4` 分别对应 Task 1~4（**Task 0 已完成**）。

### Task 0 — 语义拍板（**✅ 已完成 2026-10-13**）

**目标**：把 `DEC-2026-08-31-state-relation` §7 待办第 2 条的三个方框裁完，产出可被下游引用的载体。

**交付物**：
- `Documents/combat-system-design/decisions/dec-2026-10-13-relation-semantics.md`（新，`DEC-2026-10-13-relation-semantics`，状态 `FROZEN`）
- `DEC-2026-08-31-state-relation` 文末追加指向新载体的日期注记（正文不改）
- 本计划（`PLN-R5.5`）

- [x] **Step 1：勘察**——两仓 `git status` + §1.2 清单 9 文件读全 + §1.3 事实表逐条复核（另加 `GLOSSARY` / `gameplay-tag-governance` 规格 / `TcsSkillEntrySelector.h` / `TcsEffectTrigger.h` / M2 折叠器）。**✅ 2026-10-13**：两仓 clean（LAC `master@3f6e90c` 唯一未跟踪项 = 启动提示词自身；TCS `remake@bbc3456` clean）。
- [x] **Step 2：三项逐项拍板**——① `Priority` 归属、② 互斥组结构 + `Blocks`/`Cancels` 正交化、③ 族规则表达式 + `STAT-10` 同批裁。**✅ 2026-10-13 用户逐项拍板**（五项裁定 + 两项载体问题，见 §0.2）。**★ 同日第二笔**：吸收 Lyra 行形态 ⇒ ②③ 被取代裁决改写、① 加强（见 §0.2 Q-1/Q-2/Q-3/Q-7~Q-9 与变更记录）。
- [x] **Step 3：落纸**——`DEC-*` 载体 + 本计划 + 下游挂账同步。**✅ 2026-10-13**（下游同步见 Task 5）。

**验收信号**：三个待裁项各有可复核的裁定 + 判据锚点；`DEC-2026-08-31` 正文零修改、只追加注记。

---

### Task 1（`R5.5-e1`）— 关系表检查器（特例层）

**目标**：让关系表族字段**第一次有真实消费者**——施加态与施法态的共存冲突有确定结果，且 `ESRC_Cancelled` 第一次有内建产生者。**★ 2026-10-13 第二笔后范围 = 五字段（四容器 + 改名），含新增的 `TagsToBlock` 预防能力。**

**交付物**：
- `Source/TcsState/Public/State/TcsStateRelation.h` + `Private/State/TcsStateRelation.cpp`（新：关系检查器——**共用**，主体类型化）
- `Source/TcsState/Private/State/TcsStateOps_Relation.cpp`（新：apply / remove 两个挂点）
- `Source/TcsSkill/Private/Skill/TcsSkillOps_Relation.cpp`（新：技能侧调用点——**激活前校验 + 顶替/取消**）
- `Source/TcsState/Public/Def/TcsStateDefBase.h`（改：**若** `StatusTag` 定根，同步注释口径）
- openspec 提案（关系表检查器能力）

**依赖**：Task 0（已完成）

**边界**：
- **不含**类目降维层（Task 2）；
- **不含** `Priority` 的任何判定（A1：它不属本族）；
- **不含**移除级联重评（Task 3）——本 Task 只落"apply 时校验 + 移除时清 `TagsToCancel` + `TagsToBlock` 的即时阻断"；
- **MUST NOT** 为 `Priority` 造任何形参或字段（A1/A11 的"零消费者不预建"）；
- **MUST NOT** 引入 `FGameplayTagQuery`（A7）；
- **`TagsToBlock` 属本 Task**（它是检查器的特例层能力，A15）；**类目匹配那半**（关系行）归 Task 2。

**Steps 草案**：
1. **检查器形状 + 字段改名**——`FTcsStateRelationChecker`（纯 C++ 静态函数族，照 `FTcsStateOps` 形态），主体参数化：`CheckRequired(主体词集, 目标, 行) -> bool` / `CollectCancels(...)` / `CollectBlocks(...)`。**判定只用 `FGameplayTag::MatchesTag`**（层级匹配 = 既有语义，先例 `TcsParamChain.cpp` 的 `Entry.DefTag.MatchesTag(Filter)`）。**字段改名（零消费者，零迁移成本）**：`Blocks` → **`ForbiddenTags`**、`Requires` → **`RequiredTags`**、`Cancels` → **`TagsToCancel`**。**★ MUST 按此映射，MUST NOT 把 `Blocks` 映射到 `TagsToBlock`**——后者是**新增的施动/对外字段**，与 `ForbiddenTags` 的受动/前置**方向相反**（`DEC-2026-10-13-relation-row-shape` §4.2）。
2. **apply 挂点**——`ForbiddenTags` 命中 ⇒ 拒绝（返回 `EAR_Rejected`，具名 `Log`，**零事件**）；`RequiredTags` 未满足 ⇒ 拒绝（**排队档本批不落**，无消费者）。判定**排在**五轴共存决策**之前**（先过关系门再看叠层）。
3. **作者期校验**——`IsDataValid` **两条**规则（**★ 2026-10-13 修正：原写的"`A.Blocks ∋ B` + `A.Cancels ∋ B` ⇒ 自相矛盾"已撤销**，因按修正后的语义两者同真**合法且是一种抢占策略**，见 `DEC-2026-10-13-relation-row-shape` §3.3/§5.3）：① `A.TagsToCancel` 与 `B.TagsToCancel` 互相包含 ⇒ **双向强夺**、无确定赢家 ⇒ Error；② `A.ForbiddenTags ∋ A`（**自指**）⇒ 自我否定的门禁、永不可满足 ⇒ Error。
4. **`TagsToCancel` 挂点 + `ESRC_Cancelled` 产生者**——施加成功路径里按 `TagsToCancel` 逐条移除既有状态，原因传 `ESRC_Cancelled`（**该枚举首次有内建产生者**，E10）。
5. **`TagsToBlock` 挂点（新增能力）**——**预防语义**：A 在跑期间，命中的目标**无法开始且保持不能开始**（区别于 `TagsToCancel` 的"把已在跑的终止掉"）。**两处的检查时机 MUST 都覆盖**：① 目标尝试开始时；② **A 开始时就已存在的目标**须被即时阻断（否则出现"A 在跑、B 已在跑"的窗口）。**与 `ForbiddenTags` 的分工**：`TagsToBlock` 约束**目标**、`ForbiddenTags` 约束**自己**；两者可从相反一侧表达同一互斥（选哪侧 = 作者声明权的选择）。
6. **`ForbiddenTags` 的检查时机裁决（本 Step MUST 二选一并写清判据）**——规格的执行时机是"apply 时校验 + **任何状态移除后**重评" ⇒ **A 已在跑、B 后到**时，若互斥**只由施动侧声明**（`A.TagsToBlock ∋ B`），B 的 apply 门禁需要**反向查找**才能拒绝。两条路：① 要求互斥**双向声明**（零新机制，但作者负担）；② apply 时做**反向查找**（"是否存在存活实例的 `TagsToBlock` 覆盖我"）。**判据 = 作者负担 vs 每次 apply 的一次扫描**；倾向 ②（装置与内容侧都更省），但须实测扫描成本。
7. **技能侧调用点**——`TryActivate` 的门禁序列里插"关系门"（`ForbiddenTags` = 禁止激活 / `RequiredTags` = 激活前提）；`TagsToCancel` ⇒ 激活时取消指定运行/状态（姿态切换）；`TagsToBlock` ⇒ 预防。**共享同一检查器**（主体 = `FTcsCastRun`）。
   > **★ 2026-10-13 第三笔追注（本 Step 的开工前提已变，MUST 读）**：本句写定的"**主体 = `FTcsCastRun`**"**已被 [`DEC-2026-10-13-state-module-positioning`](../decisions/dec-2026-10-13-state-module-positioning.md) 前置**——该裁决裁定运行态改为**基底 + 两个 payload**、基底归 `TcsCore`，且**该重构先行**（裁定⑧）。⇒ 本 Step **MUST 按新基底重写**（主体 = 基底句柄，而非 `FTcsCastRun`），**MUST NOT 按本句现字面开工**。同批：Step 2 的"判定排在五轴共存决策**之前**"也绑定到要重构的决策 API（该裁决 §1.3 记明技能 `Instancing` 四路分流与状态堆叠决策是**同型重复**，重构将合并为一处）。**原文保留记现场。** **★★ 2026-10-15 追注（该前置已解除，MUST 读）**：`PLN-REFACTOR-1` **全轮完成**（Task 1~8 落地）⇒ **本 Step 的重写前提已满足**：主体类型现为 **`FTcsSkillInstance`**（技能侧，继承基底 `FTcsStateInstance`；~~旧名 `FTcsCastRun`~~ 已从代码消失）/ **`FTcsBuffInstance`**（状态侧，同基底）。**本 Step 的判据面一字不改**（"共享同一检查器""主体类型化""插在门禁序列里"三条均不受重构影响），**变的只是名与落点**；Step 2 的"判定排在五轴共存决策之前"同理——合并后的决策接口 = 共享层的 **`FTcsEntityAdmission`**（本重构 Task 1 已落地，故"2 与 3 可并行"那条判据照旧成立）。
8. **`StatusTag` 定根**（条件已成立）——本 Task 的检查器是 `StatusTag` 的**第一个解析消费者** ⇒ 按「一角色一根」查表、定根、**同步 `gameplay-tag-governance` 根表的行数与 `## Purpose` 数字**（E17；`STAT-4` 就地闭合）。
9. **规格同步** + 提案归档 + 编译（双配置）+ 装置检查。

**验收信号**：① `ForbiddenTags` 命中 ⇒ 施加被拒且**零事件**（实例数不变）；② `RequiredTags` 未满足 ⇒ 被拒；③ `TagsToCancel` 命中 ⇒ 目标被移除、**原因读数 = `Cancelled`**、账本/触发行按锚点摘净（复用既有 `RemoveByState` 级联路径）；④ **`TagsToBlock` 的两条时机都可判**（目标后到被拒 / 目标已在跑被即时阻断）；⑤ 两条作者期 Error 各被构造一次并报出；⑥ **"填法即策略"两种配置各取一次读数**（`TagsToCancel` 单独 = 后到者赢 / `ForbiddenTags` + `TagsToCancel` = 在位者赢，见 `DEC-2026-10-13-relation-row-shape` §5.3）；⑦ 双配置编译 `Succeeded` 零 warning；⑧ `Run` 区段**零非预期红字**（预期红字逐条列在装置头部）。

---

### Task 2（`R5.5-e2`）— 类目降维层（含 `STAT-10` 三项一并落地）

**目标**：把 N×M 逐对声明压成 K 条关系行——作者写"**控制域父词** 使 **动作域某词** 无法开始"，而不是逐对配。**★ 2026-10-13 取代后：形态 = Lyra 行（一行一主体词 + 四容器）+ 层级匹配，不是"Query × Query 的族规则行"。**

**交付物**：
- `Source/TcsState/Public/Def/TcsStateDefBase.h`（改：新增**类目容器**字段）
- `Source/TcsState/Public/State/TcsStateRelationMapping.h` + `Private/State/TcsStateRelationMapping.cpp`（新：**Lyra 行形态**的中心 `UDataAsset` + 求值器——`{AppliesToTag, RequiredTags, ForbiddenTags, TagsToBlock, TagsToCancel}`）
- `Source/TcsState/Private/State/TcsStateOps_Relation.cpp`（改：类目行接进检查器）
- `Source/TcsSkill/Private/Skill/TcsSkillOps_Selector.cpp`（改：`ESS_ByCategoryTags` 档真实现——`STAT-10` 匹配面 B）
- openspec 提案（类目标识与关系行能力；`STAT-10` 的载体 + 规则两项）

**依赖**：Task 1（检查器是第一个消费者）

**边界**：
- **不开任何新 tag 根**（A10）；类目词一律**宿主 ini 声明**，插件 MUST NOT 声明具体分类词（E13）；
- **不另立 `TcsGameplayTag` 模块**（A9；触发条件 = 出现第二个域也要按类目筛选的真实需求）；
- **MUST NOT 引入 `FGameplayTagQuery`**（**★ 2026-10-13 取代**：Lyra 证明"容器 + 层级匹配"即可——主体词填父词即覆盖一族；且该类型本机不可核实）。**留触发条件**：出现"一个主体词要同时命中两个不相邻父词"的真实需求时再评估；
- 关系行**不改逐对字段的语义**（两层各自独立、可叠加）。

**Steps 草案**：
1. **类目容器字段**——`FTcsStateDefBase` 新增 `FGameplayTagContainer CategoryTags`（`STAT-10` ①的声明面）。**形状判据**：容器（不是查询）——**声明的是"我是什么"，不是"我要匹配谁"**；匹配规则住在**关系行/选择器**那一侧（同一容器可被多个消费者用不同规则读，照 `DamageCategory` 的"一套词两个读取点"先例 E12）。
2. **关系行形状（Lyra 形态）**——中心 `UDataAsset` 持 `TArray<FTcsRelationRow>`，行 = `{ FGameplayTag AppliesToTag; FGameplayTagContainer RequiredTags; FGameplayTagContainer ForbiddenTags; FGameplayTagContainer TagsToBlock; FGameplayTagContainer TagsToCancel; }`。**求值 = 对主体侧词集（`{StatusTag} ∪ CategoryTags`）做 `HasTag(AppliesToTag)` 层级匹配，命中则该行的四个容器并入判定。** **这就是取代原"族规则行（Query × Query）"的那一层**（`DEC-2026-10-13-relation-row-shape` §4.1）。
3. **载体与 Def 上三数组的去留**——载体 = 中心 DataAsset（**兑现 `D3-16` 既有的"统一 DataAsset 行 + `StatusTag` 匹配"**，不是新组织）；两个 Def 上的 `ForbiddenTags` / `RequiredTags` / `TagsToCancel` 数组去留 = **归 Task 4**（与 `Priority` 同批——同一类"字段该不该留在 Def 上"的问题）。
4. **接进检查器**——Task 1 的逐对判定前先跑关系行（行是**通例**、Def 上的数组是**特例**；**特例优先**，因为它是显式声明）。
5. **`STAT-10` 匹配面 B**——`ESS_ByCategoryTags` 档真实现：读条目的 `DefTag` → 定义库解析 → 该 Def 的 `CategoryTags` → 跑匹配。**接线同时把"注解零消费者"这一条消掉**（E15）。
6. **`STAT-10` 三项收窄回写**——台账该行记：载体（定义基类容器字段）+ 规则（**容器 + 层级匹配**，关系行补"一行一主体词"的二元层）+ 模块归属（**不另立**，A9）。
7. **规格同步** + 提案归档 + 编译（双配置）+ 装置检查（**须有一条"关系行替代逐对"的等价性检查**：同一场景用两层各配一次，读数一致）。

**验收信号**：① 一条关系行替代 N 条逐对声明，**读数与逐对配置一致**（等价性检查）；② **父词行覆盖一族**（填父词的 `AppliesToTag` 命中该族全部子词——层级匹配的实测）；③ `ESS_ByCategoryTags` 档从"具名未实现 + 零施加"变为**真筛选**（条目槽位数产生真实变化）；④ 插件侧**零新增根**（`gameplay-tag-governance` 根表行数与 `## Purpose` 数字不变）；⑤ 双配置编译 `Succeeded` 零 warning。

---

### Task 3（`R5.5-e3`）— 移除级联重评

**目标**：状态被移除后，**依赖它的存活状态**被重新评估——且批量移除**合并成一次**（脏标记），不产生 N 次重评。

**交付物**：
- `Source/TcsState/Private/State/TcsStateOps_Relation.cpp`（改：脏标记 + 批量重评）
- 装置：批量移除的合并读数（N 次移除 ⇒ 1 次重评）

**依赖**：Task 1（要重评的正是检查器的判定）

**边界**：
- **不含** Task 2 的关系行对重评的影响（若 Task 2 已落地，关系行**一并**纳入重评判定；若未落地，本 Task 只覆盖逐对层）；
- **MUST NOT** 在重评里做**递归**移除（重入由既有 `InRemovalBroadcastHandles` 守卫的语义处理——先例 R5 Task 6b）。

**Steps 草案**：
1. **脏标记形状**——实体级 `TSet<FTcsStateHandle>`（待重评集）或按 DefTag 的脏集；选型判据 = "重评判定需要什么输入"（`Requires` 判定需要"某 tag 还在不在" ⇒ tag 级脏集足够）。
2. **合并点**——`RemoveState` / `ExpireState` / `UnregisterEntity` 三条路径**都**标脏；**批量移除的合并点** = `UnregisterEntity`（唯一天然批量的入口）+ 一个"本帧末尾统一 flush"（照 M2 的 `BeginBatch` / `Commit` 事务语义形态）。
3. **重评动作**——对脏集内每个存活实例重跑 **`RequiredTags`** 判定（`ForbiddenTags` 不需重评：它只在 apply / 目标开始时判）；不满足 ⇒ 移除（原因用 **`TagsToCancel` 同一档** `ESRC_Cancelled`——"被共存规则取消"；**MUST 写进注释**）。**★ 改名后按新名读**（`Requires` → `RequiredTags`、`Blocks` → `ForbiddenTags`、`Cancels` → `TagsToCancel`）。
4. **装置检查**——N 次批量移除 ⇒ 重评**恰 1 次**（合并证据）；单次移除 ⇒ 重评 1 次。

**验收信号**：批量 N 次移除的合并读数可分（N 与重评次数**不成正比**）；重评移除的原因读数正确；双配置编译零 warning。

---

### Task 4（`R5.5-e4`）— 槽位竞争表（主体 = 施法运行）

**目标**：同一受限槽位上的多个运行态/动作状态有确定的抢占结果——**从判据重新推导**，不照搬两个只有名字的概念。

**交付物**：
- `Source/TcsState/Public/State/TcsStateSlotTable.h` + `Private/State/TcsStateSlotTable.cpp`（新，或按 Step 1 的裁定落别处）
- `Source/TcsSkill/`（改：施法运行接入槽位表）
- 两处 `Priority` 字段的**物理处置**（移除 / 改述）
- openspec 提案（槽位竞争能力）

**依赖**：R6（施法运行**已落地**）；**与 Task 1~3 正交**（可并行或先行）。**★ 2026-10-15 追注**：另一处曾被登记为阻碍的依赖（**本 Task 的产出物明文绑在 ~~`FTcsCastRun`~~ 上**）**已解除**——`PLN-REFACTOR-1` 全轮完成后，施法运行态 = **`FTcsSkillInstance`**（继承基底）⇒ 本 Task 按新名写即可，**Step 草案与验收信号无需改判据**

**边界**：
- **MUST NOT** 照搬 `PriorityOnly` / `PreemptionPolicy`（其依据库外不可复核，E19/E20）；
- **MUST NOT** 复用 `ETcsAttrOverrideTieBreak`（A2：裁决键类型不同）；
- **MUST NOT** 顺手裁状态槽映射选型（A12——那是 StateTree 宿主形态问题）。

**Steps 草案**：
1. **槽位的定义**（本 Task 的**第一步也是最重要的一步**）——"槽"是什么？候选：① 按 DefTag 的类别（`Control` / `Move` / `Cast` 类目容器）；② 显式的槽身份词（新增字段）；③ 复用 `D3-16` 的"槽位竞争表行"。**MUST 二选一并写清判据**（依 `MEM-20261006-10`：从"用户要实现什么能力"反推——能力 = "**两个动作争夺同一身体资源时结果确定**"）。**★ 2026-10-13 追注（本 Step 的第一件事）**：先判定 **A12（状态槽映射选型，`DEC-2026-08-31` §7 第 1 条）是否约束本 Step**——两条都叫"槽"，但 A12 的槽是 **StateTree 承载形态**（§6：每槽一个实例 vs 单树级联，门 = "槽间级联需求"），本 Step 的槽是**受限身体资源**；**是否同一物本轮未取证**。**依 `MEM-20261001-16`**（命名争议 = 真实语义边界未知的信号）：判定为同一物 ⇒ 先裁 A12 再动本 Step；判定为不同物 ⇒ **写明区分判据**后各自独立。**MUST NOT** 据词形直接采用 A12 的口径（同"按词形配对"的既有根因，§0.4 ④）。
2. **抢占语义从判据推导**——可用先例**只有仓内可复核的**：`FTcsEffectTriggerDef::Priority`（大者胜 + **同优先级按登记序**，E8）与 `FTcsChainRedirect::Priority`（大者胜 + 打平取**后挂者**）。**判据问题**：槽位竞争要的是"登记序/后挂者"这类**时序决胜**，还是"拒绝后到者"？
3. **同优先级决胜**（A3 的落地）——按 Step 2 的裁定给规则；**不继承** M2 策略族。
4. **打断来源优先级仲裁**（E9）——"来源优先级 vs `IsInterruptibleNow()`"：本 Step 决定是否为它建载体（今天零消费者 ⇒ 若无真实需求，**如实登记为未落面**并写进台账）。
5. **两处 `Priority` 字段的物理处置**——移除（若槽位表自带槽身份 + 优先级）或改述为"声明参与哪个受控槽"。**改动面**：两个 Def + 可能的设计文档；**零消费者 ⇒ 无迁移成本**。
6. **规格同步 + 错前提回写**——`SPEC-02-states` §3.4 的"`PriorityOnly` + `PreemptionPolicy`（`TCS 报告 06` 已验证）"句须就地注明（原文保留 + 注记：该报告不可复核、语义已由 `DEC-2026-10-13` + 本 Task 重新论证）。**依 `MEM-20261006-11`**：旧裁定的字面与判据冲突 ⇒ 取判据 + 显式回写取代登记。
7. **编译（双配置）+ 装置检查**。

**验收信号**：① 槽位定义有判据、不是类比；② 两个运行态争同一槽的结果确定且可配（**阴性对照**：构造"优先级更小但时间更晚"的一条，验证不是时序偶然）；③ 两处 `Priority` 字段的处置已落地且文档同步；④ `SPEC-02-states` §3.4 该句已加取代注记；⑤ 双配置编译零 warning。

---

### Task 5 — 收束

**目标**：把本批从"文档绿"收敛为"文档 / 台账 / 规格 / 记忆四处一致"。

**交付物**：本计划全文回写 + 设计文档回写 + 台账同步 + 提案归档 + 三方向扫描报告。

- [ ] **Step 1：设计文档回写**——`SPEC-02-states` §3.4（三字段 + Priority 归属改述 + 执行时机 + `DEC-2026-08-31` §4 举例注记）、`SPEC-04-skill` §2（同形状映射 + Priority 归属）、`SPEC-04-skill` §11.2 的关系字段行与 `R6.5-g` 依赖口径。
- [ ] **Step 2：台账同步**——`STAT-4`（✅ 闭合于 Task 1）、`STAT-10`（✅ 三项落地于 Task 2）；新增条目（若 Task 4 的打断来源优先级仍未落 ⇒ 立触发条件型条目）；**计数三处对齐**（身份块 / `INDEX` §4.5 / `INDEX` §2 行）+ **机械复核 = 表行数 = 唯一 id 数**。
- [ ] **Step 3：`PLN-R5` 同步**——《R5.5 批次表》`e` 行改为"已拆四批，见 `PLN-R5.5`"+ 四批状态；`INDEX` §1 现状行、§4.3 计划表（新增 `PLN-R5.5` 行）、§4.4 路线图 R5.5 行。
- [ ] **Step 4：`GLOSSARY` 同步**——§5.1 文档 ID → 路径映射表**新增两行**（`DEC-2026-10-13-relation-semantics` / `PLN-R5.5`）；若新增编号族（`PLN-R5.5` 不是编号族）视情形。
- [ ] **Step 5：两册日志**——`LOG-DECISIONS` 补本轮裁决（五项 + 两条错前提更正）+ `LOG-IMPLEMENTATION` 补实施记录；`最后更新` 同批改。
- [ ] **Step 6：三方向扫描**（依 `MEM-20260930-05` / `MEM-20261004-16` / `MEM-20261013-03`）——
  - **符号方向**：被改动/删除的符号（若 Task 4 移除两处 `Priority`）全库 grep，每处命中显式归入"引用 / 理由 / 历史留痕"三类并处置；
  - **计数方向**：所有"共 N 个根 / N 条条目 / N 个能力"表述 grep 全集逐点对齐；
  - **被取代的结论方向**：用**否定式** grep（`未定稿` / `零消费者` / `未开工` / `已验证` / `不可复核`），逐处判定是否已被本轮动作取代，按产物时间戳分"写下即为假"（订正）与"当时为真"（原文保留 + 日期追注）两类处置。
- [ ] **Step 7：提案归档与门禁**——`openspec validate --all --strict --no-interactive` 全绿 + `changes/` 零活动提案 + 两处 `## Purpose` 手工补正（若新增能力）。
- [ ] **Step 8：检查点卡**——按 `harness-retro` 门控刷新（不自动写卡）。

**验收信号**：门禁全绿、台账计数 = 行数、四处文档一致、三方向扫描**每处命中都被显式归类**（完成标志不是"grep 零命中"）。

---

## 批次与依赖总览

```
Task 0（语义拍板，✅ 已完成）
   │
   ├─→ Task 1（e1 检查器·特例层）──┬─→ Task 3（e3 级联重评）
   │                              └─→ Task 2（e2 类目降维层 + STAT-10）
   │                                     └─→ （e2 落地后，R6.5-g 的两个 Custom 档具备条件）
   └─→ Task 4（e4 槽位竞争表）※ 与 Task 1~3 正交，主体 R6 已交付
                                    │
                              Task 5（收束）
```

**关键路径** = Task 0 → 1 → 2 → 5（`e2` 是 `R6.5-g` 的下游依赖）；**Task 4 可并行**。

---

## 非目标（本批不做）

- **不做**状态槽映射选型（多实例 vs 单树级联）——`DEC-2026-08-31` §7 第 1 条，StateTree 宿主形态问题；
- **不做**矩阵审查视图 / 规则沙盘（R8 `TOOLS-*`）；
- **不做** `Requires` 未满足的**排队**档（无消费者，零消费者不预建）；
- **不做**按类目筛选的独立模块（`TcsGameplayTag`，A9）；
- **不做**开 `State.Control.*` / `Action.*` 根（A10）；
- **不做** `PreemptionPolicy` 字段（A11）；
- **不做** `R5.5` 其余批次（`a`~`d` / `f`~`h` 归属不变）；
- **不做**网络操作复制（`WAIT-3`）、Mass 适配。

---

## 风险与弹性

| 风险 | 判断 | 处置 |
|---|---|---|
| Task 4 的"槽"定义无判据可依（唯一依据库外不可复核） | **高**——它只能从能力反推 | Step 1 就是这件事；若推导不出确定形状，**如实登记为未落面**并退回用户拍板，MUST NOT 硬造 |
| **Task 4 与 A12 的"槽"疑似同词异义**（A12 = StateTree 承载形态；Task 4 = 受限身体资源） | **中**——若实为同一物，则 `e4` 不能独立开工、须先裁 A12；若不同物，则须写明区分判据 | Step 1 的第一件事 = **判定是否同一物**（依 `MEM-20261001-16`）。**MUST NOT** 据词形直接采用 A12 口径，也 MUST NOT 在未判定时把 A12 当硬前置（两个方向都是"按词形配对"） |
| Task 4 与 Task 1~3 并行时改同一个文件（`TcsStateOps_Relation.cpp`） | 中 | Task 4 **MUST 独立文件**（槽位表是独立表，与关系检查器不同物）；若必须共处，则串行 |
| 类目容器字段加在 `FTcsStateDefBase` 上会波及 R6 已落资产 | 低 | 纯新增字段、无默认值语义变化；`IsDataValid` 不强制非空（空容器 = 不参与类目匹配） |
| `StatusTag` 定根会牵动根表与既有验收资产 | 中 | Task 1 Step 6：按「一角色一根」定名；既有资产词已是 `StateDef.*`，若改判需**同步资产与装置**（改动面 = 资产 + ini + 断言） |
| 关系行与逐对数组"两层叠加"会造出两处真相 | 中 | 等价性检查（Task 2 验收 ①）是硬判据；两层语义正交（显式声明优先于行推导） |
| **`Blocks` → `TagsToBlock` 的改名陷阱**（两格词形近而语义方向相反 ⇒ 静默语义反转） | **高**（零消费者、编译器不报错，错了没人发现） | Task 1 Step 1 的映射表 + `DEC-2026-10-13-relation-row-shape` §4.2 的判据（**回归 `TcsBuffDef.h` 原文注释**）；验收须有一条"被拒绝的是**自己**不是目标"的读数 |
| **`TagsToBlock` 的两条时机**（目标后到 / 目标已在跑）漏其一 ⇒ 留下"A 在跑、B 也在跑"的窗口 | 中 | Task 1 Step 5 明确要求**两条都覆盖**，验收 ④ 逐条给读数 |
| **`ForbiddenTags` 的检查时机**（A 已在跑、B 后到时是否需要反向查找） | 中 | Task 1 Step 6 是**显式裁决点**（双向声明 vs 反向查找）；倾向反向查找 + 实测扫描成本 |

---

## 变更记录

- 2026-10-15 **前置解除追注（`PLN-REFACTOR-1` 全轮收束的连带）**：本文 Task 1（`e1`）/ Task 4（`e4`）被 `DEC-2026-10-13-state-module-positioning` **前置**的状态**已解除**——该裁决的实施（`PLN-REFACTOR-1`）**Task 1~8 全部落地**（2026-10-15）。**本次追注只做三件事**：① 状态行与《最后更新》加解除声明；② 给出**新旧名对照**（施法运行 `FTcsCastRun` → `FTcsSkillInstance`；状态域实现名 `FTcsStateRegistry` / `FTcsStateOps` / `UTcsStateSubsystem` → `FTcsBuffRegistry` / `FTcsBuffOps` / `UTcsBuffSubsystem`）；③ Task 1 Step 7 / Task 4 的依赖处就地加注。**MUST NOT 读成"本计划的 Task 已开工"**——**实施仍未开工**，变的是**开工前提**。**同批一处口径澄清**：本计划的"`e2`/`e3` 不受影响"**仍然成立**（它们的产出物不绑运行态类型）。
- 2026-10-13 **第二笔：吸收 Lyra 行形态**（用户提出 `ULyraAbilityTagRelationshipMapping` 先例）——**开取代裁决** [`DEC-2026-10-13-relation-row-shape`](../decisions/dec-2026-10-13-relation-row-shape.md)（取代首轮的裁决②③），本计划五处同步：
  - **行形态**：`{SubjectQuery, ObjectQuery, Action}` → **`{AppliesToTag, RequiredTags, ForbiddenTags, TagsToBlock, TagsToCancel}`**（一行一个主体词 + 四容器；载体 = 中心 `UDataAsset`，兑现 `D3-16`）；
  - **匹配机制**：**去掉 `FGameplayTagQuery`** → 容器 + `FGameplayTag::MatchesTag` **层级匹配**（连带去掉一个"本机不可核实"的依赖）；
  - **字段命名**：`Blocks`/`Requires`/`Cancels` → **`ForbiddenTags`/`RequiredTags`/`TagsToCancel`**；**★ 陷阱：`Blocks` MUST NOT 映射到 `TagsToBlock`**（方向相反）；
  - **撤销一条 Error 规则**（`A.Blocks ∋ B` + `A.Cancels ∋ B` 同真 ⇒ 自相矛盾）——按修正语义**两者同真合法且是一种抢占策略**；改为"填法即策略"（A14）+ 两条新校验（自指 / 双向强夺）；
  - **新增能力 `TagsToBlock`（预防）**——与 `TagsToCancel`（终止）分工 = "目标还没起"vs"目标已在跑"。
  - **一处配对错误更正**（本轮自行抓出）：首轮把 `Blocks` 与 `Cancels` 配成一对；**真正同轴的是 `Blocks`/`Requires`**（前置条件正反两面）。根因 = 按词形而非字段注释配对；**外部对照佐证** = Lyra 把两个方向明确分成两个字段。
  - **未被取代**：裁决①（`Priority` 归槽位竞争表，**被 §5.4 进一步加强**）/ ④ / ⑤ / ⑥、`STAT-10` 同批裁、`R6.5-g` 依赖分两半。
- 2026-10-13 建立：`R5.5-e` 开工收窄轮产出——Task 0（语义拍板，五项裁定 + 两条错前提更正）已完成；Task 1~4（`e1`~`e4`）切分与 Step 草案；Task 5 收束步骤（含三方向扫描）。载体 = `DEC-2026-10-13-relation-semantics`。
