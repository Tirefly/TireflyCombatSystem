## 1. 提案与上下文同步

- [x] 1.1 `openspec validate add-tcs-state-module --strict --no-interactive` 通过（deltas 四份：`plugin-descriptor` RENAMED + MODIFIED / `integration-entity` MODIFIED / `state-def-asset` ADDED / `gameplay-tag-governance` MODIFIED）—— **✅ 2026-10-04**（全量 = 27 passed / 0 failed）
- [x] 1.2 `openspec/project.md` 同步三处：模块计数 7 / 11 → 8 / 11；"R3 实施阶段 · 六个模块"的阶段口径；Def 家族表的 `UTcsBuffDef` → `UTcsBuffDefAsset`
- [x] 1.3 `plugin-descriptor` 主规格的 `## Purpose` 就地订正（"R3 物化的编译模块集合" → 现况声明；CLI 明示 delta 的 Purpose 对既有能力被忽略，只能直改主规格）—— **✅ 2026-10-04**（改为"**当前物化**的编译模块集合（现况声明，随轮次推进——R3 七模块 → R5 八模块）"）
- [x] 1.4 `gameplay-tag-governance` 主规格 `## Purpose` 的根数计数同步（"钉死插件那 9 个根段" → 10；与 1.3 同批做，避免"表 10 而门面 9"的计数不一致窗口）—— **✅ 2026-10-04**（订正后复核：根表数据行 = 10 = 门面计数）

## 2. 模块骨架（PLN-R5 Task 1 Step 1）

- [x] 2.1 `TireflyCombatSystem.uplugin` 的 `Modules` 插入 `TcsState`（`TcsEffect` 之后、`TcsTargeting` 之前，`LoadingPhase = Default`）
- [x] 2.2 `Source/TcsState/TcsState.Build.cs`（`PublicDependencyModuleNames` = `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect`）
- [x] 2.3 模块壳 `TcsStateModule.h` / `TcsStateModule.cpp`（`FTcsStateModule : public IModuleInterface`，空体、零注册）
- [x] 2.4 日志通道 `Public/TcsStateLogChannel.h` + `Private/TcsStateLogChannel.cpp`（`LogTcsState`）
- [x] 2.5 `Public/` / `Private/` 目录按 `cpp-module-structure` 能力的分层口径建立（域子目录 PascalCase）

## 3. Def 数据形状（Step 2 / Step 4）

- [x] 3.1 `Public/Def/TcsParamRow.h`：`ETcsParamMode{ EPM_Snapshot = 0, EPM_Live = 1 }` + `FTcsNumericParamRow{ Key, Base, Mode, ValueConvention }`
- [x] 3.2 `Public/Def/TcsStateDefBase.h`：`FTcsStateDefBase`（`USTRUCT(meta = (Hidden))`，中性默认实现）六字段
- [x] 3.3 `Public/Def/TcsBuffDef.h`：`FTcsBuffDef : FTcsStateDefBase`（时值四件 / 堆叠 / 内联触发行 / 关系四字段）
- [x] 3.4 `Public/Def/TcsBuffDefTableRow.h`：`FTcsBuffDefTableRow : FTableRowBase`（`DefTag` + `BuffDef`）；运行期零 DataTable 加载路径
- [x] 3.5 `Public/Def/TcsDescriptionEntry.h`（**2026-10-04 用户裁定甲案：随本 Task 落最小载体**）：`FTcsDescriptionEntry`（`DescriptionId` / `TextKey` / `Views`）+ `FTcsDescriptionViewSlot`（`SlotName` / `View: FInstancedStruct`，**本轮不带 `meta = (BaseStruct=…)`**）——两个纯数据载体，住 **TcsState**（依据 `DEC-02-fold-display` v3 命名批"描述视图槽位 / 配置条目 → TcsState"）；视图策略族（`FTcsParamView` + 四内置 + `FTcsViewBuildContext` / `FTcsViewProbe` + 组装器）MUST NOT 在本轮出现
- [x] 3.6 **枚举落点**：`EDurationPolicy` / `ETcsPeriodRefresh` 由本 Task 首次需要 ⇒ 住 `Public/State/TcsStateEnums.h`（`FStateStackPolicy` 同批给形状）；`PLN-R5` Task 2 交付物列表里的同名两项随之让位（Task 2 在同一文件补 `EStatePhase` / `EStateRemoveCause` / `EApplyResult`）——实施时回写计划，免两处定义

## 4. 资产与作者期校验（Step 3）

- [x] 4.1 `Public/Def/TcsStateDef.h`：`UTcsStateDef : UPrimaryDataAsset`（`DefTag` + 显式 `static const FPrimaryAssetType PrimaryAssetType` + `GetPrimaryAssetId()` + `IsDataValid` 挂点）
- [x] 4.2 `Public/Def/TcsBuffDefAsset.h`：`UTcsBuffDefAsset : UTcsStateDef`（`BuffDef: FTcsBuffDef`；`PrimaryAssetType` 取值 `"TcsBuffDefAsset"`）——**须一并覆写 `GetPrimaryAssetId()`**（基类的静态成员是编译期按基类作用域绑定的）
- [x] 4.3 `IsDataValid` 六条 buff 错误判据 + 描述配置三条错误判据（判据声明处 = `state-def-asset` 的「描述配置载体」需求）+ 两条警告判据落地，且末尾 MUST 把 `NotValidated` 提升为 `Valid`；错误挂到出问题的配置元素

