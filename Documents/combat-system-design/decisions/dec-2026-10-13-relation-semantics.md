# 关系表族语义裁决：`Blocks` / `Requires` / `Cancels` 的正交语义与 `Priority` 的归属

- **文档 ID**：`DEC-2026-10-13-relation-semantics`
- **类型**：DEC / 决策记录（关系表族语义拍板）
- **状态**：FROZEN（决策已定稿并拍板 2026-10-13；**实现状态 = 零落地**——本裁决只定语义，检查器属 `PLN-R5.5`）
- **权威范围**：关系表族**四字段的语义、归属与相互边界**（`Blocks` / `Requires` / `Cancels` 的正交读法；`Priority` 的归属裁定；互斥组的两层结构；族规则表达式形态）。**不在本文**——状态槽映射选型（多实例 vs 单树级联）、矩阵视图/规则沙盘的工具排期、槽位竞争表**自身的** Priority 语义形状、检查器的实现步骤（属 `PLN-R5.5`）
- **最后更新**：2026-10-13

> **性质**：本裁决是 [`DEC-2026-08-31-state-relation`](dec-2026-08-31-state-relation.md) §7 待办第 2 条（"关系表字段细节"）的**落地载体**。该文档状态为 `FROZEN`，按 `CONVENTION` §7 其正文不再修改，仅在文末追加指向本文的日期注记。
>
> **★★ 取代注记（2026-10-13 同日，同一天内第二笔）**：**本文的裁决② 与 裁决③ 已被 [`DEC-2026-10-13-relation-row-shape`](dec-2026-10-13-relation-row-shape.md) 取代**（吸收 Lyra 的 `ULyraAbilityTagRelationshipMapping` 形态），且本文 §2 裁决② 里的一条 `IsDataValid` 规则（`A.Blocks ∋ B` + `A.Cancels ∋ B` ⇒ Error）**已撤销**。**继续有效的部分是：裁决①（`Priority` 移出关系字段族 → 槽位竞争表，且被新裁决 §5.4 进一步加强）、裁决④、裁决⑤、裁决⑥**；裁决③ 中"`STAT-10` 与本任务同批裁"这**一条结论**亦继续有效（只换匹配机制，不换该判断）。**本注按 `CONVENTION` §7 追加、正文一字未改**——正文保留记"当时为何那样裁"的现场。
>
> **证据标注**：【源码取证】= 本机全仓实测（`Plugins/Tirefly/TireflyCombatSystem` + LAC 宿主仓 `Source/TcsDev/` + `Config/DefaultGameplayTags.ini`）；【引证】= 仓内既有文本的转述（**未经本机独立复核**，逐个标注）；【设计判断】= 架构推理；【用户拍板】= 2026-10-13 本会话逐项裁定。
>
> **本轮取证边界（如实，MUST 读）**：本机**未定位到 UE 引擎源码树**（见 §3.4 E11）⇒ 凡涉引擎内部事实的锚点（`FGameplayTag::MatchesTag` 的层级匹配语义、`FGameplayTagQuery` 的成员与编辑器定制）本轮**只有仓内引证**，未经本机源码复核。故本裁决的**仓内**判据（四字段形状与消费者、两表主体分工、M2 三级阶梯、根表与类目先例）全部本机实测；**涉引擎的**判据为引证。
>
> **★★ 2026-10-13 同日订正（本段"未定位到引擎源码树"这一判断是错的，MUST 读）**：引擎源码**在本机**，位于 **`E:\UnrealEngine\UE_5.8`**（`Build.version` 实测：`MajorVersion 5` / `MinorVersion 8` / `PatchVersion 3` / `Changelist 58210709` / `BranchName ++UE5+Release-5.8`；本机另有 `UE_5.6_ex` / `UE_5.7_ex`）。早期探查找错了位置（只查了 `C:\Program Files\Epic Games` 与注册表）。⇒ **本段所述"未经本机源码复核"的两条引擎判据，现已逐条读源复核，结论如下**：
>
> - **`FGameplayTag::MatchesTag` 的层级匹配语义** —— **复核成立**。`GameplayTagContainer.h:FGameplayTag::MatchesTag` 的头注释原文：「Determine if this tag matches TagToCheck, **expanding out parent tags**；`"A.1".MatchesTag("A")` will return True，`"A".MatchesTag("A.1")` will return False」⇒ **父词命中子词、反向不成立**，与 `DEC-2026-10-13-relation-row-shape` 采用的口径**逐字一致**。同头另有 `MatchesTagExact`（精确匹配）与 `MatchesTagDepth`（匹配深度）。
> - **`FGameplayTagQuery` 的存在性** —— **确认存在**（`GameplayTagContainer.h:FGameplayTagQuery`，同类内另有 `FGameplayTagQueryExpression`、`FGameplayTagContainer::MatchesQuery`）。⇒ 当年"选它作匹配载体"在**类型存在性**上无障碍；`DEC-2026-10-13-relation-row-shape` 去掉该依赖的理由**不是"它不存在"**，而是"Lyra 以同一需求证明**容器 + 层级匹配**已足够"（该理由不受本次复核影响）。
>
> **本节按 `CONVENTION` §7 追加；正文一字未改。**

