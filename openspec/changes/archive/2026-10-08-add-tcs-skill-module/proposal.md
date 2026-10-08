# Change: 落地 TcsSkill 模块与技能 Def 资产族（PLN-R6 Task 1）

## Why

R6（M5 技能层）的第一个交付，是让"技能定义"成为**可发现、可校验、可解析**的内容资产。今天 `TcsSkill` 只活在设计文档（`SPEC-04-skill`）与目标模块清单里：`.uplugin` 没有它、没有 Def 形状、定义库也没有第五条发现路径——策划配不出任何一条技能，代码也没有按 tag 取到它的入口。

设计出处 = `SPEC-04-skill` §1（模块边界）/ §2（Def 类层级与 SkillDef）/ §7（非目标）与实施计划 `PLN-R6` Task 1；**不另开 `design.md`**——该 Task 的设计已按实施视角收窄并落纸，再写一份就是第二份真相。

本轮是 `PLN-R5` 的**同构重演**（`2026-10-04-add-tcs-state-module` 正是它的前身）：模块入场 + Def 数据形状 + 资产身份 + 编辑期表行 + 第五条发现路径 + 新根，五件事同一批落地。差异只在**依赖链层位**（`TcsSkill` 是第五层终点）与**继承关系**（`FTcsSkillDefData : FTcsStateDefBase`，白拿六个字段）。

## What Changes

- **模块清单 8 → 9**：`.uplugin` 的 `Modules` **追加于数组末尾**（`TcsIntegration` 之后）。理由：`TcsSkill` 是依赖链第五层终点，而现有 8 条已按依赖序排列——**按依赖序排在前/后都可加载**，取末尾以**保持既有 8 条顺序一字不动**，把改动面压到最小。
  **BREAKING（对规格口径）**：`plugin-descriptor` 的"恰好八个模块"与 `openspec/project.md` 的 8 计数同时失效，本变更一并订正。**需求标题不动**（R5 已把 `R3 模块物化声明` 改为 `模块物化声明`）⇒ **无需 RENAMED**。
- **新模块骨架**：`Source/TcsSkill/`——`Build.cs` 依赖 `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect` / `TcsState`；模块壳零注册；日志通道 `LogTcsSkill`。
  落位 = 依赖链**第五层（终点）**：`TcsCore ← {TcsNotation, TcsAttribute} ← TcsEffect ← {TcsDamage, TcsTargeting, TcsState} ← TcsSkill`。
  **MUST NOT** 依赖 `TcsDamage` / `TcsTargeting`（`SPEC-04-skill` §1）——链步骤是 `FInstancedStruct` 数据，运行时经执行器注册表分派，本模块代码不具名任何领域步骤类型。
