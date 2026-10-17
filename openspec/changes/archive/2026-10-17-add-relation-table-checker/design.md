## Context

关系表族自 `DEC-2026-08-31-state-relation` 立题，经 `DEC-2026-10-13-relation-semantics`（语义）与 `DEC-2026-10-13-relation-row-shape`（行形态，吸收 Lyra 的 `ULyraAbilityTagRelationshipMapping`）两轮拍板，**字段形状已落但检查器零行**。`PLN-REFACTOR-1`（2026-10-15）完成后运行态收敛为**基底 + 两 payload**（`FTcsSkillInstance : FTcsStateInstance`、`FTcsBuffInstance : FTcsStateInstance`），`e1` 的开工前提解除。

**三条硬约束**：
1. **`TcsSkill` MUST NOT 依赖 `TcsBuff`**（`PLN-REFACTOR-1` 验收明文：Build.cs 与 include 双向核）⇒ 跨域访问只能靠**类型擦除契约**，由 `TcsBuff` 向下注册；
2. **`TcsState` 是共享层**，不能 include 任一域的 Def/运行态类型（成环）⇒ 检查器形参**必须是纯值**；
3. **不开新 tag 根**：`CategoryTags` 的词是宿主内容词汇（A10/E13）。

## Goals / Non-Goals

- Goals：施加态与施法态的共存冲突有确定结果；`EBRC_Cancelled` 有内建产生者；关系行成为可复用类型；`STAT-4` 闭合。
- Non-Goals：类目降维层（`e2`）；`Priority` 的任何判定（`e4`）；移除级联重评（`e3`）；中心 `UDataAsset` 载体的落地（`e2`）。

## Decisions

### D1：关系行的归属——「行类型在 e1，载体来源分期接」

**问题**：计划 Task 1 让检查器读 **Def 上的字段**；决策 §4.4 说载体 = **中心 `UDataAsset`**、且「Def 上三数组的去留**归 `e4`**」；计划 Task 2 又把关系行落在 `TcsStateRelationMapping.h`。三处不一致。

**裁定（用户 2026-10-16）**：e1 落 **`FTcsRelationRow` 类型 + 求值器**，**来源先只接 Def 字段**；e2 接中心 `UDataAsset` 时**复用同一类型**，不重写。

**判据**：决策 §4.4 裁的是「**载体归谁**」（那个中心资产的落地归 e2/e4），不是「e1 不许有行类型」。若 e1 只做"直接在 Def 数组上判"，e2 必然另起一套行结构 ⇒ **同一语义两处表达**（本仓已记过的漂移模式 `MEM-20261016-05`）。分期接来源 = 一处类型、两个来源。

**Alternatives considered**：
- 直接落中心 `UDataAsset`（按 §4.4 字面）⇒ 否：e1 范围暴涨，且提前替 `e4` 决定 Def 数组去留；
- e1 不定义行类型 ⇒ 否：e2 重写。

### D2：主体侧匹配面——**只走 `CategoryTags` 一条通道，不做并集**

`DEC-2026-10-13-relation-row-shape` §4.6 甲案原写「主体侧 = `{StatusTag} ∪ CategoryTags`」。**该写法已被 `e0` 取代**（`StatusTag` 已删除）：
- `e0` D3 明文：`DefTag` 通道与 `CategoryTags` 通道**各自独立、MUST NOT 混为一个并集**，判据 =「查询的核心是**查询有明确结果**」；
- `DefTag` 是**唯一键**（去重键）、`CategoryTags` 是**语义词**（不参与去重）——两者语义不同，并集会让"一族"退化成"一个资产"。

⇒ e1 的主体侧词集 = **该定义实例的 `CategoryTags`**；`DefTag` 的族匹配已由 `ESS_ByDefTag` 档承接（`skill-param-chain`），**不重复**。

### D3：跨域访问契约（N1b 双向注册）

**问题**：技能侧要判"我能否激活"，须读**状态侧**的在飞实例（`TagsToBlock` 反向查找）；而 `TcsSkill` 不许依赖 `TcsBuff`。

**裁定**：共享层定义**类型擦除的查询契约**（纯值视图 + 委托），`TcsBuff` 在子系统初始化时**向下注册**实现；`TcsSkill` 只持契约引用。形状照既有先例 `FTcsEntityAdmission`（该类型已证明"共享层定义最小公共字段集 + 域侧填"这条路线可行）。

**MUST**：契约形参只有纯值（`FGameplayTag` / `FGameplayTagContainer` / 实体句柄 / 委托），**MUST NOT** 出现任一域的 Def 或运行态类型。

### D4：`ForbiddenTags` 的检查时机——**反向查找**（用户裁定 ②）

**问题**：`A` 已在跑、`B` 后到时，若互斥**只由施动侧声明**（`A.TagsToBlock ∋ B`），`B` 的 apply 门禁需**反向查找**才能拒绝。

两条路：① 要求互斥**双向声明**（零新机制，作者负担）；② apply 时做**反向查找**（每次 apply 一次扫描）。