---

## 0. 裁定总表（2026-10-13，用户逐项拍板）

| # | 议题 | 裁定 | 影响面 |
|---|---|---|---|
| ① | **`Priority` 的语义与归属** | **`Priority` 移出关系字段族**——归**槽位竞争表**（主体 = 施法运行 / 动作状态，独立表）。关系表族（`Blocks` / `Requires` / `Cancels`）**不含 `Priority`** | `FTcsBuffDef::Priority` / `FTcsSkillDefData::Priority` 的**语义归属**改判；两字段的物理去留归 `R5.5-e4`；体裁与 M2 `OverridePriority` **判为两个无关概念** |
| ② | **互斥组的结构** | **混合两层**：逐对字段为**特例层**（本轮可落地）；类目元数据 + 族规则为**通例降维层**（可选叠加，非替代）。`Blocks` / `Cancels` 语义**正交化**（退让声明 vs 强夺声明） | `SPEC-02-states` §3.4、`SPEC-04-skill` §2 关系字段节；检查器实现 |
| ③ | **族规则表达式** | **类目容器（宿主词汇，不开新根）+ `FGameplayTagQuery` 对另一侧容器求值 + 一层"族规则行"**。`STAT-10` 与本任务**判为同一套机制的两半，MUST 同批裁** | 台账 `STAT-10`；`FTcsSkillEntrySelector::CategoryTags`；`FTcsStateDefBase` 新增类目字段 |
| ④ | **`State.Control.*` / `Action.*` 两个根** | **不建**（裁决 ③ 已用宿主词汇类目容器绕开开根需求） | `DEC-2026-08-31` §4 的该举例作废（见 §4 错前提更正①） |
| ⑤ | **`PriorityOnly` / `PreemptionPolicy`** | **不作为既有设计照搬**——其唯一依据（`TCS 报告 06`）库外不可复核；本轮**不为 `PreemptionPolicy` 建字段**，槽位竞争的语义须在 `R5.5-e4` 从判据重新推导 | `SPEC-02-states` §3.4 该句；`DEC-2026-08-31` §4 第 2 条的该读法 |
| ⑥ | **范围边界** | `DEC-2026-08-31` §7 三框**只有第 2 条属本任务**；第 1 条（状态槽映射选型）与第 3 条（工具排期）**本轮不裁**；`R5.5-e` 的后两步**再拆批次** | `PLN-R5.5` §5 Task 切分；`PLN-R5`《R5.5 批次表》`e` 行 |

---

## 1. 勘察：现状事实表

### 1.1 什么是"形状已落"（源码取证）

| 事实 | 锚点 | 结论 |
|---|---|---|
| 状态侧四字段已落 | `Source/TcsState/Public/Def/TcsBuffDef.h:FTcsBuffDef::Blocks` / `::Requires` / `::Priority` / `::Cancels` | 形状在，注释写明"检查器归 R5.5-e" |
| 技能侧四字段同款已落 | `Source/TcsSkill/Public/Def/TcsSkillDefData.h:FTcsSkillDefData::Blocks` / `::Requires` / `::Priority` / `::Cancels` | 同形状，注释写明"与状态族同形" |
| 字段载体类型 | `TArray<FGameplayTag>` ×3 + `int32 Priority = 0` | **纯逐对声明**——载体是 tag 数组，不是类目容器，也不是查询 |

### 1.2 什么是空的（源码取证）

| 事实 | 锚点 | 结论 |
|---|---|---|
| **关系字段检查器零行** | 全仓 grep `Blocks` / `Requires` / `Cancels` 的**消费点**（`.Blocks` / `->Blocks` 一类）**零命中** | 四字段**零消费者**——"形状已落"不等于"语义已定" |
| `Priority` 名的三个既有消费者**都不是关系表** | `Source/TcsEffect/Public/Trigger/TcsEffectTrigger.h:FTcsEffectTriggerDef::Priority`（同 Tag 多行触发顺序）、`Source/TcsAttribute/Public/Attribute/TcsAttrModDef.h:FTcsAttrModDef::OverridePriority`（覆盖带强弱）、`Source/TcsSkill/Public/Skill/TcsChainRedirect.h:FTcsChainRedirect::Priority`（重定向栈取用） | 关系表的 `Priority` **不继承**上述任一语义（见 §2 裁决①） |
| **类目容器字段不存在** | `FTcsStateDefBase`（`Source/TcsState/Public/Def/TcsStateDefBase.h`）无类目字段；`FTcsSkillEntrySelector::CategoryTags` 存在但**零消费者**（`Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h`，注释自述"本轮预留字段"） | 裁决 ③ 的**声明面**尚无载体 |
| `State.Control.*` / `Action.*` 两根不存在 | `Source/` 全仓零命中；LAC `Config/DefaultGameplayTags.ini` 的 11 个根无此二者 | `DEC-2026-08-31` §4 的族规则举例**依赖不存在的根**（见 §4 错前提更正①） |
| 无"打断来源优先级"类型 | `Source/TcsSkill/Public/Skill/TcsCastOps.h:FTcsCastOps::CancelCast` 注释自述"MUST NOT 为它发明一个零消费者的优先级形参" | 打断优先级仲裁**无载体**，是裁决 ① 的独立佐证 |
| TCS 活跃提案 0 个 | `openspec/changes/` 仅存 `archive/` | 若要开提案须新建 |
| `PLN-R5.5` 不存在 | `Documents/combat-system-design/plans/` 无 `r5.5` 文件 | `R5.5` 至今只有批次表、无 Task 骨架 |