- **技能 Def 数据形状**：`FTcsSkillDefData : FTcsStateDefBase`——**白拿** `StatusTag` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows`，本类补**施法语义**字段：布尔开关表 `BoolSwitches`、施法时段表 `Phases`、查询契约三档（`CastQueryMode` + `bInterruptibleDefault` / `bCanMoveDefault` + `CastQueryFragment`）、AttrCapture 声明列表、主链（`CastChainId` + `MainChainStart` + `MainChainStartPhaseTag`）、实例化 `Instancing`、关系字段（`Blocks` / `Requires` / `Priority` / `Cancels`）、内联触发行 `Triggers`。
- **`FTcsBoolSwitchRow` 落 `TcsSkill`（2026-10-06 用户裁定 Q-8，改判设计语料两处）**：`USTRUCT FTcsBoolSwitchRow{FGameplayTag Key, bool Base, ETcsParamMode Mode}`。
  依据 = **实测**（非印象）：`BoolSwitches` 的消费者**只有技能侧四处**，`SPEC-02-states` 全文**零 `BoolSwitch` 命中**；按基类自己的判据"**无时值/无堆叠 = 基类；有时值/有堆叠 = 派生**"——数值表两层共用故留基类，布尔开关技能专属故落派生类。
  **`ETcsParamMode` 仍从 `TcsState` 复用**（含 `EPM_Live`，其注释原文即"技能侧的实时通道用；R5 状态层不用"），零新编译边。
  **反向风险登记**：若将来状态/buff 也要布尔开关，把纯数据类型**下移**到 `TcsState` 是机械改动（无行为、无反射契约）——现在预建才是违反"零消费者不预建"。
- **两个 Def 资产类**：`UTcsStateDef`（族基类，R5 已建）与 `UTcsSkillDef : UTcsStateDef`（持 `SkillDef`）。
  数据结构名带 `Data` 后缀消歧，资产类保持 `UTcsSkillDef`——本轮采**"资产名已有文档在位 ⇒ 数据加 `Data`"** 这一支（属性族 / 技能族同款），与触发族 / Buff 族的"数据名已被交付类型占用 ⇒ 资产加 `Asset`"**并列成立**（`openspec/project.md:19` 的限定语本变更一并改判为二选一判据）。
- **定义库第五条按类发现路径**：`DiscoverSkillDefs()` / `ResolveSkillDef(FGameplayTag)`——`integration-entity` 的"四条按类发现路径"随之变五条；技能定义**不做**世界装配（同状态定义的"只进缓存"口径）。
- **新增一个技能定义身份根（`SkillDef`）**：`gameplay-tag-governance` 的根表 11 → 12——技能定义资产的身份解析（`DiscoverSkillDefs` / `ResolveSkillDef` / 资产身份与去重键）是一个**新的消费角色**，表内没有任何既有根承载它（`StateDef` 是状态定义资产的身份角色；先例 = `EffectTriggerDef`、`AttrModDef`）。
- **`MUST NOT 归一` 例外（本轮不动的两处）**：`UTcsBuffDefAsset` 的既有命名**不改**（本轮只登记判据，不做全库重命名）；`ECastInstancing` / `ECastQueryMode` / `EMainChainStart` **保持裸枚举名**（实测枚举两族并存：`ETcs*` 15 / 裸名 8，`EDurationPolicy` / `EStatePhase` 为既有裸名族）。
- **不做（非目标）**：账本与施法运行态（Task 2/3）、参数链修正器（Task 4）、施法时段推进与查询实现（Task 5）、**冷却与 Cost（R6.5）**、关系字段检查器（`R5.5-e`）、`PrimaryAssetTypesToScan`（R7 `INTEG-3`）、`Descriptions` 渲染与视图策略族（R8）。

## Impact

- Affected specs：
  - `plugin-descriptor`（**MODIFIED**：模块清单 8 → 9；`TcsSkill` 由"目标模块"移入"当前物化"）
  - `integration-entity`（**MODIFIED**：定义库第五条发现路径 + 技能定义的发现期身份校验三项 + "不装配"口径 + 去重键与 GC 缓存清单一并扩项）
  - `skill-def-asset`（**ADDED**：技能 Def 数据形状 / 布尔开关行落点 / 资产身份 / 编辑期表行 / 作者期校验，五条需求）
  - `gameplay-tag-governance`（**MODIFIED**：根段注册表 11 → 12，新增 `SkillDef` 根；`## Purpose` 数字**手工同步**——归档器不碰该段）
- Affected code：`TireflyCombatSystem.uplugin`；`Source/TcsSkill/**`（新模块）；`TcsIntegration` 的定义库（第五条发现路径 + 第五份缓存 + `Build.cs` 加 `TcsSkill` 依赖）；
  **`Source/TcsState/Public/Def/TcsDescriptionEntry.h`（仅一行注释改判"技能面板归 R6" → R8，零行为零契约）**；
  宿主 LAC 侧：`Config/DefaultGameplayTags.ini`（新增 `SkillDef` 根 + 检查词）
- Affected docs：`openspec/project.md`（模块计数在落地后 8 → 9、能力计数在本提案归档后 34 → 35 + `:19` 命名限定语改判为二选一判据 + 家族表补 `FTcsSkillDefData`）；`SPEC-04-skill` §2 的 `BoolSwitches` 落点父注**订正为 TcsSkill**（与 `DEC-02-fold-display:142` 同步）；`openspec/specs/state-def-asset/spec.md` 的"技能面板归 R6"改判 R8
- 验收：双配置编译（Development Editor + Game Shipping，零 warning）；装置侧断言"发现 N 个 `UTcsSkillDef`、`IsDataValid == Valid`、按 `DefTag` 可解析"；`openspec validate --all --strict` 全绿；**归档前主规格能力数仍为 34，验收后归档才新增 `skill-def-asset` 并核实 34 → 35**
