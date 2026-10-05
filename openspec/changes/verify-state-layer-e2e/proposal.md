# Change: verify-state-layer-e2e — R5 状态层端到端竖切验证

## Why

R5 的六个能力（`state-def-asset` / `state-instance-lifecycle` / `state-param-snapshot` /
`state-modifier-materialization` / `state-stacking-policies` / `state-behavior-fragments`）各自都有独立证据，
但**没有任何一条证据证明它们能在同一个实例上串起来跑**。六个环节的耦合点——`CascadeAnchor` 的锚定、
`ParamSnapshot` 的绑定作用域、内联触发行登记与撤销的配对、计时条目的取消、到期回收后属性是否复原——
目前只在各自单元里被分别验过，从未被同一次运行穿过。

同时 Task 4 遗留的**唯一未闭合边界**至今没有证据：`FTcsStateModifierMaterializer` 的真资产路径
（`TcsStateModifierMaterializer.cpp:50-54` 的 `Row.Get()` → `Row.LoadSynchronous()` 兜底）。
全库 `UTcsAttrModDef` 资产数为 **0**（已扫 `Content/` 与 `Plugins/`，零命中），Task 4 用的是 C++ 里
手搓的**瞬态**模板（`TcsDevSliceRig.cpp:1432-1434` 注释原文："两条模板都建为**瞬态**模板资产
（不为验收面新增内容资产）"）⇒ "内容驱动 `ModifierRows`"这条路径**从未被执行过**。

**为何本变更不只是验证**：要验那条路径，就必须落一个**真的** `UTcsAttrModDef` 资产；而
`UTcsAttrModDef::IsDataValid` 把空 `TemplateTag` 判为 **Error**（`TcsAttrModDef.cpp:28-32`），
`GetPrimaryAssetId()` 也拿它当主身份名 ⇒ 真资产必须有有效的 `TemplateTag` ⇒ 必须有承载它的根。
该根此前故意不开（台账 `ATTR-1`："零解析消费者 ⇒ 不预建根，归属 = 触发条件"），
故本变更**先补上那条解析路径**（`DiscoverAttrModDefs` / `ResolveAttrModDef`），
触发条件成立后再登记 `AttrModDef` 根。**这是 ATTR-1 的按期闭合，不是规则的例外。**

本变更交付一条可复跑的竖切 + 一份如实标注边界的证据文档。

## What Changes

- 新增宿主验收装置命令 `Tcs.Test.State.Run`（常规面，要求**零非预期红字**）与 `Tcs.Test.State.Reject`
  （拒绝面：全部预期红字集中于此），落 `LegendAutoChess/Source/TcsDev/`。
- 新增 **4 个内容资产**（首次为状态面验收引入内容资产）：
  1 个 `UTcsAttrModDef` 修正器模板（**全库首个**）+ 1 个 `UTcsBuffDefAsset` 竖切 buff
  + 2 个 `UTcsEffectChainDef`（施加速链 / 行为链）。
- **不改动既有 `DA_Check_BuffDef`**：新增 `Period` 会让它的周期回调污染既有检查 20l 的采样窗口，
  使既有验收转红（实测理由，非洁癖）。
- 新增 7 个宿主声明 tag（`StateDef.Check.*` / `EffectChain.Check.*` / `TcsStateParam.Check.*` /
  `AttrModDef.Check.StateLayer`）。**词形纪律**：全部取"功能根 + `Check` 段"（3 段）——检查词的消费路径与正式内容词
  完全相同（链 id 仍须 `RegisterChain` 认、定义身份仍须定义库认），故 MUST NOT 另立根；
  MUST NOT 把生命周期词放进路径中段（原稿 `*.E2E.*` 形态命中此禁，宿主契约 `host-gameplay-tag-registry`
  把同形路径当反例点名）。仓内先例 = `StateDef.Check.Burn` / `EffectChain.Check.{SelfSub,Sort,…}`。
- 新增证据文档 `Documents/combat-system-design/evidence/2026-10-05-state-layer-pie.md`
  （`EVID-2026-10-05-state-layer`）：装置输出冻结快照的 SHA-256、关键日志行引用、双面读数、边界清单。
- **插件侧新增一条定义库发现路径 + 新立一个根（本变更性质由"纯验证"变为"验证 + 实现"，见下）**：
  `DiscoverAttrModDefs` / `ResolveAttrModDef` + `AttrModDef` 根（插件侧 10 → 11 根）。
  **为什么折进本变更**：`ATTR-1` 记"该身份词零解析消费者 ⇒ 不为它开根，归属 = 触发条件：出现'按 tag 解析修正器模板'
  的真实需求时"；而验证"真资产 `LoadSynchronous()` 路径"必须先有**合 `IsDataValid == Valid` 的真资产**，
  该资产的 `TemplateTag` 非空即需要有根 ⇒ **本变更制造了那个解析方，触发条件成立**。
  这不是"预建根"，是条件成立后的按期登记（同 `StateDef` 根 9→10 的先例）。

## Impact

- Affected specs: 新增 `state-layer-e2e-validation`（4 条需求）；**修改两份既有能力**
  （`integration-entity`：发现路径 3 → 4 条 + 新解析口 + GC/去重边界；`gameplay-tag-governance`：
  根表 10 → 11 + 根名判据 + 深度上限度量）；**新增一份**（`attribute-types`：模板身份词归属，ADDED 需求）。
- Affected code（TCS 仓新增/改动，LAC 仓为装置与内容）：
  - `TireflyCombatSystem/Source/TcsIntegration/Private/TcsDefinitionSubsystem_AttrModDef.cpp`（新增，照 `_State.cpp` 形状）
  - `TireflyCombatSystem/Source/TcsIntegration/Public/TcsDefinitionSubsystem.h`（加 `ResolveAttrModDef` + 声明 + 两份成员）
  - `TireflyCombatSystem/Source/TcsIntegration/Private/TcsDefinitionSubsystem.cpp`（`Initialize` 调发现 + 就绪日志 + `Deinitialize` 清理）
  - `LegendAutoChess/Source/TcsDev/Private/Dev/TcsDevSliceRig_State.cpp`（新增）
  - `LegendAutoChess/Source/TcsDev/Public/Dev/TcsDevSliceRig_State.h`（新增）
  - `LegendAutoChess/Source/TcsDev/Private/Dev/TcsDevSliceRig_Internal.h`（新增：4 个共享件提升）
  - `LegendAutoChess/Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`（改为消费内部头；`RegisterCommands` 增两条命令）
  - `LegendAutoChess/Content/TcsDev/E2E/`（4 个新资产，含 `.uasset`）
  - `LegendAutoChess/Config/DefaultGameplayTags.ini`（+7 行）
- Affected docs: `Documents/combat-system-design/`（证据文档、`INDEX.md`、`ledger/deferred-inputs-ledger.md`、
  `log/implementation-log.md`）。
- 台账预期：本变更**闭合 `ATTR-1`**（触发条件达成——"按 tag 解析修正器模板"的路径已落地；残留边界 = 运行期调用者为零，
  如实记入 `ATTR-1` 的收束语），并闭合 **Task 4 边界①**（真资产 `LoadSynchronous` 路径）由本变更的证据行。其余既有行
  （`STAT-2` / `DAMAGE-5` / `WAIT-6` / `TRIG-4` / `TRIG-5` / `DAMAGE-2`）仍归 Task 8；新增行（若有）在同轮登记。
