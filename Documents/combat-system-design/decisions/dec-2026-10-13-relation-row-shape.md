# 关系表行形态与字段命名（Lyra 形态吸收）——取代 `DEC-2026-10-13-relation-semantics` 裁决②③

- **文档 ID**：`DEC-2026-10-13-relation-row-shape`
- **类型**：DEC / 决策记录（关系表**行形态**与**字段命名**）
- **状态**：FROZEN（2026-10-13 拍板；**实现状态 = 零落地**——本裁决只定形态，检查器属 `PLN-R5.5`）
- **权威范围**：关系表的**行形态（一行一个主体词 + 四个容器）、字段命名、主体侧匹配面、抢占策略的表达方式**。**不在本文**——`Priority` 的归属与槽位竞争表（承 [`DEC-2026-10-13-relation-semantics`](dec-2026-10-13-relation-semantics.md) 裁决①，**未被本文取代**）、检查器实现步骤（属 `PLN-R5.5`）
- **最后更新**：2026-10-13（**★ 2026-10-16 追加实施注记，见下方块；正文一字未改**）

> **★★ 实施注记（2026-10-16，按 `CONVENTION` §7「冻结」只追加、正文不改）**：变更 `merge-status-tag-into-category-tags`（`R5.5-e0`）已落地，**取代本文两处**：
>
> **① 取代 §4.5 的一条否定判据**。原文否掉技能侧使用 `StatusTag`，理由是「它**单义**：状态词 ⇒ 复用会让『技能也有 `StatusTag`』读不通」。**该判据的前提已随 `PLN-REFACTOR-1`（2026-10-15 全轮收束）瓦解**——技能运行态现在真的继承同一基底（`FTcsSkillInstance : FTcsStateInstance`）、技能 Def 也真的继承同一 Def 基类（`FTcsSkillDefData : FTcsStateDefBase`）⇒「技能也有 StateInstance」与「技能也有 StatusTag」是**同构且同样成立**的事实。**处置不是放宽它，而是删掉该字段**：`FTcsStateDefBase.StatusTag`（单 tag）**已删除**，代之以 `FGameplayTagContainer CategoryTags`（容器）。
> **判据三条**：① 容器 ⊇ 单值（元数冗余）；② 外部先例同向（GAS 的 `UGameplayEffect::InheritableGameplayEffectTags` 就是容器，`GameplayEffect.h:2346`，类型 `FInheritedTagContainer`）；③ 本文 §4.5 的否定判据已瓦解。**⇒ 问题随字段消失而消失，比打补丁干净。**
>
> **② 取代 §4.6 甲案的匹配面**。原文写「主体侧匹配面 = `{StatusTag} ∪ CategoryTags`；**不含 `DefTag`**」（乙案被否的理由 = 「会多出只作用于某个**资产**的寻址路径，同一语义出现两条路径」）。**用户 2026-10-15 裁定改判**：判据是「查询的核心不是查一类还是查一个，而是要**查询有明确结果**」——⇒ **`DefTag` 可作查询依据**，但**两个通道各自独立、MUST NOT 混为一个并集**：`DefTag` 通道答「是不是这个资产／这一族资产」（它是**唯一键**，定义库校验三拦重复），`CategoryTags` 通道答「属不属于这个语义类」（**语义词**，不参与去重，多资产可共享同值）。**真正的风险是「混在一个并集里」导致结果不明确**，不是「有两条通道」 <br>**〔2026-10-24 订正：本条已被推翻——最终裁定为 `CategoryTags` ∪ `{DefTag}`（并集），见文件头「★★★ 最终口径确认」注记〕**
>
> **③ 连带一条改名**：`FTcsSkillEntrySelector` 的查询字段 `CategoryTags` → **`ByCategoryTags`**。它是 `FGameplayTagQuery`（表达「我要匹配谁」），与定义侧的容器 `CategoryTags`（声明「我是什么」）**同名不同型且语义方向相反**——正是本文 §4.5 记过的同类静默陷阱（词形相近而方向相反，同 `Blocks` vs `TagsToBlock` 的判据）。改名后与档名 `ESS_ByCategoryTags` 对齐。
>
> **不受影响（MUST NOT 读成被推翻）**：**S1 行形态**（一行一主体词 + 四容器）——不变；**S2 匹配机制**（容器 + `FGameplayTag::MatchesTag` 层级匹配，**不引入 `FGameplayTagQuery`**）——不变，且本次实测再证；**S3 字段命名**（`AppliesToTag` / `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel`）——不变；**S4 `Blocks` 的受动/前置方向**——不变；**S5 撤销的那条 Error 规则**——不变；**S7 抢占策略 = 填哪几个字段**——不变；**S8 `TagsToBlock` 是新增能力**——不变。**`CategoryTags` 的 tag 根归属仍未裁**（该字段本轮落地后**仍零解析消费者**，检查器归 `R5.5-e1`、选择器真实现归 `R5.5-e2`）⇒ 台账 `STAT-4`（已与 `STAT-10` 合并）记其归属 = `PLN-R5.5` Task 1。
>
> **取证**：`EVID-2026-10-16-merge-status-tag`（双配置编译 0/0 + PIE 四轮全绿；**检查 18 新判据实测 `CategoryTags 含定义词=是`**）。
>
> **★★★ 最终口径确认（2026-10-24）**：用户再次确认 —— **「`DefTag` 与 `CategoryTags` MUST 为并集」**。
>
> **本文内部两处口径的先后关系（MUST 按此读，勿再混）**：
> - **上方 ② 的「两个通道各自独立、MUST NOT 混为一个并集」是 2026-10-16 的过渡口径**，它已被 **[DEC-2026-10-17-tag-relationship-ownership](dec-2026-10-17-tag-relationship-ownership.md) §4.2/§4 明文取代**（该文「被取代者 ②」一栏直接点名 `e0` 的 D3，新口径 = **「两者都进匹配面」**）；
> - **§4.6 甲案的精神（并集 + 单一路径）实际被恢复**，只是成员换了：`StatusTag` 项随 `R5.5-e0` 删除，`DefTag` 项被 2026-10-17 的终裁加回 ⇒ **最终式 = `CategoryTags` ∪ `{`DefTag`}`**；
> - ⇒ **§4.6 表格里的「不含 `DefTag`」与「乙案被否的理由（避免同一语义两条寻址路径）」均作废**。该表格属冻结正文、按 `CONVENTION` §7 不改，以本注记为准。
>
> **一处误推 MUST 纠正以免重犯**：上方 ② 写「`DefTag` 表达不了『一类』，把它并进类别匹配会把『一族』退化成『一个资产』」——**这是错的**。并集**不会**退化：父词仍在 `CategoryTags` 里照常覆盖一族，`DefTag` 只是**额外**多给了「精确到这一个资产」的能力。**真正的风险从来不是并集，而是「两条寻址路径」**（先按 `DefTag` 查一遍、再按 `CategoryTags` 查一遍）——那才产生不明确。
>
> **★ 真实的缺陷在实施，不在文档**：同日实施的 `f626582`（`R5.5-e1`）**按上述被取代的过渡口径落了代码**——两域的主体面都只填 `CategoryTags`，`DefTag` 从未进入匹配面；并把旧判词抄进了三处代码注释。⇒ **裁定为真、落地为假**，已登记待实施批次修复。
>
> **落地面（本轮已改，仅文档与注释）**：`openspec/specs/state-relation-check`（补并集判据 + 2 Scenario）/ `buff-def-asset`（订正「两个通道各自独立」）/ `TcsStateDefBase.h` / `TcsStateRelation.h` / `TcsBuffOps_Relation.cpp`。
>
> **⚠ 一条必须在实施时守住的边界**：本裁定只改**主体侧**（`AppliesToTag` 拿谁去匹配）。**客体侧**（`CheckRequired` / `CheckForbidden` / `CollectBlocks` / `CollectCancels` 拿到的"在跑实例词面"）是否也并 `DefTag`，**本文未裁**——两域现状都是**只并 `CategoryTags`**（`CollectLiveCategoryTags` / `LiveQuery->ForEachLiveState`），改动它会改变业务语义，须另行拍板。