## 5. 定义库发现与解析（Step 5）

- [x] 5.1 `TcsIntegration.Build.cs` 加 `TcsState` 依赖
- [x] 5.2 `UTcsDefinitionSubsystem::DiscoverStateDefs()`（照 `DiscoverTriggerDefs` 形状：`IAssetRegistry::GetAssetsByClass(UTcsBuffDefAsset::StaticClass()->GetClassPathName())` + `IsLoadingAssets()` 就绪门 + 四条身份级校验进 `FailureList`）
- [x] 5.3 `UTcsDefinitionSubsystem::ResolveStateDef(FGameplayTag DefTag) -> const FTcsBuffDef*`（未命中返回 nullptr，不 ensure）
- [x] 5.4 第三份缓存（`StateDefs`）+ `AddReferencedObjects` 逐条补引用（判据 = 容器是否 GC 可见，与值/指针语义无关）
- [x] 5.5 **不做**世界装配：`OnPostWorldInitialization` 不为状态定义登记任何每世界结构
- [x] 5.6 空 TcsState 模块进模块列表后全链路编译自检（`.uplugin` 顺序、依赖方向、无反向 include）

## 6. 文档回写（Step 6）

- [x] 6.1 `SPEC-02-states` §2 资产名 `UTcsBuffDef` → `UTcsBuffDefAsset` 的提案落点销账 + §1 依赖行与 §6 编译依赖同步（复核结论：§1 / §6 的依赖行**本就与落地一致**，无需改动；§2 的资产名注记已在收窄轮落纸）
- [x] 6.2 `SPEC-02-states` §10 v2 增补 9 的父注订正：原写"描述载体类型住 TcsNotation"，与 `DEC-02-fold-display` v3 命名批的"住哪"表（`FTcsDescriptionViewSlot` / `FTcsDescriptionEntry` → TcsState）冲突 ⇒ 按决策文档改判并把冲突留痕（§6.4 禁止隐性多真相）
- [x] 6.3 `PLN-R5` Task 1 收束：Step 1~6 全勾 + Step 7 编译半边勾 + 偏离注记（本 Task 新增规格面：`integration-entity` MODIFIED / `state-def-asset` ADDED / `gameplay-tag-governance` MODIFIED；描述载体缺口 → 随本 Task 落最小集；两处类型提前落；`StateDef` tag 根 9 → 10）
- [x] 6.4 台账与两册日志同步（新增 `STAT-4` = 49 → 50；两册各一条；`INDEX` 八处同步）

## 7. 验收（Step 7）

- [x] 7.1 双配置编译（Development Editor + Game Shipping）零 warning 零 error —— **✅ 2026-10-04 已跑**：Dev 23 动作 / Ship 16 动作均 `Result: Succeeded`、两日志 `error` 与 `warning` 命中数均 0；产出 `UnrealEditor-TcsState.dll` / `.lib`（含宿主装置改动后的复编）
- [x] 7.2 宿主 tag 声明：LAC `Config/DefaultGameplayTags.ini` 加检查词 `StateDef.Check.Burn`（`<功能根>.Check.<名>` 形态；DevComment 写清消费路径 = 定义库 `DiscoverStateDefs` / `ResolveStateDef`）
- [x] 7.3 `TcsDevTags` 加 `GetStateDefCheckBurnTag()`（缓存解析；首次调用 MUST 晚于 tag 表加载——GameInstance 级或更晚）
- [x] 7.4 装置断言（`TcsDevSliceRig`）：`ResolveStateDef(StateDef.Check.Burn)` 非空 + 解析出的 `StatusTag` 与该词一致 + 定义库失败清单**无**状态定义条目（`WITH_EDITOR` 面另断 `IsDataValid == Valid`）——**代码已落并编译通过，判据待 PIE 实测**
- [x] 7.5 编辑器内建一个 `UTcsBuffDefAsset`（`DefTag` / `StatusTag` 用同一 `StateDef.Check.Burn` 词）—— **✅ 2026-10-04**：`/Game/TcsDev/Checks/DA_Check_BuffDef`（存盘实证 = 日志 `Cmd: OBJ SAVEPACKAGE`，L2430；文件 SHA-256 `6c6c945d…`，`IsDataValid == Valid`）
- [x] 7.6 PIE 跑 `Tcs.Test.Slice.Run`：**✅ 2026-10-04**——即时 **通过 20 / 失败 0**（原 19 项 + 新增**检查 18**）、延迟判定 **7b PASS**、区间零红字；证据 = `EVID-2026-10-04-tcs-state-def-asset`（区段 L2909–L3168，区段 SHA-256 `0dd2bb9a…`）
- [x] 7.7 归档：`openspec archive add-tcs-state-module --yes` + `openspec validate --all --strict --no-interactive` 全绿 + 主规格核对（8 模块清单 / 三条发现路径 / 根表 10 / 新能力 `Purpose` 非占位）—— **✅ 2026-10-04**：归档输出 `+5 added / ~3 modified / →1 renamed`；全量 **27 passed / 0 failed**、`changes/` 零活动提案；主规格逐项复核通过（`模块物化声明` 八模块、根表 10 行、三条发现路径 + `ResolveStateDef`、`state-def-asset` 5 需求 / 13 场景、`Purpose` 非占位）