### 1.3 什么是库外不可复核的（源码 + 文档取证）

| 事实 | 锚点 | 结论 |
|---|---|---|
| 槽位竞争语义的唯一依据是库外报告 | `Documents/combat-system-design/spec/03-module-states.md` §3.4 引 `TCS 报告 06`；同文件 §12.4 已自我登记"**不可复核**，语义拍板时 MUST 重新论证" | `PriorityOnly` / `PreemptionPolicy` **只有名字**（见 §4 错前提更正②） |

---

## 2. 口径裁决表（逐项：议题 / 裁定 / 判据 / 影响面）

### 裁决 ①：`Priority` 移出关系字段族

**裁定**：关系表族 = `Blocks` / `Requires` / `Cancels` **三字段**。`Priority` **不属于**这一族——它归**槽位竞争表**（主体 = 施法运行 / 动作状态，与关系表**并存**、各有主体，照 `SPEC-02-states` §3.4 的既有分工）。

**判据（三条，逐条取证）**：

1. **原裁决自己就把 Priority 挂在"槽"上**——`DEC-2026-08-31-state-relation` §4 第 2 条原文写的是"**减速/定身/眩晕抢同一槽时**靠显式 `Priority` …确定性收敛"，主语是"抢同一槽"而不是"成对关系"。⇒ 把 `Priority` 读成"关系表字段细节"是**后人的归类**，不是原裁决的表述。【源码取证 + 设计判断】
2. **两表主体不同**——`SPEC-02-states` §3.4 明写"关系表 / 槽位竞争表**并存**（槽位 = 施法运行 / 动作状态，关系 = Buff 施加态，互相影响）"。而 `FTcsBuffDef` 是**施加态**定义、`FTcsSkillDefData` 是**施法语义**定义 ⇒ 挂在 `FTcsBuffDef` 上的 `Priority` 其主体错位。【源码取证】
3. **能力轴反推**（依 `MEM-20261006-10`：形态题 MUST 从"用户要实现什么能力"反推，MUST NOT 用"同族已有形态"类比）：本族要的能力是"**两个不同定义争夺同一受限资源时，结果确定且可配**"。逐对关系（`Blocks` / `Cancels`）表达的是"**哪两个不能共存**"；`Priority` 若也留在这一族，它表达的是"**共存冲突时谁赢**"——**后者是槽位竞争的问题，不是关系表的问题**。【设计判断】

**与 M2 `OverridePriority` / `OverrideTieBreak` 的关系（本裁决 MUST 明确回答，防同词两套语义）**：**判为两个无关概念**，理由是**裁决键的类型根本不同**：

| 维度 | M2 覆盖带（`OverridePriority` + `OverrideTieBreak`） | 关系表 / 槽位竞争 |
|---|---|---|
| 竞争对象 | 同一**属性**上多条 `TAO_Override` 修正器 | 同一**受限槽位**上多个运行态 / 施加态 |
| 第一裁决键 | `OverridePriority`（大者胜） | 槽位竞争表自己的规则（**形状待 `R5.5-e4` 裁**） |
| 同优先级决胜 | `OverrideTieBreak` 四值（`OTB_Max` / `Min` / `MaxAbs` / `MinAbs`）—— **数值比较**，策略住**属性定义** | **无数值可比** ⇒ 不适用该策略族 |
| 框架其余规则 | 明文"框架不定义任何其它'谁盖谁'的规则" | 成对关系由**显式声明**给全序 |

**MUST NOT 复用 `ETcsAttrOverrideTieBreak`**（`Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h:ETcsAttrOverrideTieBreak`）：它的四个值全是**数学比较**（取最大/最小/绝对值最大/最小），语义前提是"候选各带一个有符号数值"；而状态/技能冲突的候选**没有可比的数值**——强行复用会造出"用一个数值策略裁一个无数值的冲突"这种无意义配置。

**唯一共享的是"方向约定"**（不是概念）：全仓"谁更强"一律**大者胜**（先例 `FTcsEffectTriggerDef::Priority` 注释明写"与覆盖带 `OverridePriority` 同向——避免系统内出现两套方向相反的'谁更强'约定"）。本裁决**沿用该方向约定**，但**不主张概念复用**。

