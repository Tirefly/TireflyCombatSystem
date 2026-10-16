# Change: Def 类别容器统一（`StatusTag` 并入 `CategoryTags`）

## Why

`FTcsStateDefBase` 今天持一个**单个** tag 字段 `StatusTag`，而关系表族（`R5.5-e`）与条目选择器（台账 `STAT-10`）都要求一个**类别容器**。两者功能重叠：一个字段两种名字、一套语义两条寻址路径，是"同一件事两个入口"的既有反模式。

**三条判据**（逐条可核）：

1. **元数冗余**——容器 ⊇ 单值。`StatusTag: FGameplayTag` 能表达的一切，`CategoryTags: FGameplayTagContainer` 都能表达；反之不成立（`SPEC-02-states` §3.4 原文即要求"行 = 单个**或一类** StateDef"）。
2. **外部先例同向**——GAS 的 `UGameplayEffect::InheritableGameplayEffectTags` **就是容器**（`GameplayEffect.h:2346`，类型 `FInheritedTagContainer`）。GE 资产的标签从来不是单个 tag。
3. **一条既有否定已失效**——`DEC-2026-10-13-relation-row-shape` §4.5 曾以"`StatusTag` 单义：状态词 ⇒ 复用会让『技能也有 StatusTag』读不通"否掉技能侧使用。**该判据的前提已被 `PLN-REFACTOR-1` 瓦解**：技能运行态现在真的继承同一基底（`FTcsSkillInstance : FTcsStateInstance`）、技能 Def 也真的继承同一 Def 基类（`FTcsSkillDefData : FTcsStateDefBase`）⇒"技能也有 StateInstance"与"技能也有 StatusTag"是同构且同样成立的事实。**删掉该字段而非放宽它，比打补丁更干净。**

## What Changes

- **BREAKING**：`FTcsStateDefBase` 删除 `FGameplayTag StatusTag`，新增 `FGameplayTagContainer CategoryTags`（**非必填**）；
- **BREAKING**：定义库状态侧发现期校验由**四项收为三项**（删 `Def.StatusTag` 非空那条）；
- **BREAKING**：`FTcsSkillEntrySelector` 的查询字段 `CategoryTags` 改名 `ByCategoryTags`（**消歧**：Def 侧是声明"我是什么"的容器，选择器侧是表达"我要匹配谁"的查询，同名不同型且方向相反 = `§4.5` 记过的同类静默陷阱）；
- **兼容性**：6 个已落资产的序列化属性名由 `StatusTag` 改为 `CategoryTags`（编辑器重存或 `PropertyRedirects`）；
- **边界（裁 E 的批次归属，MUST 读）**：`Interrupt` → `Canceled` 一族改名（`ECFR_Interrupted` / `IsInterruptibleNow()` / `bInterruptible*` / `ECT_OnCastInterrupted`）**归 `e1`，不在本变更**——判据 = 四者是一个概念族，同批改才有单一事实源；且本变更已含资产迁移，再塞改名会让同一批资产**两次重存**。**本变更的资产迁移 MUST NOT 改名**（清单与理由见 design.md §D6）；
- **挂账合并**：台账 `STAT-4`（状态词的 tag 根归属）与 `STAT-10`（类别标识容器 + `ByCategoryTags` 档）**合并为一条**——两者本是同一个字段的两个面；
- **不开新根**：`CategoryTags` 在 e0 落地时**仍零解析消费者**（检查器归 e1、选择器真实现归 e2）⇒ 按「零消费者不预建根」，`STAT-4` 的定根动作**顺延至 e1**（检查器落地同批）。

## Impact

- Affected specs: `state-def-asset`、`skill-def-asset`、`integration-entity`、`skill-param-chain`、`gameplay-tag-governance`
- Affected code:
  - `Source/TcsState/Public/Def/TcsStateDefBase.h`（字段替换）
  - `Source/TcsIntegration/Private/TcsDefinitionSubsystem_State.cpp`（校验四条 → 三条）
  - `Source/TcsBuff/Private/State/TcsBuffOps_Snapshot.cpp`（日志读数改名）
  - `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h` + `Private/Skill/TcsParamChain.cpp`（字段改名 + 具名报出文案）
  - `Source/TcsSkill/Public/Def/TcsPhaseSpan.h` / `TcsSkillDefData.h` / `TcsCooldownTypes.h`（`Interrupt` → `Canceled` 三处）
  - LAC 宿主装置（`Source/TcsDev/`）的检查 18 读数与三处 `bInterruptible` 夹具
- Affected content: 6 个资产（`DA_Check_BuffDef` / `DA_State_E2E` / 4 个 `DA_SkillDef_E2E*`），其中 2 个另含 `bInterruptible*`
- 关联：`DEC-2026-13-relation-row-shape` §4.5/§4.6 的**取代登记**（另见 design.md §Decisions）
