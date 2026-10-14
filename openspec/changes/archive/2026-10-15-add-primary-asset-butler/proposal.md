# Change: 新增资产加载层（PrimaryAsset 注册与加载）

## Why

各域（属性 / 效果链 / 状态 / Buff / 技能）的 Def 资产此前**没有任何运行期的类型注册与加载机制**——实测：`PrimaryAssetTypesToScan` 全库零命中、`UAssetManager` 子类 / `StartInitialLoading` / `AssetManagerClassName` 全库零命中、`CoreRedirects` 未配置。定义发现只能走 `IAssetRegistry::GetAssetsByClass`，而**该路径不提供"按 `FPrimaryAssetId` 解析"**（未注册类型时 `UAssetManager` 按类型查询会**静默返回空**，排障成本高）。

同时存在一条**架构死结**：各域要能向加载层注册自己的 Def 资产类型，加载层就必须**住得比所有注册者都低**；而若它认识任一业务 Def 类型，它就必须住到那些类型**之上** ⇒ 各域（含层级最低的 `TcsAttribute`）无法再向下注册（**成环**——本仓实测过同型环）。原提过的"`TcsDef` 新模块承载共用 Def 族"与"`TcsDef` 并入 `TcsIntegration`"两条中间方案**均因该环作废**。

**依据** = `DEC-2026-10-13-asset-butler`（裁定①~⑧）+ `PLN-REFACTOR-1` Task 8（本变更的执行形状）。

## What Changes

- **新建模块 `TcsAssetButler`**：只承载 **PrimaryAsset 机制**（类型注册表 / 运行期加载 / 防重 / 编辑器期规则），**零 Tcs 业务依赖**（只依赖引擎 `Core` / `CoreUObject` / `Engine` / `DeveloperSettings` / `GameplayTags` / `AssetRegistry`）。位置 = 与 `TcsCore` / `TcsNotation` 平级的最低层。
- **注册键 MUST 类型擦除**：`FTcsPrimaryAssetEntry` 只收 `FPrimaryAssetType` + `UClass*` + 扫描路径 + 策略 + `bHasBlueprintClasses` / `bIsEditorOnly`——**MUST NOT** 认识任何业务 Def 类型。**本仓已有同型先例**（`FTcsEffectStepExecutorRegistry::Register(const UScriptStruct*, ...)` 的宿主完全不认识领域步骤类型）。
- **类型清单走代码注册**：`UE_DEFINE_PRIMARY_ASSET_TYPE(DefClass, ScanPaths, Policy)` 在各域 `.cpp` 内自注册；类型名由 `StaticClass()` **反射派生**（**零字符串面**）。**MUST NOT 用 ini 提供类型清单**——否则改名会漏改 ini 而**静默失联**。
- **`UTcsAssetManager : UAssetManager`**：覆写 `StartInitialLoading()`，`Super::` 之后遍历注册表补调 `ScanPathsForPrimaryAssets(...)`（**绕开 ini**）。
- **运行期加载三件**：`ETcsAssetLoadPolicy{PreloadAll, PreloadSelected, OnDemand}` + `FTcsAssetLoader`（异步默认 / 同步逃生口 / 同 `FPrimaryAssetId` 防重 + 句柄计数）。
- **编辑器期规则**：`UTcsAssetButlerSettings : UDeveloperSettings`（**红线：值 MUST NOT 影响模拟语义**）。
- **四处搬迁（裁定⑤）**：`UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` 自 `TcsIntegration` **搬入 `TcsEffect`**（链/触发机制的家），**同批配 `CoreRedirects`**；`UTcsCombatEntityComponent` / `UTcsPieEntityQuery` **留 `TcsIntegration`** 并就地声明归属（该"实体接线层"就是 `TcsIntegration` 自身，**不是新建模块**）。
- **`TcsIntegration` 被掏空后**：仍是定义**缓存层**（认识业务类型，住上层），与低层加载注册层**分层**（裁定④）。

## Impact

- Affected specs: `primary-asset-loading`（**新增能力**）、`effect-chain-asset` / `effect-trigger-asset`（**MODIFIED**：宿主模块与头路径变更 + 主资产注册）、`plugin-descriptor`（**MODIFIED**：模块清单 10 → 11）、`cpp-module-structure`（**MODIFIED**：依赖方向表新增加载层）
- Affected code:
  - 新建 `Source/TcsAssetButler/**`（模块壳 / 日志通道 / 注册项 / 注册表 / 自注册宏 / `UTcsAssetManager` / 加载策略 / 加载器 / 设置）
  - `Source/TcsEffect/**`（收下两个 Def 资产类 + `Build.cs` 补 `TcsAssetButler`）
  - `Source/TcsIntegration/**`（两 Def 类搬出、两接线类型留原地并声明归属）
  - `TireflyCombatSystem.uplugin`（模块 **10 → 11**）
  - 宿主 `Config/DefaultEngine.ini`（`AssetManagerClassName` + `[CoreRedirects]` 追加两条 `ClassRedirects`）
  - `DEC-00` §9 模块表（**+1 行**，表行 **12 → 13**）、`SPEC-06-integration` §1 依赖行重写（去 StateTree 声称，裁定⑦）
