## Context

`FTcsStateDefBase` 自 R5 Task 1 起持 `FGameplayTag StatusTag`（单 tag），而关系表族（`R5.5-e`）与条目选择器（`STAT-10`）都要求一个**类别容器**。同一语义长期有两个名字，且 `DEC-2026-10-13-relation-row-shape` §4.5 曾以"`StatusTag` 单义：状态词"否掉技能侧使用它。

`PLN-REFACTOR-1`（2026-10-15 全轮收束）改变了两个前提：技能运行态现继承同一基底（`FTcsSkillInstance : FTcsStateInstance`）、技能 Def 继承同一 Def 基类（`FTcsSkillDefData : FTcsStateDefBase`）。⇒ "技能也有 StateInstance"与"技能也有 StatusTag"成为同构且同样成立的事实，§4.5 的否定判据随之瓦解。

**用户 2026-10-15 裁定**：不做"放宽 `StatusTag`"的补丁，而是**删掉 `StatusTag`、把 `CategoryTags` 作为唯一载体**——从根上消除冗余。

## Goals / Non-Goals

- Goals：① 一个定义级类别载体，单值/多值/层级匹配统一由容器表达；② 技能与状态两域共用同一字段（不再有"技能也有 X"的读不通问题）；③ 消除选择器侧与 Def 侧的同名不同型陷阱。
- Non-Goals：**不开新根**（该字段本轮仍零解析消费者）；**不做**关系表检查器（归 e1）；**不做** `Interrupt` → `Canceled` 一族改名（归 e1）；**不引入** `FGameplayTagQuery`。

## Decisions

### D1：删 `StatusTag`，`CategoryTags` 成为唯一载体（而非"两者并存、各留其用"）

**判据三条**：

1. **元数冗余**——容器 ⊇ 单值。单个 tag 能表达的一切用**单元素容器**表达；反向不成立。
2. **外部先例同向**——GAS 的 `UGameplayEffect::InheritableGameplayEffectTags` **就是容器**（`GameplayEffect.h:2346`，类型 `FInheritedTagContainer`）。GE 资产的标签从来不是单个 tag。GAS 另有 `GrantedTags` / `ActivationOwnedTags`（运行期授予 owner 的标签，tag 本体住 ASC 的 `FGameplayTagCountContainer`）——**那是另一个设施，本轮不做**（用户裁定："先不急，之后再说"）。
3. **否定判据已失效**——见 §Context。删除而非放宽，比打补丁更干净：**问题随字段消失而消失**。

**备选（未采纳）**：保留 `StatusTag` 并放宽"技能也可用"。否决理由 = 留下两个功能重叠的字段，且要为"用哪个"写分流规则。

### D2：`CategoryTags` **非必填**

原 `StatusTag` 是**必填**（定义库发现期拦空值）。合并后**取消该约束**（用户裁定）。

**判据**：容器是**筛选依据**而非身份——空容器在有消费者筛选时**自然不命中任何查询**，这是**明确结果**，不是缺省故障。拦空值属**误报**（它与"这条资产能不能被按 tag 寻址"无关，而发现期只判那一件事）。

### D3：两个查询通道**各自独立**，MUST NOT 混为并集

用户判据："查询的核心不是查一类还是查一个，而是要**查询有明确结果**。"

- `DefTag` 通道：答"是不是这个资产/这一族资产"（`DefTag` 是**唯一键**，定义库校验三拦重复）；
- `CategoryTags` 通道：答"属不属于这个语义类"（**语义词**，多个资产可共享同值，不参与去重）。

**这取代了 `DEC-2026-10-13-relation-row-shape` §4.6 甲案**（原写"主体侧匹配面 = `{StatusTag} ∪ CategoryTags`；**不含 `DefTag`**"）。新口径 = **`DefTag` 可作查询依据**（用户裁定），但**两个通道不混合**——原甲案否掉 `DefTag` 的理由是"避免同一语义两条寻址路径"，而新判据指出：**真正的风险是"混在一个并集里"导致结果不明确**，不是"有两条通道"。`DefTag` 表达不了"一类"，把它并进类别匹配会把"一族"退化成"一个资产"。

### D4：选择器字段 `CategoryTags` → `ByCategoryTags`

选择器侧字段是 **`FGameplayTagQuery`（查询）**，表达"我要匹配谁"；Def 侧新字段是 **`FGameplayTagContainer`（容器）**，声明"我是什么"。**同名不同型、语义方向相反**——这正是 §4.5 记过的"词形相近而方向相反 = 静默陷阱"（`ForbiddenTags` vs `TagsToBlock` 同款）。**且选择器档名本来就是 `ESS_ByCategoryTags`**，字段名与档名对齐后自解释。

### D5：资产迁移走**编辑器重存**，不走 `PropertyRedirects`