**同优先级决胜**：**本轮不裁**——它属槽位竞争表批次（`R5.5-e4`）。本裁决只钉死"方向 = 大者胜"与"决胜规则不继承 M2 的策略族"。

**字段的物理去留**：`FTcsBuffDef::Priority` 与 `FTcsSkillDefData::Priority` 本轮**保持原样、不动代码**（零消费者，按"零消费者不预建"纪律如实登记）。其**最终处置**（移除，或改述为"该定义声明自己参与哪个受控槽"的字段/引用）归 `R5.5-e4`——因为该决定取决于槽位竞争表**自己的**形状，今天裁它会造出两个互相依赖的悬空裁定。

**影响面**：`SPEC-02-states` §3.4 与 `SPEC-04-skill` §2 的关系字段节须改述为"三字段 + Priority 归属槽位竞争表"；`PLN-R5`《R5.5 批次表》`e` 行按 §5 重切。

### 裁决 ②：互斥组两层结构 + `Blocks` / `Cancels` 正交化

**裁定（乙层为特例、丙层为通例）**：

- **特例层（本轮可落地）**：逐对声明。载体 = 现状的 `TArray<FGameplayTag>`，**零新类型**。
- **通例降维层（可选叠加，非替代）**：类目元数据 + 族规则，把 N×M 压成 K 条（`DEC-2026-08-31` §4 第 1 条的"族规则降维"对策）。载体 = 裁决 ③ 的类目容器 + 族规则行。

**`Blocks` 与 `Cancels` 的语义差（本裁决 MUST 正面回答，否则两字段会长期重叠）**——**正面回答是：它们不是"互逆的两个字段"，而是"本状态面对冲突时的两种策略声明"**：

| 字段 | 语义（钉死） | 拒绝/驱散的**来源** | 竞争结果 |
|---|---|---|---|
| `A.Blocks ∋ B` | **退让声明**——"B 在时，**我放弃施加**" | 来源 = **A 自己** | **B 赢**（A 被拒，返回 `EAR_Rejected`） |
| `A.Cancels ∋ B` | **强夺声明**——"我施加时，**把 B 去掉**" | 来源 = **A 自己** | **A 赢**（B 被移除，原因 `ESRC_Cancelled`） |

**为什么必须两个字段都存在**（这是"是否互为反向、若是为何要两个"的正面回答）：

1. **两者都是单向声明，且都从"本状态"出发**——`Blocks` 是**自我约束**（我不去覆盖别人），`Cancels` 是**对外作用**（我主动清掉别人）。**描述的对象不同**：前者描述"我的施加条件"，后者描述"我的施加副作用"。
2. **单方面声明是常态**：A 可以声明"B 在则我退"而 B 对 A 一无所知（`A.Blocks ∋ B` 成立，B 侧零声明）；反过来 A 也可以声明"我上时清掉 B"而 B 零声明。**两种策略都不需要对方的配合** ⇒ 两个字段各自独立可用，**不冗余**。
3. **若砍掉任一个，就会逼出隐式仲裁**：只剩 `Cancels` ⇒ "我想退让"无法表达（只能靠"别人声明夺我"）；只剩 `Blocks` ⇒ "我想夺取"无法表达（同上）。而**隐式仲裁正是本族要消灭的东西**（`DEC-2026-08-31` §3 立规矩"每条规则只允许一个事实源"）。

**同一有序对上的真冲突判定（作者期校验，MUST 报 Error）**：
- `A.Blocks ∋ B` **且** `A.Cancels ∋ B` ⇒ **A 同时声明"我退"与"我夺"** = 自相矛盾 ⇒ **Error**。
- `A.Blocks ∋ B` **且** `B.Cancels ∋ A` ⇒ 两句分别说"B 赢"与"B 赢" ⇒ **一致**，不冲突（不同侧声明同一结果，合法）。
- `A.Cancels ∋ B` **且** `B.Cancels ∋ A` ⇒ **双向强夺**，无确定赢家 ⇒ **Error**。

**执行时机**（照既有规格，不改）：apply 时校验（`Blocks` 拒绝 / `Requires` 未满足拒绝）；**任何状态移除后对依赖它的存活状态做事件驱动重评**（脏标记，批量移除合并一次）。

**影响面**：`SPEC-02-states` §3.4；检查器的 `IsDataValid` 两条新增规则；`R6.5-g`（多形态组合走查）的依赖由此**部分解除**（特例层可先行）。

### 裁决 ③：族规则 = 类目容器（宿主词汇）+ 查询对另一侧求值

**裁定**：

- **载体 = 类目容器字段**，住**定义基类**（`FTcsStateDefBase`），`FGameplayTagContainer` 类型，**词归宿主**（宿主 ini 声明，插件零具体词）。
- **匹配 = `FGameplayTagQuery`（复用引擎既有类型）+ 一层"族规则行"**。
- **不开任何新根**：`State.Control.*` / `Action.*` **不建**。