> **本文是取代裁决**：它取代 [`DEC-2026-10-13-relation-semantics`](dec-2026-10-13-relation-semantics.md) 的**裁决② 与 裁决③**，并**撤销**该文在两处写下的一条 `IsDataValid` 规则（见 §3）。**裁决①（`Priority` 移出关系字段族）与裁决④⑤⑥ 未被取代、继续有效**。
>
> **为什么另立载体而不是改原文**：该文状态为 `FROZEN`，按 `CONVENTION` §7 正文不再修改，后续信息以带日期的实施注记追加。**本文是"新证据到达"型取代**（不是"字面与判据冲突"型）——取代理由是**外部实现证据**（Lyra），已在原文追加指向本文的注记。
>
> **证据标注**：【源码取证】= 本机全仓实测；【引证】= 仓内既有文本转述（未经本机独立复核）；**【外部源码】= 公开镜像读到的 Lyra 源码（本机无官方仓；见 §1 取证边界）**；【通识】= UE/GAS 公开已知做法（~~**本机无引擎源码树，未复核**~~ —— **★ 同日订正：该前提为假，见下方订正块**）；【设计判断】；【用户拍板】。
>
> **★★ 2026-10-13 同日订正（"本机无引擎源码树"为假，MUST 读）**：引擎源码**在本机** —— `E:\UnrealEngine\UE_5.8`（`Build.version` 实测 5.8.3 / CL **58210709** / `++UE5+Release-5.8`），**且 GAS 插件源码亦在**（`Engine/Plugins/Runtime/GameplayAbilities/Source/`）。早期探查找错了位置（只查 `C:\Program Files\Epic Games` 与注册表）。⇒ 本文原标【通识】的两处**已逐条读源复核**：
>
> - **`UAbilitySystemComponent::ApplyAbilityBlockAndCancelTags` 的实现**（`AbilitySystemComponent_Abilities.cpp`，本机可读）—— 实测其实现极简：`bEnableBlockTags` ⇒ `BlockAbilitiesWithTags` / `UnBlockAbilitiesWithTags`（**按 tag 计数增减 `BlockedAbilityTags`**）；`bExecuteCancelTags` ⇒ `CancelAbilities(&CancelTags, nullptr, RequestingAbility)`。**关键复核点**：**block 与 cancel 是两条独立分支、由两个独立 bool 控制**，⇒ 与本文 §3.2「Lyra 把两个方向明确分成两个字段」的读法**一致**；且 `AreAbilityTagsBlocked` 的实现是 `Tags.HasAny(BlockedAbilityTags.GetExplicitGameplayTags())` 并带注释「Expand the passed in tags to get **parents**, not the blocked tags」——**同样是容器 + 层级匹配**，与本文采用的口径同源。
> - 「`ActivationBlockedTags` 的『必须缺席』语义」—— 上条实现**支持**该读法（blocked 集合独立于 ability 自身 tags）。
>
> **本节按 `CONVENTION` §7 追加；正文其余部分一字未改。**