**裁定 = ②**（用户 2026-10-16：「apply 反向查找（单向声明即生效）」）。**判据**：作者负担是**永久**成本（每条互斥都要写两遍、且漏写即静默失效），扫描是**O(该实体在册实例数)** 的一次遍历——后者与"该实体已有几个状态"同阶，而 apply 本身就是该量级的操作。且单向声明即生效消除了"漏写一侧"这一整类静默缺陷。

**成本实测义务**：装置须给出该实体的在册实例数与一次 apply 的扫描次数读数（非性能断言，只证"扫描边界 = 该实体桶"）。

### D5：`Blocks` → `ForbiddenTags`（**MUST NOT** 映射到 `TagsToBlock`）

承 `DEC-2026-10-13-relation-row-shape` §4.2 的**迁移陷阱**：

| 现字段 | 语义 | 映射目标 |
|---|---|---|
| `A.Blocks ∋ B` | "B 在时 **A 不可施加**" = 约束**自己** | **`A.ForbiddenTags ∋ B`** |
| （新增）`A.TagsToBlock ∋ B` | "我在跑期间 **B 不能开始**" = 约束**目标** | 新字段，**与 `Blocks` 无关** |

两格词形极近（`Blocks` ↔ `TagsToBlock`）而**方向相反**；错映射 = 静默语义反转，且**两个 Def 都零消费者、编译器不报错**。

### D6：改名族（`Interrupted` → `Canceled`）

四者是一个概念族，同批改才有单一事实源。用户 2026-10-13 裁定"跟着 e0 一起改名"，但**批次上归 e1**（`e0` 已含 6 资产迁移，塞进来会让同一批资产两次重存）。

**⚠️ 反向核义务**（承 `MEM-20261016-10`）：改名扫描 MUST 按「**新名 + 宿主类型**」反向核，**只按旧名扫有结构性盲区**——`e0` 实测踩过（`Selector.CategoryTags` 因左值形态不同而漏改，随后的"残留扫描"用旧名 grep 扫不到）。

### D7：`CategoryTags` **不开插件根**，`STAT-4` 就地闭合

决策与计划原文都写「e1 落地时按『一角色一根』**定根**」。**实测推翻**：
- `Config/DefaultGameplayTags.ini` 里 `StateDef.Check.*` 这类词**全部标注"宿主声明"**；
- `e2` 边界 A10/**E13 明文**：插件 MUST NOT 声明具体分类词；
- 「零消费者不预建根」在同批仍适用。

⇒ **裁定（用户 2026-10-16）**：`CategoryTags` 是**宿主内容词汇**，插件侧**不开根**。`STAT-4` 的闭合形态 = **一条裁定**（"词归宿主 ini 声明"），**不是**新增第 13 根 ⇒ `gameplay-tag-governance` 根表 12 与 5 处数字副本（`:5`/`:19`/`:164`/`:216`/scenario `:221`）**不变**。

**附带修正**：`Config/DefaultGameplayTags.ini:39` / `:59` 的 `DevComment` 仍写"兼状态词（`Def.StatusTag` 同词）"——字段已删，属 `e0` 残留，本变更一并订正。

### D8：载荷补 `Reason`

`FTcsCastEventPayload` 新增 `ECastFinishReason Reason`（`BlueprintReadOnly`）。**判据**：`Interrupted` 与 `Replaced` **发同一枚事件**（`TcsEvent.Cast.Interrupted`），订阅者此前只能靠"哪枚事件"反推，**无法区分**"我被打断了"与"我被自己的下一次施法顶掉了"——而这个区分是 `ECastFinishReason` 注释自己声明的存在理由（"两者对订阅者语义不同"）。

## Risks / Trade-offs

| 风险 | 缓解 |
|---|---|
| 四字段改名后**遗漏调用点**（`e0` 实测踩过） | 按 `MEM-20261016-10` 做**双向核**：新名 + 宿主类型反查，不只按旧名扫 |
| 反向查找被误读为"每次 apply 全实体扫描" | 装置给出扫描边界读数（`D4` 成本实测义务）；扫描域 = **该实体桶**，非全局 |
| `bInterruptible` 改名影响 2 个资产（**类型未变**） | 同型改名 ⇒ `PropertyRedirects` **适用**（与 `e0` 的类型变更不同）；本次走编辑器重存 + 磁盘级复核 |
| 关系门插在共存决策前，改变既有 apply 行为 | 零消费者字段的**默认为空** ⇒ 空容器不命中任何判定 ⇒ **既有资产行为逐字不变**（装置须有该阴性对照） |
| `CategoryTags` 含类别词而关系表零行 ⇒ 行为不变 | 这是**设计意图**（D2）；装置须有一条"配了类别词但无关系行 ⇒ 照常施加"的对照 |

## Migration Plan

1. 改两个 Def 的四字段（改名 + 新增）→ 编译；
2. 编辑器重存 2 个含 `bInterruptible` 的资产（**同型改名**，走 `PropertyRedirects` 或重存）；
3. 落检查器 + `FTcsRelationRow` + 跨域契约；
4. 接 apply / remove / 技能门禁三处挂点；
5. 改名族 + 载荷 `Reason`；
6. 作者期两条 Error；
7. 双配置编译 0/0 → 无头 → **PIE**；
8. 归档 + 台账 `STAT-4` + 文档同步。

**回滚**：字段零消费者、资产仅 2 个 ⇒ `git checkout` + 重存即可。
