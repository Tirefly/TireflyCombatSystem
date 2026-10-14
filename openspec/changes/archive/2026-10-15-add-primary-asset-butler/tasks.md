## 1. 开工前置与实测

- [x] 1.1 §0.5.2 四动作（A 按 Task 8 改动面重枚举体例债 / B 日期现场值 / C 护栏表 / D 超限三段式）
- [x] 1.2 T1 实测：覆写 `StartInitialLoading()` 后补扫的时序后果（结论：不错过 bulk scan、不延误 `OnCompletedInitialScan`）
- [x] 1.3 T3 实测：`AssetManagerClassName` 解析与 `LoadingPhase` 取值（结论：`Default` 已足够且已实证）

## 2. 加载层模块

- [x] 2.1 模块壳 `TcsAssetButlerModule.h/.cpp` + `TcsAssetButler.Build.cs`（零 Tcs 业务依赖）
- [x] 2.2 日志通道 `TcsAssetButlerLogChannel.h` + `Private/.cpp`（`LogTcsAssetButler`）
- [x] 2.3 `FTcsPrimaryAssetEntry`（类型擦除）+ `FTcsPrimaryAssetRegistry`（同类型重复报错，对齐引擎语义）
- [x] 2.4 自注册宏 `UE_DEFINE_PRIMARY_ASSET_TYPE` + `FTcsPrimaryAssetRegistrar`
- [x] 2.5 `UTcsAssetManager` 覆写 `StartInitialLoading()`（遍历注册表补扫）
- [x] 2.6 `ETcsAssetLoadPolicy` + `FTcsAssetLoadHandle` + `FTcsAssetLoader`（异步 / 同步 / 防重）
- [x] 2.7 `UTcsAssetButlerSettings`（编辑器期规则，零模拟语义）

## 3. 四处搬迁

- [x] 3.1 两个 Def 类搬入 `TcsEffect`（改导出宏 `TCSINTEGRATION_API` → `TCSEFFECT_API`）
- [x] 3.2 两个接线类型留 `TcsIntegration` 并就地声明归属
- [x] 3.3 宿主 `[CoreRedirects]` 追加两条 `ClassRedirects`（同段追加，不另起段）
- [x] 3.4 逐资产验证 14/14 可打开且类路径已解析

## 4. 文档与计数

- [x] 4.1 `plugin-descriptor` 模块清单 10 → 11
- [x] 4.2 `openspec/project.md` 模块清单 10 → 11
- [x] 4.3 `DEC-00` §9 表新增 `TcsAssetButler` 行 + 表行 12 → 13 + 依赖链
- [x] 4.4 `SPEC-06-integration` §1 依赖行重写（裁定⑦）

## 5. 验证

- [x] 5.1 双配置编译（Dev Editor + Game Shipping，0 error / 0 warning）
- [x] 5.2 `Public/` 头零业务 Def include（`grep` 复核）
- [ ] 5.3 宿主装置读数（判据 = 既有读数逐字不变）——**人工验证点**（PIE 内敲命令）