**为什么是"复用 + 补一层"而不是"原样复用"（回答提示词 §2 ③ 的形状之问）**：

`FGameplayTagQuery` 表达的语义是"**这个容器满不满足查询**"——**对单个容器求值**。而族规则要表达的是"**A 域阻断 B 域**"这种**域间**关系。两者形状不同的**确切位置**：

- `FGameplayTagQuery` 是**一元谓词**：`Query.satisfies(Container) -> bool`
- 族规则是**二元关系**：`Relation(主体侧容器, 客体侧容器) -> 作用(退让/强夺)`

⇒ **补的那一层 = 族规则行**：`{ 主体侧查询(Query), 客体侧查询(Query), 作用 }`，求值 = 对客体侧每个存活实例的类目容器跑**主体侧查询**。**`FGameplayTagQuery` 本体零改动、零新自定义类型**（它是 `USTRUCT(BlueprintType)` + `AllTagsMatch()` / `AnyTagsMatch()` + 自带编辑器定制 —— 引擎既有）。

**载体选择判据（为什么用宿主词汇根而非新开根）**——**同族先例 = `DamageCategory`**：

- `openspec/specs/gameplay-tag-governance/spec.md`「根段注册表」末行登记 `DamageCategory` 的消费角色 = "**供条件匹配的伤害分类集**"，声明方 = **宿主 ini**，插件**零具体词**；同规格的 `DamageCategory` 共享说明明写"**插件 MUST NOT 声明任何具体分类词**"，且判据是"**词的归属是否唯一**，不是读取点是否唯一"。
- `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h` 的注释自己给出了这条论证："**`Category` 有同族先例**：`DamageCategory` 根在 TCS 已是'供条件匹配的伤害分类集'——本字段是**同一形态扩到定义级**，不是新造机制"。
- ⇒ 类目容器是**宿主内容词汇**，不构成本插件的新根 ⇒ **绕开"要不要开 `State.Control.*` / `Action.*`"这个硬缺口**（该缺口受"一角色一根 + 零消费者不预建根"约束，开根须先论证"解析消费者已出现"并按角色定名；用宿主词汇则无此负担）。

**`STAT-10` 与本任务的关系（提示词要求 MUST 判明）**：**判为同一套机制的两半，MUST 同批裁**。

| 面 | 载体 | 消费者 |
|---|---|---|
| **声明面**（`STAT-10` 的①） | 定义上的**类目容器**字段 | 族规则 + 条目选择器 |
| **匹配面 A**（本任务） | **族规则行**（容器 × 容器） | 关系表检查器 |
| **匹配面 B**（`STAT-10` 的②） | `FTcsSkillEntrySelector::CategoryTags`（**已落空壳**） | 技能条目选择器 `ESS_ByCategoryTags` 档 |
| **模块归属**（`STAT-10` 的③） | GameplayTag 筛选机制住哪 | 两处共用 |

⇒ **同一个字段、两个消费者**。`STAT-10` 的"载体 + 规则"两项与本裁决 ③ 是**同一份工作的两个出口**，拆开裁会让"类目容器长什么样"出现两处互相等待的裁定。**`STAT-10` 的③（是否另立 `TcsGameplayTag` 模块）本次一并收窄**：本轮**不另立模块**（筛选逻辑的落点 = 现有模块内的一处薄实现；另立模块的触发条件 = 出现"第二个域也要按类目筛选"的真实需求）。

**本轮只裁形态、不落机制**（判据 = "零消费者不预建"）：真正的消费者 = 关系表检查器（`R5.5-e1`）与条目选择器（`R6.5-g`）⇒ 类目字段与族规则行的**落地**归 `R5.5-e2`，本裁决只提供语义与形状。

**影响面**：`FTcsStateDefBase` 新增类目字段（`R5.5-e2`）；`STAT-10` 三项收窄；`R6.5-g` 的依赖链（类目载体一经落地，`ESS_ByCategoryTags` 档与 `CQM_Custom` / `MCS_Custom` 具备落地条件）。

### 裁决 ④⑤⑥（由 ①②③ 导出）

| # | 裁定 | 导出依据 |
|---|---|---|
| ④ | `State.Control.*` / `Action.*` **不建** | 裁决 ③ 用宿主词汇绕开 ⇒ 无开根需求（`gameplay-tag-governance`「新建根前先查表」：无新消费角色则不开根） |
| ⑤ | `PriorityOnly` / `PreemptionPolicy` **不作为既有设计照搬**；`PreemptionPolicy` **不建字段** | 其唯一依据库外不可复核（见 §4 错前提更正②）；按"零消费者不预建"不为仅有名字的概念造字段 |
| ⑥ | 范围边界见 §5 | 提示词 §3 其三 |

---

## 3. 现状证据表（每条判据挂可复核锚点）

> 引用格式照 `CONVENTION` §5 第 4 行：源码用 `路径:符号名`（**优先符号名，禁行号**）；设计文档用文档 ID + 节号。

### 3.1 形状与消费者

