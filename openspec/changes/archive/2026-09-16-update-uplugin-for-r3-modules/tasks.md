## 1. Implementation——Step 2 插件描述文件

- [x] 1.1 在 `TireflyCombatSystem.uplugin` 中将 `EngineVersion` 设为 `"5.8"`
- [x] 1.2 将 `Modules` 数组整体替换为 TcsCore / TcsNotation / TcsAttribute（Type=Runtime、LoadingPhase=Default、依赖序）
- [x] 1.3 移除 `Plugins` 数组
- [x] 1.4 人工检查：JSON 可解析、字段与 proposal 口径逐项核对（禁 TDD 纪律下不做自动化测试）
- [x] 1.5 用户检查通过（2026-09-10）

## 2. Implementation——Step 3 三模块骨架（plan1 Task 0 Step 3，用户拍板并入本提案）

- [x] 2.1 TcsCore：Build.cs（Core/CoreUObject/Engine/DeveloperSettings/GameplayTags）+ 根壳 + LogChannel（LogTcsCore）+ UTcsDeveloperSettings + `Parameter/` 载体五文件（PV 系列终态，见第 5 节）
- [x] 2.2 TcsNotation：Build.cs（仅 Core/CoreUObject）+ 根壳 + LogChannel（LogTcsNotation）+ `FTcsValueConvention.h`
- [x] 2.3 TcsAttribute：Build.cs（引擎基础 + TcsCore + TcsNotation）+ 根壳 + LogChannel（LogTcsAttribute）
- [x] 2.4 人工检查：目录收窄口径 / 依赖方向 / 命名 / UTF-8 无 BOM + LF 逐项核对

## 3. Implementation——Step 4 冒烟编译（plan1 Task 0 Step 4）

- [x] 3.1 按 unreal-cpp-compile 技能刷新 LAC 工程项目文件（-projectfiles）
- [x] 3.2 LAC 工程 Development Editor 编译通过（Result: Succeeded，2026-09-10）

## 4. Verification

- [x] 4.1 Step 2 范围产物停点检查已通过；Step 3/4 完成后停点待用户检查；git commit 需用户明确授权

## 5. 补充——PV 载体体系改造（2026-09-11 用户补充提示词）

- [x] 5.1 删除 `Parameter/FTcsParamScalar.h`（零消费者）；新建五文件：`FTcsParamValue` / `FTcsParamValueSource`（Evaluate 用默认体 + `meta=(Hidden)`——UHT TCppStructOps 禁纯虚，规格偏差已报备待追认）/ `FTcsParamEvaluateContext`（反射可见，禁 TFunction） / `ITcsParamTableReader` / `FTcsParamSource_Literal` / `FTcsParamSource_ParamRef`
- [x] 5.2 目录更名 `Vocabulary/` → `Parameter/`（用户拍板：领域命名消除二义性），include 路径与文档全量同步
- [x] 5.3 冒烟编译通过（Result: Succeeded）；plan1 Task 0 末尾注记已加