---

## 0. 裁定总表（2026-10-13，用户拍板）

| # | 议题 | 裁定 | 取代关系 |
|---|---|---|---|
| **S1** | **行形态** | 改取 **Lyra 的行形态**：**一行 = 一个主体词（单 `FGameplayTag`）+ 四个容器**；载体 = **中心 `UDataAsset`**，`TArray<行>` | **取代** `DEC-…-relation-semantics` 裁决③ 的"`{SubjectQuery, ObjectQuery, Action}` 族规则行" |
| **S2** | **匹配机制** | **容器 + `FGameplayTag::MatchesTag` 层级匹配**，**不引入 `FGameplayTagQuery`** | **取代** 同文裁决③ 的"复用 `FGameplayTagQuery` + 补一层" |
| **S3** | **字段命名** | 主体词 = **`AppliesToTag`**；四容器 = **`RequiredTags`** / **`ForbiddenTags`** / **`TagsToBlock`** / **`TagsToCancel`** | **修正**现 `Blocks` / `Requires` / `Cancels` 三字段名 |
| **S4** | **`Blocks` 的语义方向** | `Blocks` **是受动/前置**（"我必须缺席"）⇒ 与 `Requires` 配对，与 `Cancels` **正交** | **更正**同文裁决② 把 `Blocks` 与 `Cancels` 配成一对的读法 |
| **S5** | **撤销一条 Error 规则** | **撤销** `A.Blocks ∋ B` + `A.Cancels ∋ B` 同真 = Error 的判定（**两者同真合法，且是一种抢占策略**） | **撤销**同文 §2 裁决② 的该条 + `PLN-R5.5` Task 1 Step 3 的该条 |
| **S6** | **主体侧匹配面** | **`{StatusTag} ∪ CategoryTags}`**（甲案）；~~**不含 `DefTag`**~~ **〔2026-10-24 订正：`DefTag` 已加回 ⇒ 最终式 = `CategoryTags` ∪ `{DefTag}`；本行原判断作废，见文件头实施注记〕** | 细化 `PLN-R5.5` Task 2 的匹配面 |
| **S7** | **抢占策略表达** | **"填哪几个字段"即策略**（见表 §5.3），不需要 `Priority` 数字 | **加强**同文裁决①（`Priority` 移出本族） |
| **S8** | **新增能力** | 新增 **`TagsToBlock`（预防：我在跑期间，目标无法开始且不能开始）** —— TCS 今天表达不了 | 新增 |

---

## 1. 吸收来源与取证边界（MUST 读）

### 1.1 来源

**Lyra 的 `ULyraAbilityTagRelationshipMapping`**（`FLyraAbilityTagRelationship` + 该 `UDataAsset` 类）。

### 1.2 取证边界（如实，MUST NOT 读成"已在官方仓核实"）