| # | 判据 | 锚点 |
|---|---|---|
| E1 | 状态侧四字段形状 | `Source/TcsState/Public/Def/TcsBuffDef.h:FTcsBuffDef::Blocks` / `::Requires` / `::Priority` / `::Cancels`；头注释明写"检查器归 R5.5-e" |
| E2 | 技能侧同款四字段 | `Source/TcsSkill/Public/Def/TcsSkillDefData.h:FTcsSkillDefData::Blocks` / `::Requires` / `::Priority` / `::Cancels` |
| E3 | **检查器零行**（四字段零消费者） | 全仓 grep `Blocks` / `Requires` / `Cancels` 的成员访问点，`Source/` 内**零命中**（命中的全是**同名不同物**：`FTcsEffectTriggerDef::Priority` ×2、`FTcsChainRedirect::Priority` ×2） |
| E4 | 类目容器字段不存在于定义基类 | `Source/TcsState/Public/Def/TcsStateDefBase.h:FTcsStateDefBase`（六字段无类目） |

### 3.2 裁决① 的判据锚点

| # | 判据 | 锚点 |
|---|---|---|
| E5 | 两表主体分工的规格出处 | `SPEC-02-states` §3.4「组织与解析栈（D3-16）」：关系表/槽位竞争表**并存**，槽位 = 施法运行/动作状态，关系 = Buff 施加态 |
| E6 | 原裁决把 Priority 挂在"槽"上 | `DEC-2026-08-31-state-relation` §4 第 2 条（原文"抢同一槽时靠显式 `Priority`"） |
| E7 | M2 那条路的裁决键是**数值比较** | `Source/TcsAttribute/Public/Attribute/TcsAttributeBandFold.h:IsStrongerTcsOverride`（优先级 → 打平才按策略比数值）；`Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h:ETcsAttrOverrideTieBreak` 四值全为数学比较 |
| E8 | 全仓"谁更强"方向约定 = 大者胜 | `Source/TcsEffect/Public/Trigger/TcsEffectTrigger.h:FTcsEffectTriggerDef::Priority` 头注释（"与覆盖带 `OverridePriority` 同向——避免系统内出现两套方向相反约定"） |
| E9 | 打断来源优先级**无载体** | `Source/TcsSkill/Public/Skill/TcsCastOps.h:FTcsCastOps::CancelCast` 头注释（自述"全仓无任何类型声明打断来源的优先级"） |
| E10 | `ESRC_Cancelled` 已为其预留读法 | `Source/TcsState/Public/State/TcsStateEnums.h:EStateRemoveCause::ESRC_Cancelled`（ToolTip："被更高优先级的共存规则取消（关系表族归 R5.5-e）"） |

### 3.3 裁决③ 的判据锚点

| # | 判据 | 锚点 |
|---|---|---|
| E11 | `FGameplayTagQuery` 是引擎既有可反射类型 | **【引证，非本机实测】**：`Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h` 头注释（自述 `GameplayTagContainer.h` 的 `FGameplayTagQuery`，`USTRUCT(BlueprintType)` + `AllTagsMatch()` / `AnyTagsMatch()` 表达式 + 自带编辑器定制）+ 台账 `STAT-10` 行内的同一引擎事实（该行记 `GameplayTagContainer.h:737`）。**取证边界（如实）**：~~本机**未定位到 UE 引擎源码树**（`C:\Program Files\Epic Games` 下只有 Launcher / 分发件，注册表无 `InstalledDirectory`）⇒ 该引擎事实**本轮**只有引证~~ → **★ 2026-10-13 同日订正：该边界为假，本条已升为【引擎源码】**——源码在 `E:\UnrealEngine\UE_5.8`（5.8.3 / CL 58210709）；`GameplayTagContainer.h` 内**确认存在** `FGameplayTagQuery`（同类内另有 `FGameplayTagQueryExpression` 与 `FGameplayTagContainer::MatchesQuery`），且 `MatchesTag` 的**父词命中子词**语义已逐字读源复核（头注释原文 `"A.1".MatchesTag("A")` → True）。**注意：本条"该类型存在"升为实测，但本行原先支持的"用它做匹配载体"这个**取舍**仍按 `DEC-2026-10-13-relation-row-shape` 作废（理由是容器+层级匹配已足够，非"无法核实"）**未经本机源码复核**，两处仓内引证**互相独立且一致**。MUST NOT 读成"已在本机核实" |
| E12 | **宿主词汇根先例** = `DamageCategory` | `openspec/specs/gameplay-tag-governance/spec.md`「根段注册表」`DamageCategory` 行（声明方 = **宿主 ini**，消费角色 = 供条件匹配的伤害分类集） |
| E13 | 插件 MUST NOT 声明具体分类词 | 同规格 `DamageCategory` 共享说明：判据是"**词的归属是否唯一**"，不是读取点是否唯一 |
| E14 | 类目扩到定义级的同族论证 | `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h` 头注释（"`Category` 有同族先例…同一形态扩到定义级，不是新造机制"） |
| E15 | 空壳字段已在、零消费者 | `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h:FTcsSkillEntrySelector::CategoryTags`；档 `ETcsSkillEntrySelectorMode::ESS_ByCategoryTags` |
| E16 | 侧向匹配已有实现形态 | `Source/TcsEffect/Public/Trigger/TcsTriggerCondition.h:FTcsTriggerCondition_HasAllTags`（分类 Tag 集上做 HasAll 匹配）；流程侧同形 `TcsFlowStepConditions.h:FTcsConditionHasAllTags` |
| E17 | 开根受"零消费者不预建根"约束 | `gameplay-tag-governance`「根段注册表」：新建根前 MUST 先查表，已有根承载同一消费角色则 MUST NOT 另立 |
| E18 | 两根不存在 | `Source/` 全仓 grep `State.Control` / `Action.` 零命中；LAC `Config/DefaultGameplayTags.ini` 11 个根无此二者 |

