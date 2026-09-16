# cpp-module-structure Specification

## Purpose
TBD - created by archiving change update-uplugin-for-r3-modules. Update Purpose after archive.
## Requirements
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

