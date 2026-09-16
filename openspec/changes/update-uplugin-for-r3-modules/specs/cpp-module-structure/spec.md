## ADDED Requirements

### Requirement: 模块骨架目录收窄口径

每个 UE 模块的根目录 MUST 仅平铺三（组）模块壳文件——`<模块名>.Build.cs`、`<模块名>Module.h`、`<模块名>Module.cpp`（IModuleInterface 实现 + IMPLEMENT_MODULE）；其余代码 MUST 按 `Public/` / `Private/` 分层，对外头文件放 `Public/<领域子目录>/`（领域子目录 PascalCase）；日志分类 MUST 独立通道文件（`Public/<模块名>LogChannel.h` 声明 + `Private/<模块名>LogChannel.cpp` 定义），使用日志只 include LogChannel 头、不 include Module.h。

#### Scenario: 模块根仅壳文件

- **WHEN** 列出任一模块（TcsCore/TcsNotation/TcsAttribute）根目录
- **THEN** 仅存在 `<模块名>.Build.cs`、`<模块名>Module.h`、`<模块名>Module.cpp` 与 `Public/`、`Private/` 目录

#### Scenario: 日志通道独立成对

- **WHEN** 检查任一模块的日志分类声明与定义
- **THEN** 声明在 `Public/<模块名>LogChannel.h`（DECLARE_LOG_CATEGORY_EXTERN(LogTcs<名>, Log, All)）、定义在 `Private/<模块名>LogChannel.cpp`（DEFINE_LOG_CATEGORY），Module.h/Module.cpp 只含模块类

### Requirement: 最小编译集依赖方向

三模块 Build.cs 依赖 MUST 符合 R0 §9：TcsCore 仅依赖引擎基础（Core/CoreUObject/Engine/GameplayTags，禁依赖任何兄弟/上层模块）；TcsNotation 仅依赖 Core/CoreUObject（与 TcsCore 平级互不依赖）；TcsAttribute 依赖引擎基础 + TcsCore + TcsNotation（D5-18 v2）；禁止反向 include。

#### Scenario: 依赖按 R0 §9 落位

- **WHEN** 读取三模块 Build.cs 的 PublicDependencyModuleNames
- **THEN** TcsCore 不含任何 Tcs* 模块，TcsNotation 不含任何 Tcs* 模块，TcsAttribute 恰好含 TcsCore 与 TcsNotation

### Requirement: Task 0 载荷类型与设施

TcsCore MUST 提供 header-only 的 `FTcsParamValue` 载体体系（PV-1/PV-2，2026-09-11 补充改造，取代 D2-12 FTcsParamScalar）：载体 `FTcsParamValue{TInstancedStruct<FTcsParamValueSource> Source}`（默认 Literal）；抽象基类 `FTcsParamValueSource::Evaluate`（UHT TCppStructOps 禁纯虚——StateTree 同款默认体 + `meta=(Hidden)`）；反射可见上下文 `FTcsParamEvaluateContext`（禁 TFunction/std::function 成员，参数表走 `TScriptInterface<ITcsParamTableReader>`）；内置源 `FTcsParamSource_Literal{Value}` / `FTcsParamSource_ParamRef{Key, Fallback}`（限同域、Fallback 必填）；`ITcsParamTableReader` UINTerface（`TryGetNumericParam(FName, out double)`，miss=false，宿主/UnrealSharp 可实现）。TcsNotation MUST 提供 `ETcsValueConventionFlag`（VCF_None=0/VCF_Percent/VCF_OneMinus/VCF_Negate，EnumFlags）与静态 `ConvertToCanonical` 转换助手（固定组合顺序 Percent→OneMinus→Negate，D5-18）；TcsCore MUST 提供 `UTcsDeveloperSettings` 空壳（Config 分类名 `Tcs`）；Task 0 不落任何其他领域类型（属性类型留 plan1 Task 4）。

#### Scenario: 统一数值载体就位

- **WHEN** 检查 `Source/TcsCore/Public/Parameter/` 目录
- **THEN** 存在五个 header-only 文件（FTcsParamValue / FTcsParamValueSource / ITcsParamTableReader / FTcsParamSource_Literal / FTcsParamSource_ParamRef），FTcsParamValue 默认构造后 Source 为 Literal 源，FTcsParamScalar 已删除

#### Scenario: 值约定转换顺序固定

- **WHEN** 调用 `FTcsValueConvention::ConvertToCanonical(85, VCF_Percent|VCF_OneMinus|VCF_Negate)`
- **THEN** 按 Percent（÷100）→ OneMinus（1−v）→ Negate（取负）的固定顺序返回 -0.15