### 3.4 可复核性缺陷锚点

| # | 判据 | 锚点 |
|---|---|---|
| E19 | 槽位竞争的唯一依据库外不可复核 | `SPEC-02-states` §3.4 引 `TCS 报告 06`；同文件 §12.4 已自我登记"不可复核，语义拍板时 MUST 重新论证" |
| E20 | 库外报告系列已全域搜过、确不在库内 | `GLOSSARY.md` §6 第 10 条（`TCS 报告 01–13 + ZZ` 系列在 LAC 工作区与 TCS git 历史中均不存在） |

---

## 4. 错前提更正

### ① "族规则降维依赖 `State.Control.*` 阻断 `Action.*`" —— 该举例**依赖两个不存在的根**

`DEC-2026-08-31-state-relation` §4 第 1 条以 `State.Control.*` 阻断 `Action.*` 举例说明"Tag 层级把 N×M 对压成 K 条"，并称 Stun 挂 `State.Control.*` 下。

**实测**：这两个根在**插件全仓 `Source/`** 与**宿主 ini 的 11 个根**里都不存在（E18）。⇒ 该举例**不是一个可实施的设计**，而是一个**依赖未建前提的示意**。

**更正**：族规则的**机制**（把 N×M 压成 K 条）**成立且保留**；但其**载体**从"两个新 tag 根"改为"**宿主词汇的类目容器 + 查询**"（裁决 ④）。⇒ **本任务不开新根**，该举例作为"示意可用形态"保留在原文（`FROZEN` 正文不改），但**MUST NOT 被读成"已定的落地方案"**。

**处置依据**：`gameplay-tag-governance`「新建根前先查表」+ "一角色一根 / 零消费者不预建根"（E17）。

### ② "槽位竞争已验证（`TCS 报告 06`）" —— 该报告**库外不可复核**

`SPEC-02-states` §3.4 原文："槽位竞争（`PriorityOnly` 抢占 + `PreemptionPolicy` + 同优先级策略，`TCS 报告 06`（库外，已不在库内）已验证）并入 Priority 语义。"

**实测**：同文件 §12.4 已自我登记该报告**不可复核**；`GLOSSARY` §6 第 10 条记该系列（`TCS 报告 01–13 + ZZ`）在 LAC 工作区与 TCS git 历史中**均不存在**（E19 / E20）。

⇒ **`PriorityOnly` / `PreemptionPolicy` 今天是两个只有名字的概念**。"已验证"三字**不成立**。

**更正**：
1. **MUST NOT** 把这两个词当作"既有设计"照搬进实现；
2. **`PreemptionPolicy` 本轮不建字段**（"零消费者不预建"——它今天既无载体也无消费者）；
3. 槽位竞争的语义**须在 `R5.5-e4` 从判据重新推导**，推导时的可用先例**只有仓内可复核的**：`FTcsEffectTriggerDef::Priority`（大者胜 + 同优先级按登记序）与 M2 的三级阶梯（**仅作方向参照，不作概念复用**，见裁决①）。

### ③ "`R5.5-e` 是一个批次" —— 它实际是**四件独立的事**

`PLN-R5`《R5.5 批次表》`e` 行原文把四步写在**一格**里：字段语义拍板 → 检查器 → 移除级联重评 → 槽位竞争表。

**更正**：四步的**依赖关系不对称**——

| 步 | 依赖 | 可否独立开工 |
|---|---|---|
| 字段语义拍板 | 无 | ✅ 本轮（本裁决） |
| 检查器（特例层） | 语义拍板 | ✅ 依赖已满足 |
| 移除级联重评 | 检查器（要重评的正是检查器的判定） | ❌ 串行 |
| 槽位竞争表 | 主体 = **施法运行**（R6 已落地） | ✅ **与关系表正交，可独立** |

⇒ 按"验收信号纯净"的既有切分传统（`PLN-R5` §轮次路线图"R5 切分的理由"），必须**再拆批次**（见 §5）。

---

## 5. 范围边界与 Task 切分（交付给 `PLN-R5.5`）

