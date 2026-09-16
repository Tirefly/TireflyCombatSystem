# Change: R3 Task 0 基线——插件描述文件重写与三模块骨架

## Why
R3 计划一 Task 0 是一个原子基线任务：Step 2 重写 `.uplugin` 指向新三模块物化（TcsCore/TcsNotation/TcsAttribute），Step 3 落三模块骨架与日志通道，Step 4 冒烟编译消除中间态。当前描述文件仍引用已随仓库清理而删除的旧模块（TireflyCombatSystem / TireflyCombatSystemEditor），并残留幽灵插件依赖（GameplayMessageRouter 等——R0 §2 第 7 条将其列为从零重建的原因之一）。经用户拍板（2026-09-10），Steps 2–4 并入本提案整体执行，不留不可编译悬空状态。

## What Changes
### Step 2：插件描述文件（已完成并经用户确认）
- `EngineVersion` 设为 `"5.8"`（与宿主工程 `LegendAutoChess.uproject` 引擎关联 GUID → `E:/UnrealEngine/UE_5.8` 一致）。
- **BREAKING**：`Modules` 数组整体替换为 `TcsCore`、`TcsNotation`、`TcsAttribute`——Type 均为 Runtime、LoadingPhase 均为 Default、按依赖序排列（R0 §9）。
- **BREAKING**：移除 `Plugins` 数组（StateTree、GameplayStateTree、GameplayMessageRouter、TireflyObjectPool）——事件总线与对象池内置 TcsCore、不替换 Lyra GameplayMessageRouter（R0 §9 基础设施内置规定）；StateTree/GameplayStateTree 待 plan2 TcsIntegration 落地时回补。
- 其余元数据字段保持不变；Tab 缩进、UTF-8 无 BOM、LF。

### Step 3：三模块骨架（本提案扩展范围，用户拍板并入）
- `Source/TcsCore/`：根壳三文件（Build.cs / Module.h / Module.cpp）+ `Public/TcsCoreLogChannel.h` + `Private/TcsCoreLogChannel.cpp`（`LogTcsCore`）+ `Public/UTcsDeveloperSettings.h/.cpp`（UDeveloperSettings 空壳，Config 分类名 `Tcs`）+ `Public/Parameter/` 载体五文件——2026-09-11 PV 系列补充改造（用户提示词）：D2-12 `FTcsParamScalar` 删除（零消费者），改为 `FTcsParamValue{TInstancedStruct<FTcsParamValueSource> Source}`（默认 Literal）+ 抽象基类 `FTcsParamValueSource::Evaluate`（UHT TCppStructOps 禁纯虚，采用 StateTree 同款默认体 + `meta=(Hidden)`——规格偏差已报备待追认）+ 反射可见上下文 `FTcsParamEvaluateContext`（禁 TFunction 成员）+ `ITcsParamTableReader` UINTerface + 内置源 `FTcsParamSource_Literal{Value}` / `FTcsParamSource_ParamRef{Key, Fallback}`。
- `Source/TcsNotation/`：根壳三文件 + 日志通道（`LogTcsNotation`）+ `Public/FTcsValueConvention.h`（`ETcsValueConventionFlag` EnumFlags + 静态 `ConvertToCanonical`，固定组合顺序 Percent→OneMinus→Negate，D5-18）。
- `Source/TcsAttribute/`：根壳三文件 + 日志通道（`LogTcsAttribute`）——类型文件留待 plan1 Task 4。
- 目录收窄口径：模块根仅 Build.cs/Module.h/Module.cpp；其余 Public/Private 分层 + 领域子目录 PascalCase；日志独立通道文件，使用日志不 include Module.h。

### Step 4：冒烟编译
- 按 unreal-cpp-compile 技能刷新 LAC 工程项目文件（-projectfiles）后执行 Development Editor 编译，以编译通过为 Task 0 完成门槛。

## Impact
- Affected specs: `plugin-descriptor`、`cpp-module-structure`（新建能力）。
- Affected code: `TireflyCombatSystem.uplugin`（已改）+ 新增 `Source/` 树 19 个骨架文件 + LAC 工程生成文件刷新（Intermediate/不提交）。
- 编译验证通过前宿主工程不可编译属预期；编译属 Step 4 范围内动作。
