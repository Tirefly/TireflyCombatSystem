# Change: 触发行独立资产载体（`UTcsEffectTriggerDefAsset` + 定义库发现与装配）

## Why

R4 Task 1/2 已把"事件 → 触发行 → 效果链"的**机制层**打通（`FTcsEffectTriggerDef` 纯配置 + 登记表 + 共享求值器 + 四道门），但触发行**今天只能 C++ 注册**：全库 `RegisterTriggerRow` 在 `TcsEffect/` 之外使用点为零，纯内容侧配不出一条触发规则。而 `effect-trigger` 规格已把"**定义加载期登记**（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄"写成事实——**该分支今天没有实现，规格与代码不符**。

本变更补上载体与装配路径：策划用一个 Def 资产声明"系统级规则"（如"任何单位死亡时触发某链"），定义库在发现期校验并缓存它，在世界装配期把它登记成该世界的触发行。

## What Changes

- **新增资产类 `UTcsEffectTriggerDefAsset : UPrimaryDataAsset`**（住 `TcsIntegration/Public/Trigger/`）：`TriggerTag`（内容身份）+ `FTcsEffectTriggerDef Def`（定义数据）+ 显式 `PrimaryAssetType`（值 = 类名 `"TcsEffectTriggerDefAsset"`）+ 覆写 `GetPrimaryAssetId()` + `IsDataValid`；
  - **类名带 `Asset` 后缀 = UHT 硬约束下的明示例外**：Def 资产命名标准是 `<Family>Def`，但该名字已被 Task 1 交付的数据 struct `FTcsEffectTriggerDef` 占用，而 UHT 按"**去前缀后的引擎名**"判重，实测直接失败：`Error: Class 'UTcsEffectTriggerDef' shares engine name 'TcsEffectTriggerDef' with struct 'FTcsEffectTriggerDef'`（UHT 阶段，2026-10-04）⇒ 标准条文补限定语（`openspec/project.md`），本类取 `Asset` 后缀消歧；
- **`UTcsDefinitionSubsystem` 加第二条发现路径**：`DiscoverTriggerDefs()` / `ResolveTriggerDef(TriggerTag)`；**世界装配期**把每条定义登记为该世界的触发行（`Source` = 定义库来源句柄，`Initialize` 时发放一次）；链装配在前；
- **引用链预检**：`Def.EffectChainId` 在该世界不可解析 ⇒ **Warning + 仍登记**（不跳过登记）；
- **定义库补 GC 引用收集**（`AddReferencedObjects`）：链与触发**两份**缓存都持 `FInstancedStruct`，内层可达宿主自定义 struct 的 `UPROPERTY` 对象引用 ⇒ 不补即静默回收。**链缓存那一半是同批驱动修**（同一个类、同一种缺陷、同一处覆写）；
- **新增根段 `EffectTriggerDef`**（宿主 ini 声明、定义库消费）并登记进插件侧根段注册表；同批把**检查词的 `Check` 子段约定**写进规格（既有 `EffectChain.Check.*` 与本批检查资产都按此形态，属"把已在跑的实践补成规则"）；
- **非目标**：SkillDef/BuffDef 内联位（M3/M5 未落地，届时自然成立）；链资产 `IsDataValid` 缺"提升为 `Valid`"段（归台账 `TOOLS-2` ③，本批只保证新类做对）；宿主自行登记触发行的那条路（既有 API，不在本批改动）。

## Impact

- **Affected specs**：
  - `integration-entity`（**MODIFIED**：定义库——两条发现路径 / 触发定义失败清单 / 装配为触发行 / 引用链预检 / 两份缓存的 GC 补引用）
  - `effect-trigger-asset`（**ADDED 新能力**：触发行独立资产载体）
  - `gameplay-tag-governance`（**MODIFIED**：根段注册表 +1 根、根名判据表 +1 行、深度度量与 `Check` 子段订正）
- **Affected code**：
  - 新建 `Source/TcsIntegration/Public/Trigger/TcsEffectTriggerDefAsset.h` + `Private/Trigger/TcsEffectTriggerDefAsset.cpp`
  - 改 `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` + `Private/TcsDefinitionSubsystem.cpp`（现 176 行）+ 新建 `Private/TcsDefinitionSubsystem_Trigger.cpp`（发现与解析按仓规 `<Name>_<Feature>.cpp` 分片，**全部 ≤ 300 行**）
  - 改 `openspec/project.md`（Def 资产命名标准补限定语）
- **依赖面**：零新增边——`TcsIntegration` 本就依赖 `TcsEffect`；`Build.cs` / `.uplugin` 零改动；`TcsEffect` MUST NOT 反向依赖（资产类住 `TcsIntegration` 正是为此）
- **跨仓义务（本提案不落地，登记在案）**：LAC 侧 `host-gameplay-tag-registry` 的声明位置清单 + `Config/DefaultGameplayTags.ini` 的实际词（`EffectTriggerDef.*`）——建议随 R4 Task 4 的内容词（破甲规则）同批落地，避免空转一次治理改动
- **人工检查**：本批需一条**临时**规则资产跑通正路与失败面（资产经 UE MCP 建、检查后删除或改建为 Task 4 的夹具），PIE 由用户执行