> 本节是**切分建议与边界声明**；完整的五节执行形状（勘察 / 裁决表 / 证据 / 错前提 / Task）住 [`PLN-R5.5`](../plans/plan-r5.5-relation-table.md)。

### 5.1 明确不裁（本轮边界）

| 议题 | 为何不裁 | 归属 |
|---|---|---|
| **状态槽映射选型**（多实例 vs 单树级联） | `DEC-2026-08-31` §7 待办第 1 条，是 **StateTree 宿主形态**问题，与关系表**正交** | 独立议题（等"槽间级联"实际需求） |
| **矩阵审查视图 / 规则沙盘的工具排期** | 同 §7 待办第 3 条，是**编辑器工具**问题 | R8（`TOOLS-*`） |
| **槽位竞争表自身的 Priority 语义形状** | 取决于槽位表自己的形状；今天裁会造出两个互相依赖的悬空裁定 | `R5.5-e4` |
| **`FTcsBuffDef::Priority` / `FTcsSkillDefData::Priority` 的物理去留** | 同上（见裁决①末段） | `R5.5-e4` |

### 5.2 批次重切（`e` 拆四批）

| 新批次 | 内容 | 依赖 | 与原 `e` 的关系 |
|---|---|---|---|
| **`R5.5-e1`** | **关系表字段语义落地 + 检查器（特例层）**——`Blocks` / `Requires` / `Cancels` 三字段的判定实现；`IsDataValid` 两条新增规则（自相矛盾 / 双向强夺）；apply 时校验 | 本裁决（已满足） | 原 `e` 第 1+2 步 |
| **`R5.5-e2`** | **类目降维层**——定义基类新增类目容器字段 + 族规则行（查询 × 查询）+ `STAT-10` 三项一并落地 | `R5.5-e1`（检查器是消费者） | 原 `e` 的第 1 步通例面 |
| **`R5.5-e3`** | **移除级联重评**（脏标记批量合并） | `R5.5-e1` | 原 `e` 第 3 步 |
| **`R5.5-e4`** | **槽位竞争表**（主体 = 施法运行）——含 `Priority` 语义从判据重新推导、两处 `Priority` 字段的物理处置、打断来源优先级仲裁 | R6（**已落地**）；**与 e1/e2/e3 正交** | 原 `e` 第 4 步 |

**切分判据**（三条）：① 每批有**独立可判的验收信号**；② 依赖关系如实（`e3` 串行于 `e1`，`e4` 正交）；③ 不把"三件独立的事捆在一起"（提示词 §3 其三的要求）。

### 5.3 下游挂账的处置

| 挂账 | 本裁决的处置 |
|---|---|
| `R6.5-g`（多形态组合走查 + `CQM_Custom`/`MCS_Custom` 两档真实现） | **依赖部分解除**——裁决 ② 的特例层（`e1`）即可支撑"关系字段门禁"那一半；`CQM_Custom` / `MCS_Custom` 仍等 `e2` |
| 台账 `STAT-4`（`StatusTag` 的 tag 根归属） | **条件已部分成立**——`e1` 的检查器是 `StatusTag` 的**第一个解析消费者** ⇒ `e1` 落地时按"一角色一根"定根；本裁决**不预开根** |
| 台账 `STAT-10`（类别标识容器 + `ByCategoryTags` 档） | **判为同批裁**（裁决 ③）；三项收窄见该节 |
| `EStateRemoveCause::ESRC_Cancelled` 无内建产生者 | **产生者已定**——`Cancels` 胜出路径（裁决 ②）；实现归 `e1` |
| 打断来源优先级仲裁 | **归属已定**——槽位竞争表（`e4`），非关系表 |

---

## 6. 依据

- **拍板**：2026-10-13 本会话五项逐项裁定（用户贡献：`Priority` 归槽位竞争表、混合两层互斥结构、类目容器 + 查询对另一侧求值、独立 `DEC-*` 载体、独立 `PLN-R5.5`）。
- **本裁决的源起**：`DEC-2026-08-31-state-relation` §7 待办第 2 条。
- **形状与执行时机的唯一权威处**：`SPEC-02-states` §3.4（三字段形状 + 执行时机 + 两表并存 + 解析栈）。
- **技能侧同形状映射**：`SPEC-04-skill` §2「关系字段」（Block=禁止激活 / Require=激活前提 / Priority=顶替优先 / Cancels=激活时取消）。
- **根治理**：`openspec/specs/gameplay-tag-governance/spec.md`「根段注册表」+「新建根前先查表」+ `DamageCategory` 共享说明。
- **记忆卡**：`MEM-20261006-10`（形态题区分轴 MUST 从能力反推，MUST NOT 同族类比——本裁决①③ 直接适用）、`MEM-20260930-05` / `MEM-20261004-16` / `MEM-20261013-03`（收尾三方向扫描）、`MEM-20261006-11`（旧裁定字面与判据冲突时取判据并回写取代登记）。
- **执行计划**：[`PLN-R5.5`](../plans/plan-r5.5-relation-table.md)。
