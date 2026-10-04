# Change: 落地 TcsState 模块与状态 Def 资产族（PLN-R5 Task 1）

## Why

R5（M3 状态层）的第一个交付，是让"buff 定义"成为**可发现、可校验、可解析**的内容资产。今天 `TcsState` 只活在设计文档（`SPEC-02-states`）与目标模块清单里：`.uplugin` 没有它、没有 Def 形状、定义库也没有第三条发现路径——策划配不出任何一条 buff，代码也没有按 tag 取到它的入口。

设计出处 = `SPEC-02-states` §2（Def 类层级）/ §6（依赖方向）/ §12（R5 收窄）与实施计划 `PLN-R5` Task 1；**不另开 `design.md`**——该 Task 的设计已按实施视角收窄并落纸，再写一份就是第二份真相。

## What Changes

- **模块清单 7 → 8**：`.uplugin` 的 `Modules` 在 `TcsEffect` 之后、`TcsTargeting` 之前插入 `TcsState`（`Default` 相位）；`TcsState` 由"目标架构模块"移入"当前物化模块"。
  **BREAKING（对规格口径）**：`plugin-descriptor` 的"恰好七个模块"与 `openspec/project.md` 的 7 / 11 计数同时失效，本变更一并订正（需求标题同时由 `R3 模块物化声明` 改为 `模块物化声明`）。
- **新模块骨架**：`Source/TcsState/`——`Build.cs` 依赖 `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect`；模块壳零注册；日志通道 `LogTcsState`。
  落位 = 依赖链第四层：`TcsCore ← {TcsNotation, TcsAttribute} ← TcsEffect ← {TcsDamage, TcsTargeting, TcsState} ← TcsSkill`。
- **状态 Def 数据形状**：`ETcsParamMode`（`EPM_Snapshot` 默认 / `EPM_Live`）、`FTcsNumericParamRow`、`FTcsStateDefBase`（抽象、编辑器隐藏）、`FTcsBuffDef`、编辑期表行 `FTcsBuffDefTableRow`。
- **描述配置载体（本轮最小集，2026-10-04 用户裁定甲案）**：`FTcsDescriptionEntry` + `FTcsDescriptionViewSlot` 两个纯数据 struct——`FTcsStateDefBase.Descriptions` 的类型**全库尚不存在**（TcsNotation 今天只有值约定与日志通道），
  而 `SPEC-02-states` §12.4 已裁定"本轮只落字段与作者侧校验" ⇒ 本变更只补这两个载体（**住 TcsState**，依据 `DEC-02-fold-display` v3 命名批"描述视图槽位 / 配置条目 → TcsState（Def 形状所在）"）；
  `FTcsParamView` 基类、四个内置视图、`FTcsViewBuildContext` / `FTcsViewProbe` 与组装器仍归 R8。
- **两个 Def 资产类**：`UTcsStateDef : UPrimaryDataAsset`（族基类，持 `DefTag`）与 `UTcsBuffDefAsset : UTcsStateDef`（持 `BuffDef`）。
  资产类名带 `Asset` 后缀消歧——UHT 按"去前缀引擎名"判重，`UTcsBuffDef` 与数据 `FTcsBuffDef` 同名在 UHT 阶段即失败（先例 `UTcsEffectTriggerDefAsset`）。
- **定义库第三条按类发现路径**：`DiscoverStateDefs()` / `ResolveStateDef(FGameplayTag)`——`integration-entity` 的"两条按类发现路径"随之变三条；状态定义**不做**世界装配（无"每世界登记一次"的语义）。
- **新增一个状态定义身份根（`StateDef`）**：`gameplay-tag-governance` 的根表 9 → 10——状态定义资产的身份解析（`DiscoverStateDefs` / `ResolveStateDef` / 资产身份与去重键）是一个**新的消费角色**，
  表内没有任何既有根承载它（`TcsStateParam` 是参数表读取角色，不是 Def 身份；先例 = 触发定义身份的专有根 `EffectTriggerDef`）。**`Def.StatusTag`（状态词）本轮 MUST NOT 开根**：它今天的解析消费者为零（关系表检查器归 R5.5-e）⇒ 归属待那一轮拍板，验收资产里复用同一个 `StateDef.*` 词。
- **不做（非目标）**：关系表检查器（字段语义未定稿 ⇒ R5.5-e）、`PrimaryAssetTypesToScan`（R7 `INTEG-3`）、Def 双轨同步器（R8 `TOOLS-3`）、槽位竞争、实例与门面（Task 2 起）、`Descriptions` 渲染（R8）。

## Impact

- Affected specs：
  - `plugin-descriptor`（**RENAMED + MODIFIED**：模块清单 7 → 8；需求标题去掉轮次标签）
  - `integration-entity`（**MODIFIED**：定义库第三条发现路径 + 状态定义的发现期校验与"不装配"口径）
  - `state-def-asset`（**ADDED**：状态 Def 数据形状 / 描述配置载体 / 资产身份 / 编辑期表行 / 作者期校验，五条需求）
  - `gameplay-tag-governance`（**MODIFIED**：根段注册表 9 → 10，新增 `StateDef` 根并写明状态词本轮不开根）
- Affected code：`TireflyCombatSystem.uplugin`；`Source/TcsState/**`（新模块，含 `Public/Def/TcsDescriptionEntry.h` 描述配置载体）；`TcsIntegration` 的定义库（第三条发现路径 + 第三份缓存 + `Build.cs` 加 `TcsState` 依赖）；
  宿主 LAC 侧：`Config/DefaultGameplayTags.ini`（新增检查词 `StateDef.Check.Burn`）+ `TcsDevTags` 的解析访问器 + `TcsDevSliceRig` 的发现/校验断言
- Affected docs：`openspec/project.md`（模块计数 7 / 11 → 8 / 11 + Def 家族表 `UTcsBuffDefAsset`）；`SPEC-02-states` §2 资产名与 §1 依赖行的规格提案落点销账 + §10 v2 增补 9 的"描述载体类型住 TcsNotation"父注**订正为 TcsState**（与 `DEC-02-fold-display` v3 命名批的"住哪"表冲突，以决策文档为准）
- 验收：双配置编译（Development Editor + Game Shipping，零 warning）+ 装置侧断言"发现 N 个 `UTcsBuffDefAsset`、`IsDataValid == Valid`、按 `DefTag` 可解析"（`PLN-R5` Task 1 Step 7）
