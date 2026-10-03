## 1. 提案与批准

- [x] 1.1 起草提案（`proposal.md` / `design.md` / `tasks.md` + 三份规格 delta）
- [x] 1.2 `openspec validate add-effect-trigger-def-asset --strict --no-interactive` 通过
- [x] 1.3 用户批准（**批准前不动代码**；两处开放问题见 `design.md`）

## 2. 实施：资产类（`TcsIntegration`）

- [x] 2.1 新建 `Source/TcsIntegration/Public/Trigger/TcsEffectTriggerDefAsset.h`——`UTcsEffectTriggerDefAsset`（`PrimaryAssetType` / `TriggerTag` / `Def` / `GetPrimaryAssetId` / `IsDataValid` 声明）
- [x] 2.2 新建 `Source/TcsIntegration/Private/Trigger/TcsEffectTriggerDefAsset.cpp`——类型常量（值 = 类名 `"TcsEffectTriggerDefAsset"`）/ 身份覆写 / 校验（含 `NotValidated` → `Valid` 提升）
- [x] 2.3 风格核对（`unreal-cpp-style`）：Tab / UTF-8 无 BOM / LF / `.h` region 与访问域 / `.cpp` 扁平 / 文件头版权行 / 单文件 ≤ 300 行
- [x] 2.4 `openspec/project.md` 的「Def 资产命名标准」补限定语（`<Family>Def` 已被数据类型占用时资产类加 `Asset` 后缀——UHT 引擎名判重）

## 3. 实施：定义库（`UTcsDefinitionSubsystem`）

- [x] 3.1 `DiscoverTriggerDefs()`：按类扫描 `UTcsEffectTriggerDefAsset` → 四条校验（加载失败 / 空 `TriggerTag` / 空 `Def.EventTag` / 空 `Def.EffectChainId`）→ 去重（`TriggerTag`）→ 入缓存；失败项进 `FailureList`
- [x] 3.2 `ResolveTriggerDef(FGameplayTag TriggerTag)`：返回缓存内容裸指针，未命中 nullptr（不 ensure）
- [x] 3.3 `SeedWorld` 扩展：链装配**之后**逐条 `RegisterTriggerRow`（`Source` = 定义库来源句柄）+ **引用链预检**（`IsChainRegistered` 不过 ⇒ Warning + 仍登记）+ 装配计数进日志；早退条件改为"两份缓存皆空"
- [x] 3.4 `Initialize` 发放来源句柄（`FTcsSourceHandleRegistry::Allocate` 一次）；就绪日志补触发定义条数
- [x] 3.5 `AddReferencedObjects` 覆写：逐条走 `ChainDefs` + `TriggerDefs`（`AddPropertyReferencesWithStructARO`）+ 资产 `UPROPERTY` 锚定不变
- [x] 3.6 `Deinitialize` 补清理（触发缓存 / 资产锚定数组 / 来源句柄复位）
- [x] 3.7 `.cpp` 行数守 300（现 176 行；超限按 `TcsDefinitionSubsystem_<Feature>.cpp` 拆）

## 4. 规格与治理

- [x] 4.1 实施后核对：根段注册表 9 根、`EffectTriggerDef` 与 `EffectTriggerGate` 互不为前缀、`Check` 子段约定与仓库现实一致
- [x] 4.2 **✅ 已同步（2026-10-04）**：`gameplay-tag-governance` 的 Purpose 改为"9 个根段"并补"根名互不为真前缀 / 检查词 `Check` 子段"两项；`effect-trigger-asset` 的 Purpose 由归档器占位 `TBD` **手工改写**为实义（归档后原： `gameplay-tag-governance` 的 `## Purpose`（"8 个根段" → "9 个根段"——归档器不动既有能力的 Purpose）
- [x] 4.3 （跨仓）LAC 侧已同批落地：提案 `add-effect-trigger-def-tag-root`（`host-gameplay-tag-registry` MODIFIED × 2）+ ini 词 `EffectTriggerDef.Check.Seed` + 根清单 14 根校验 0 违规

## 5. 编译与静态检查

- [x] 5.1 `LegendAutoChessEditor Win64 Development` 编译通过（0 error / 0 warning）
- [x] 5.2 `LegendAutoChess Win64 Shipping` 编译通过（含 `WITH_EDITOR` 的 `IsDataValid` 面，必跑）
- [x] 5.3 依赖面零新增边：`git diff --stat` 证明未动任何 `Build.cs` / `.uplugin`
- [x] 5.4 `TcsEffect` 零反向依赖：`Source/TcsEffect/**` 内不得出现 `TcsIntegration` 头（grep 取证）
- [x] 5.5 `git status` 证明未动 `TcsEffect` 既有源文件（本批只新增 `TcsIntegration` 侧文件）

## 6. 定向人工检查（PIE，用户执行）

- [x] 6.1 **正路**：检查资产 `DA_Check_TriggerRule`（`TriggerTag = EffectTriggerDef.Check.Seed` / `Def.EventTag = TcsEvent.Attribute.ValueChanged` / `Def.EffectChainId = EffectChain.Check.WaitEvent`；LAC 侧提案 `add-effect-trigger-def-tag-root` 建）→ 日志报"触发定义 1 条"且世界装配行报触发行数 +1 → 发布该事件 → 目标链执行（日志锚点：链起 / 链完成 / 步骤副作用）
- [x] 6.2 **失败面**：把该资产的 `TriggerTag`（或 `Def.EventTag`）临时置空 → 失败清单含该资产路径与原因、该行**未**装配（行数与基线一致）、**无 ensure**；检查完毕后删除/改建该临时资产 —— **✅ Run 2 实证**：`触发定义 0 条，失败 1 条` + `触发行 0/0 条` + 恰 1 条 Error、零 ensure；连带实证"被拒资产不误伤其余"（仅检查 0 翻红，8 项仍绿）
- [x] 6.3 **幂等**：同一 PIE 会话内装配只发生一次（装配日志只一条、行数不翻倍）
- [x] 6.4 **引用链预检**：临时把 `Def.EffectChainId` 指向未登记链 → 留一条 Warning（含 `TriggerTag` 与链 id）且该行**仍**装配（行数 +1）、无 Error —— **✅ Run 3 实证**：恰 1 条 Warning（文本含两者）+ `触发定义 1 条，失败 0 条` + `触发行 1/1 条`（与 Run 2 的 `0/0` 形成关键对照）
- [x] 6.5 常规检查命令零红字（6.2 的故意失败面**不混进**常规命令——独立资产、独立轮次）

## 7. 收口

- [x] 7.1 证据文档（`Documents/combat-system-design/evidence/`）——命令、逐项日志锚点、夹具与失败面处置
- [x] 7.2 台账登记（新增条目：引用链预检的误报面 / 内联位归属 / LAC 侧根登记义务 / `Check` 子段约定）
- [x] 7.3 计划回写：`PLN-R4` Task 2.5 的 Step 1–4 勾选 + 实施注记（含"与计划的偏离"三条：类名 / 代行登记 / ARO 判据纠正）
- [x] 7.4 **✅ 已归档为 `2026-10-04-add-effect-trigger-def-asset`**（`validate --all --strict` = **26/26**，无活动变更）：`openspec archive add-effect-trigger-def-asset --yes` + `openspec validate --all --strict` 全绿