| 项 | 实况 |
|---|---|
| **Epic 官方仓** | **不可达**——`github.com/EpicGames/UnrealEngine` 需授权，匿名取回 **HTTP 404**（本轮实测） |
| **本轮实际来源** | **公开镜像** [`BulkheadLtd/Lyra_5_4`](https://github.com/BulkheadLtd/Lyra_5_4)（UE 5.4 Lyra），经 GitHub API 取文件内容（base64 解码） |
| **本轮实际读过** | ① `LyraAbilityTagRelationshipMapping.h`（结构定义，sha `73d4ae17…`）；② 同 `.cpp`（三个求值函数）；③ `LyraAbilitySystemComponent.cpp` 的**消费点**（`ApplyAbilityBlockAndCancelTags` / `GetAdditionalActivationTagRequirements` / `SetTagRelationshipMapping` / `ActivationGroup` 一族） |
| **本轮未读** | ~~GAS 自身 `UAbilitySystemComponent::ApplyAbilityBlockAndCancelTags` 的**实现**（`ActivationBlockedTags` 的"必须缺席"语义属【通识】）——**本机无 UE 引擎源码树**~~ → **★ 同日已读（该"未读"与"无源码树"两个前提均为假）**：实现在 `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent_Abilities.cpp:UAbilitySystemComponent::ApplyAbilityBlockAndCancelTags`，**本机可读**。复核结论 = **block / cancel 是两条独立分支、由 `bEnableBlockTags` 与 `bExecuteCancelTags` 两个独立 bool 控制**（block ⇒ `BlockAbilitiesWithTags` / `UnBlockAbilitiesWithTags` 对 `BlockedAbilityTags` 计数增减；cancel ⇒ `CancelAbilities(&CancelTags, nullptr, RequestingAbility)`）。⇒ **§3.2「两个方向分成两个字段」的读法获 GAS 侧佐证**；（`ActivationBlockedTags` 的"必须缺席"语义）**由【通识】升为【引擎源码】**。另：`AreAbilityTagsBlocked` 用 `Tags.HasAny(BlockedAbilityTags.GetExplicitGameplayTags())` + 注释「Expand the passed in tags to get parents」，**同为容器 + 层级匹配** |
| **镜像可信度** | **未独立验证**该镜像与官方 Lyra 逐字一致（镜像为第三方 fork，非 Epic 组织）。⇒ 本文的**形态级结论**（几个字段、什么名字、怎么匹配）可信度高（三处文件互相自洽）；**逐字级引用**（注释原文、sha）以该镜像为准，**MUST NOT 当作官方出处引用** |

### 1.3 实测到的 Lyra 形状（【外部源码】）

```cpp
USTRUCT() struct FLyraAbilityTagRelationship
{
    FGameplayTag          AbilityTag;              // 行键：本行关系作用于哪个词（单 tag）
    FGameplayTagContainer AbilityTagsToBlock;      // 我用这个词 ⇒ 这些被阻断
    FGameplayTagContainer AbilityTagsToCancel;     // 我用这个词 ⇒ 这些被取消
    FGameplayTagContainer ActivationRequiredTags;  // 我用这个词 ⇒ 我额外需要这些
    FGameplayTagContainer ActivationBlockedTags;   // 我用这个词 ⇒ 我被这些阻断
};
UCLASS() class ULyraAbilityTagRelationshipMapping : public UDataAsset
{ TArray<FLyraAbilityTagRelationship> AbilityTagRelationships; };
```

**匹配机制（两条实读）**：`AbilityTag` 声明带 `meta = (Categories = "Gameplay.Action")`；求值走 **`AbilityTags.HasTag(Tags.AbilityTag)`**（主体侧命中）与 **`Tags.AbilityTagsToCancel.HasAny(AbilityTags)`**（客体侧命中）——**纯容器 + 层级匹配，无查询表达式**。三个函数（`GetAbilityTagsToBlockAndCancel` / `GetRequiredAndBlockedActivationTags` / `IsAbilityCancelledByTag`）都是**朴素线性迭代**（源码注释自述 `// Simple iteration for now`）。

---

## 2. 机制对照（同构 / 超越 / 缺失）

> 分类法依 `MEM-20260902-08`：**同构 = 确认自家方案并标注出处；超越 = 说明机制级原因；缺失 = 最小引入**。只有"缺失"类进入设计工作量。

| # | Lyra 机制 | TCS 现状 | 分类 | 处置 |
|---|---|---|---|---|
| 1 | `ActivationBlockedTags` | `A.Blocks ∋ B`（"B 在时 A 不可施加"） | **同构** | 改名 `ForbiddenTags`（S3/S4） |
| 2 | `ActivationRequiredTags` | `A.Requires ∋ B` | **同构** | 改名 `RequiredTags` |
| 3 | `AbilityTagsToCancel` | `A.Cancels ∋ B` | **同构** | 改名 `TagsToCancel` |
| 4 | **`AbilityTagsToBlock`** | **无对应字段** | **缺失（真新能力）** | 新增 `TagsToBlock`（S8） |
| 5 | 中心 `UDataAsset` + 行键 = 主体 tag | `SPEC-02-states` §3.4「组织与解析栈（D3-16）」**已写**"统一 DataTable/DataAsset 行（行 = 单个或**一类** StateDef——`StatusTag` 匹配）" | **同构** | 吸收 = **兑现既有规格**，不是新决策 |
| 6 | **容器 + 层级匹配，无查询语言** | 裁决③ 选了 `FGameplayTagQuery`（且该类型**本机不可核实**，见同文 E11） | **超越候选** | **采纳 Lyra 的更简形态**（S2）——见 §4 理由 |
| 7 | **无 `Priority`** | TCS 四字段含 `Priority` | **反向支持** | **加强**裁决①（S7） |
| 8 | `ActivationGroup`（`Independent` / `Exclusive_Replaceable` / `Exclusive_Blocking`，住 **ASC 侧**） | `ECastInstancing` + `bRetriggerOnActive`（住 Def 侧） | **同构** | 不改；**注意**：Lyra 的"独占/顶替"仲裁**也不在关系表里**——与裁决①"归槽位竞争表"同向 |

**第 5 条值得单记**：`D3-16` 早就写了"统一 DataAsset 行 + `StatusTag` 匹配"，但**从未落地**（`R5.5-e` 只落了 Def 上的三个数组）。⇒ 本轮吸收 Lyra 形态 = **回到 `D3-16` 原本的写法**，与既有规格合流，不是引入第二套组织。

---

## 3. 被本文推翻的两处（含我自己的错）

### 3.1 推翻一：`FGameplayTagQuery` 不必要（取代裁决③ 的匹配机制）

原文裁决③ 定"类目容器 + **`FGameplayTagQuery`** 对另一侧容器求值 + 一层族规则行"，理由是"`FGameplayTagQuery` 是一元谓词、族规则是二元关系 ⇒ 复用 + 补一层"。

**推翻理由（机制级，不是偏好）**：Lyra 用**同一个二元关系的需求**，仅靠**主体侧单 tag + `HasTag`/`HasAny` 层级匹配**就表达完了——**根本不需要查询表达式**。回顾裁决③ 的推导：它先认定"族规则要表达 A 域阻断 B 域"，再发现"`FGameplayTagQuery` 对单容器求值 ⇒ 形状不匹配"，于是补一层。**Lyra 给出了第三条路**：**主体词本身可以填父词** ⇒ "A 域阻断 B 域"退化为"`AppliesToTag` = A 域的父词"，**域间关系由层级匹配自然覆盖**，无需查询。

**连带好处（三条）**：① 去掉一个**本机不可核实**的引擎类型依赖；② 配置面从"写查询表达式"退化为"填 tag"（编辑器友好度差异显著）；③ 与 `D3-16` 的既有措辞合流。

**留一条触发条件**：若出现"一个主体词要同时命中**两个不相邻的父词**"这类**多条件组合**的真实需求，再评估引入查询——**今天零消费者，不预建**。

### 3.2 推翻二：`Blocks` 与 `Cancels` **不是**一对（取代裁决② 的正交化读法）

原文裁决② 写"`Blocks` = **退让声明** / `Cancels` = **强夺声明**，两者**正交**"。**正交的结论对，配对的对象错。**

**精确读 TCS 自己的字段注释**（`TcsBuffDef.h`）：

| 字段 | 原文注释 | 实为 |
|---|---|---|
| `Blocks` | "这些状态**存在时本状态不可施加**" | **前置条件：我必须缺席**（受动） |
| `Requires` | "这些状态**必须已存在**" | **前置条件：我必须已具备** |
| `Cancels` | "施加本状态时**取消**这些状态" | **对外副作用**（施动） |

⇒ **真正的配对是 `Blocks` / `Requires`（"必须缺席" vs "必须存在"，一对互补的前置条件）**；`Cancels` 与它们**正交**。

**我上一轮犯的错**：把 `Blocks` 拿去和 `Cancels` 配成"退让 vs 强夺"一对。**两个字段确实正交，但同轴的是 `Blocks`/`Requires`，不是 `Blocks`/`Cancels`。** 根源 = 我按**词形**（Block/Cancel 都是"拦"）而非**字段注释**配对，且当时**没有 Lyra 作为第三方对照**——Lyra 的 `ActivationBlockedTags`（受动/前置）与 `AbilityTagsToBlock`（施动/对外）**明确分成两个字段**，正是这两个方向的分离。

> **与裁决① 的关系**：裁决① 把 `Blocks` 读作"**退让声明**（B 在时我放弃施加）"——**该读法与"前置条件：我必须缺席"是同一件事**（都指"我不去覆盖 B"，结果都是 B 赢）。⇒ **裁决① 的诊断（`Priority` 该归槽位竞争表）不受影响、反而加强**（§5.3）。

### 3.3 连带：撤销一条 Error 规则（S5）

原文 §2 裁决② 与 `PLN-R5.5` Task 1 Step 3 各写了一条作者期校验：

> `A.Blocks ∋ B` **且** `A.Cancels ∋ B` ⇒ **自相矛盾** ⇒ Error。

**该判定撤销**——按修正后的语义（`Blocks` = 前置 / `Cancels` = 副作用），两者同真**不矛盾，且是一种有效的抢占策略**（§5.3）。**该规则会误杀一个正确且有用的配置。**

**保留的两条校验**（它们不依赖被推翻的配对）：① `A.Cancels ∋ B` **且** `B.Cancels ∋ A` ⇒ 双向强夺 ⇒ **Error**（无确定赢家）；② `A.ForbiddenTags ∋ A`（自指）⇒ **Error**（自我否定的门禁，永远不会被满足）。

---

## 4. 最终行形态（S1/S3/S6）

### 4.1 字段表

| # | 字段（建议名） | 类型 | 语义 | 与现状的关系 |
|---|---|---|---|---|
| 1 | **`AppliesToTag`** | `FGameplayTag`（**单 tag**） | **本行关系作用于哪个词**（靠层级匹配覆盖子域/一族） | **新**（行键） |
| 2 | **`RequiredTags`** | `FGameplayTagContainer` | **前置·必须存在**：我要求它们已在 | = 现 `Requires`，**语义不变、仅改名** |
| 3 | **`ForbiddenTags`** | `FGameplayTagContainer` | **前置·必须缺席**：我要求它们不在 | = **现 `Blocks`**，**语义不变、仅改名**（★ 见 §4.2 陷阱） |
| 4 | **`TagsToBlock`** | `FGameplayTagContainer` | **对外·预防**：我在跑期间，它们无法开始且保持不能开始 | **真新能力**（S8） |
| 5 | **`TagsToCancel`** | `FGameplayTagContainer` | **对外·终止**：我在跑时，把它们已有的终止掉 | = 现 `Cancels`，**语义不变、仅改名** |

### 4.2 ★ 迁移陷阱（MUST 读，防改名变成语义反转）

**现有 `Blocks` 映射到 `ForbiddenTags`（受动/前置），不是 `TagsToBlock`（施动/对外）。**

两格的词形极近（`Blocks` ↔ `TagsToBlock`），而**语义方向相反**：

- 现 `A.Blocks ∋ B` = "B 在时 **A 不可施加**" ⇒ **约束 A 自己** ⇒ `A.ForbiddenTags ∋ B`
- 新 `A.TagsToBlock ∋ B` = "我在跑期间，**B 不能开始**" ⇒ **约束 B** ⇒ 是另一个字段

**若把 `Blocks` 直接映射到 `TagsToBlock`，等于把"约束自己"改成"约束别人"** —— 静默的语义反转（两个 Def 都零消费者、编译器不会报错）。**判据 = 回归 `TcsBuffDef.h` 的原文注释**（§3.2 表）。

### 4.3 为什么这是"三改名 + 一新增"，不是五个新字段

`RequiredTags` / `ForbiddenTags` / `TagsToCancel` 三格**语义与现有字段逐字相同**，只是名字变了（且 `Blocks` 的改名是为消除 §5.2 的施动/受动歧义）。**实质新增只有 `TagsToBlock` 一格。**

### 4.4 载体与落点

| 项 | 裁定 | 依据 |
|---|---|---|
| 载体 | **中心 `UDataAsset`**（派生 `UTcsRelationMappingAsset` 一类），持 `TArray<行>` | `D3-16` 已写"统一 DataTable/DataAsset 行"（§2 第 5 条）；Lyra 同形 |
| 住哪个模块 | **`TcsState`**（两族的共同下层；`TcsSkill` 依赖它，MUST NOT 反向） | 承 `PLN-R5.5` Global Constraints 的依赖铁律 |
| Def 上三数组的去留 | **归 `R5.5-e4`**（与 `Priority` 同批处置——两者都是"字段该不该留在 Def 上"的同一类问题） | 承 `DEC-…-relation-semantics` 裁决① 末段的同款判据 |
| 解析栈（全局/模式/角色覆盖） | **仍留 `D3-16` 原判**（今天零消费者） | 不在本轮范围 |

### 4.5 命名判据（用户拍板，逐条记理由）

**主体词 = `AppliesToTag`**。它 MUST 同时承载"具体定义 / 类别"两层含义（靠层级匹配统一：填具体词只作用于它，填父词覆盖一族）。**逐候选否决**：

| 候选 | 否决理由（仓内事实） |
|---|---|
| `AbilityTag`（Lyra 原名） | "Ability"= 技能；而本表**同时管状态（`FTcsBuffDef`）与技能** ⇒ 词域过窄 |
| `DefTag` / `CategoryTag` | 用户已排除；且二者各指一层，都会把另一层读丢 |
| **`SubjectTag`** | **`Subject` 在 TCS 已被实体占用**（`FTcsParamEvaluateContext::Subject` / `FTcsStateEventPayload::Subject` = 实体句柄）⇒ 会与"被施加方实体"混 |
| **`TargetTag`** | 同上——`FTcsEffectContext::Targets` 已是实体 |
| `StatusTag` | 已是 `FTcsStateDefBase` 的既有字段（**单义**：状态词）⇒ 复用会让"技能也有 StatusTag"读不通 |
| `ScopeTag` / `MatchTag` | 可用，但一个偏抽象、一个是机制视角 ⇒ 都需注释才能读懂 |

⇒ 取 **`AppliesToTag`**（使用点自解释：`SubjectTags.HasTag(Row.AppliesToTag)`）。

**前置·缺席 = `ForbiddenTags`**（用户提"`Blocked` 有疑虑"，复核后同意）。**`Blocked` 的两处问题**：

1. **方向读不出**——`Blocked` 是被动分词却**不带施动者**，且与 `TagsToBlock` **只差两个字母**（`BlockedTags` vs `TagsToBlock`），紧邻时肉眼几乎不可分；
2. **配错对的诱因**——它的词形让人以为和 `TagsToBlock` 是一对，而**它真正的对偶是 `RequiredTags`**（前置条件的正反两面）。

⇒ 用 **`ForbiddenTags`**：与 `RequiredTags` 构成干净的**前置反义对**（必须存在 / 必须缺席），描述的是**约束本身**而非"别人对我做的事"。**留档备选** = `AbsentTags`（更字面，但 "absent" 易被读成"当前缺席"而非"要求缺席"）。

**其余两名沿用 Lyra**：`TagsToBlock` / `TagsToCancel`（分词形态统一、`TagsTo*` 前缀成对）。

### 4.6 主体侧匹配面（S6，用户拍板甲案）

| 方案 | 主体侧容器 | 裁定 |
|---|---|---|
| **甲** ⭐ | **`{StatusTag} ∪ CategoryTags`** | **采纳**——关系表约束的是**语义**，不是资产打包；单一寻址路径、无歧义；且族规则（父词覆盖一族）由此成立 |
| 乙 | 甲 **+ `DefTag`** | 否——会多出"只作用于某个**资产**"的寻址路径，同一语义出现两条路径 |
| 丙 | 仅 `StatusTag` | 否——会架空类目层（台账 `STAT-10`） |

**匹配语义**：主体侧**并集**上做 `HasTag(Row.AppliesToTag)` **层级匹配**（父 tag 命中全部子词——既有先例 `TcsParamChain.cpp` 的 `DefTag.MatchesTag(Filter)`）；客体侧做 `HasAny(SubjectTags)`。**不引入通配新语法**（承 `STAT-4` 行同批记的那条）。

### 4.7 与 `STAT-10` 的关系（不变）

本裁决**不改变** `DEC-2026-10-13-relation-semantics` 裁决③ 的**这条结论**：`STAT-10`（类别标识容器 + `ByCategoryTags` 档）**判为与关系表族同批裁**，因为**声明面是同一个字段**、两个消费者。**只换匹配机制，不换"同批"这个判断。**

---

## 5. 抢占策略 = "填哪几个字段"（S7）

### 5.1 本节取代原文的"两条 Error 规则"

### 5.2 施动 / 受动两个方向的分离（消歧的根据）

| 方向 | 字段 | 谁被约束 |
|---|---|---|
| **受动**（我自己的门禁） | `RequiredTags` / `ForbiddenTags` | **我** |
| **施动**（我对外的影响） | `TagsToBlock` / `TagsToCancel` | **目标** |

`ForbiddenTags` 与 `TagsToBlock` **可以表达同一件互斥**，但**从相反的一侧**：

- `A.TagsToBlock ∋ B` = "A 在跑期间 B 不能开始"
- `B.ForbiddenTags ∋ A` = "A 在时 B 不可施加"

⇒ **两条独立声明路径**。这不是冗余，而是**让作者选哪一侧拥有声明**（同族先例：`Cancels` 与 `ForbiddenTags` 也能从两侧表达同一驱逐）。

**连带一个必须一起定的执行细节**（进 `PLN-R5.5` Task 1）：**`ForbiddenTags` 的检查时机**。规格现在的执行时机是"apply 时校验 + **任何状态移除后**重评" ⇒ **A 已在跑、B 后到**时，若只有 `A.TagsToBlock ∋ B`（施动侧声明），B 的 apply 门禁**需要反向查找**才能拒绝。两条路：① 要求互斥**双向声明**（零新机制，但作者负担）；② apply 时做**反向查找**（"是否有存活实例的 `TagsToBlock` 覆盖我"）。**归属 = `PLN-R5.5` Task 1 的 Step，届时二选一并写清判据**；本裁决只钉死"两种声明路径都合法"。

### 5.3 填法即策略（取代被撤销的 Error 规则）

| 配置 | 净语义（B 在时施加 A） | 谁赢 | Lyra 对应 |
|---|---|---|---|
| 只 `A.TagsToCancel ∋ B` | A 上、B 被清 | **后到者赢** | `AbilityTagsToCancel` |
| `A.ForbiddenTags ∋ B` **+** `A.TagsToCancel ∋ B` | A 被拒（B 在）⇒ 只能等 B 自然消失 | **在位者赢** | `ActivationBlockedTags` + `AbilityTagsToCancel` |

⇒ **两者同真合法**，且表达**两种不同的抢占策略**。**"谁顶掉谁"由"填哪几个字段"决定，不需要 `Priority` 数字。**

### 5.4 这加强了裁决①（不是削弱）

承 §2 第 7 条：**Lyra 也没有 `Priority`**；它的"独占/顶替"仲裁住在 `ActivationGroup`（**ASC 侧**，不在关系表里）。

⇒ 两份独立实现（TCS 原裁决的"挂槽"读法 + Lyra 的 ASC 侧 `ActivationGroup`）**同向**：**关系表管"能不能/要不要清"，不管"谁更强"**；后者的归宿 = **槽位竞争表**（`R5.5-e4`）。**裁决① 继续有效且获得第三条佐证。**

---

## 6. 影响面

### 6.1 直接受影响的文档与产物

| 载体 | 动作 |
|---|---|
| `DEC-2026-10-13-relation-semantics` | **追加日期注记**（正文不改）：裁决②③ 被本文取代、§2 裁决② 的 Error 规则撤销；裁决①④⑤⑥ 继续有效 |
| `PLN-R5.5` | Task 1 的**形状与两条 Error 规则**改写（Step 1/3）；Task 2 的**族规则行 → Lyra 行形态**、**匹配机制 → 容器 + 层级匹配**、匹配面 → 甲案；Task 1 新增"`ForbiddenTags` 检查时机"Step；风险表更新 |
| `SPEC-02-states` §3.4 | 族规则那段的就地注记需再订正一次（`FGameplayTagQuery` → 容器 + 层级匹配；行形态；字段命名） |
| `SPEC-04-skill` §2 关系字段节 | 同上 + 字段改名（`Blocks` → `ForbiddenTags` 等） |
| 两个 Def 头（`TcsBuffDef.h` / `TcsSkillDefData.h`） | **代码改名归 `R5.5-e1`**（零消费者，零迁移成本）；**本轮不动代码** |
| 台账 | `STAT-4` / `STAT-10` 的措辞再对齐（两者归属不变）；**是否新增条目**：`TagsToBlock` 是新能力，但**有 Task 归属**（`R5.5-e2`）⇒ 按入册判据第 3 条**不入册** |
| `GLOSSARY` §5.1 | 新增本文一行（`DEC-2026-10-13-relation-row-shape`） |

### 6.2 不受影响（MUST NOT 被读成被推翻）

- **裁决①**（`Priority` 移出关系字段族 → 槽位竞争表）—— **加强**（§5.4）；
- **裁决④**（`State.Control.*` / `Action.*` 不建根）—— 不变（§2 第 6 条与本文同向：都不需要新根）；
- **裁决⑤**（`PriorityOnly` / `PreemptionPolicy` 不照搬、`PreemptionPolicy` 不建字段）—— 不变；
- **裁决⑥**（范围边界：§7 三框只有第 2 条属本任务）—— 不变；
- **`STAT-10` 同批裁** —— 不变（§4.7）；
- **`R6.5-g` 依赖分两半（`e1` / `e2`）** —— 不变。

---

## 7. 依据

- **拍板**：2026-10-13 本会话（用户贡献：指出 Lyra 先例、指定"开取代裁决 + 同步修订"、命名取舍、匹配面甲案）。
- **吸收来源**：Lyra `ULyraAbilityTagRelationshipMapping`（**公开镜像**，见 §1.2 取证边界）。
- **分类法**：`MEM-20260902-08`（外部设计的吸收方式——同构/超越/缺失三分类，**缺失才引入**）。
- **配对错误的教训**：`MEM-20261001-16`（命名争议 = 真实语义边界未知的信号——本文 §3.2 与 §4.5 即按"回读字段注释 + 第三方对照"定案）。
- **取代登记纪律**：`MEM-20261006-11`（旧裁定字面与判据冲突时取判据 + **MUST 显式回写取代登记**）。
- **被取代的原文**：[`DEC-2026-10-13-relation-semantics`](dec-2026-10-13-relation-semantics.md) 裁决②③。
- **执行计划**：[`PLN-R5.5`](../plans/plan-r5.5-relation-table.md)。
- **既有规格**：`SPEC-02-states` §3.4「组织与解析栈（D3-16）」（统一 DataAsset 行 + `StatusTag` 匹配——本文与之合流）。