**实测**：`StatusTag`（`FGameplayTag`）→ `CategoryTags`（`FGameplayTagContainer`）是**类型变更**。引擎的 `PropertyRedirects`（`BaseEngine.ini:437` 有用法先例）做的是**同型属性的改名映射**，不负责类型转换 ⇒ 不适用。

**方案**：6 个资产在编辑器内重存（`StatusTag` 的值逐个搬进 `CategoryTags` 作为单元素）。**实测值已确认**：

| 资产 | 原 `StatusTag` 值 |
|---|---|
| `DA_Check_BuffDef` | `StateDef.Check.Burn` |
| `DA_State_E2E` | `StateDef.Check.StateLayer` |
| `DA_SkillDef_E2E` | `SkillDef.Check.SkillLayer` |
| `DA_SkillDef_E2E_Second` | `SkillDef.Check.SecondSkill` |
| `DA_SkillDef_E2E_Replaceable` | `SkillDef.Check.SkillLayerReplaceable` |
| `DA_SkillDef_E2E_SinglePhase` | `SkillDef.Check.SkillLayerSinglePhase` |

⇒ **每资产一个词、无多值需求**，迁移是机械的。

### D6：`Interrupt` → `Canceled` 一族**归 e1**，不在 e0

用户裁 E 说"跟着一起改名"，但**批次上归 e1**（与 `ECFR_Interrupted` → `ECFR_Canceled` / `IsInterruptibleNow()` → `CanBeCanceled()` 同批）。判据：四者是一个概念族，同批改才有单一事实源；且 e0 已含资产迁移，把改名也塞进来会让同一批资产**两次重存**。

⇒ **e0 的资产迁移 MUST NOT 改名**，只搬值。e1 再改 `bInterruptible*`（届时再重存一次，或同批用 `PropertyRedirects`——同型改名，那里适用）。

**e1 的改名清单（登记于此，防遗漏）**：
- `FTcsPhaseSpan::bInterruptible` → `bCancellable`
- `FTcsSkillDefData::bInterruptibleDefault` → `bCancellableDefault`
- `ETcsSkillCooldownTiming::ECT_OnCastInterrupted` → `ECT_OnCastCanceled`
- `ECastFinishReason::ECFR_Interrupted` → `ECFR_Canceled`
- `ITcsCastStateQuery::IsInterruptibleNow()` → `CanBeCanceled()`（含 `UTcsCastStateQuery` 与门面转发）
- **事件 tag `TcsEvent.Cast.Interrupted` 不改**（裁定 F：宿主项目该 tag 之后大概率退役）

## Risks / Trade-offs

| 风险 | 缓解 |
|---|---|
| 类型变更导致 6 个资产静默丢值（`StatusTag` 读不出、`CategoryTags` 为空） | 迁移后**逐资产人工确认**（tasks 7.4）；装置检查 18 的判据改为"容器包含该词"后会直接暴露空容器 |
| 技能侧原有 `StatusTag` 值无意义（技能 Def 从不填它） | 实测 4 个技能资产的 `StatusTag` **确实都有值**（见 D5 表），照搬即可 |
| 删除必填校验后，作者忘填类别 → 筛选静默不命中 | **这是设计意图**（D2）；有消费者时"不命中"是明确结果，e2 落地选择器真实现后可通过装置读数发现 |
| 台账合并（`STAT-4` + `STAT-10`）导致条目数变化，多处计数漂移 | 按既有纪律"计数声称 MUST 全库同批回改"：条目数 75 → 74 处 + 前缀分布 + 身份块，逐处复算 |

## Migration Plan

1. 改 `FTcsStateDefBase`（字段替换）→ 编译（**此时 6 个资产会报 `StatusTag` 丢失**，属预期）；
2. 编辑器重存 6 个资产搬值；
3. 改定义库校验（四条 → 三条）+ 装置检查 18 判据；
4. 改选择器字段名 + 调用点；
5. 双配置编译 + 无头 + PIE；
6. 归档。

**回滚**：本变更未提交且无外部消费者（该字段零解析消费者）⇒ 直接 `git checkout` 相关文件即可，无数据迁移残留。

## Open Questions

- **`CategoryTags` 是否需要独立 tag 根**？——本轮**不裁**（零解析消费者）。e1 检查器落地时按「一角色一根」查表定名；届时须**同批回改** `gameplay-tag-governance` 的 5 处数字副本（`:5` / `:19` / `:164` / `:216` / scenario `:221`）+ 由来表加行。
- **`GAS` 式"运行期向实体授予 tag"`（`GrantedTags` / `ActivationOwnedTags` + ASC 的 tag 计数容器）**——用户裁定"之后再说"。**实测 TCS 今天完全没有这一层**（`GameplayTagCountContainer` / `LooseGameplayTag` / `HasMatchingGameplayTag` 全仓零命中）。若将来要做，它是**独立设施**，与本变更的"定义级静态类别"不是一回事。
