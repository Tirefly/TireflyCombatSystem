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

### Requirement: 文件名去类型前缀

仓库内每个 `.h` / `.cpp` 文件名 MUST 与其承载类型同名但**不含类型前缀字母**（A/U/F/T/I）——UE 官方命名规范项。U/I 成对（`UINTERFACE` + 接口类）同住一文件时，文件名取去前缀后的类型名（`TcsAttributeProvider.h` 承载 `UTcsAttributeProvider` / `ITcsAttributeProvider`）；一文件多类型时取主导类型。模块壳 `Tcs<模块名>Module.h/.cpp` 与日志通道 `Tcs<模块名>LogChannel.h/.cpp` 不受本规则约束（后者的载体是日志分类名而非类型名）。改名 MUST 连带同步 `.generated.h` include（UHT 按头文件名生成产物）与全库路径引用。

#### Scenario: 类文件不带前缀

- **WHEN** 检查承载 `UTcsAttrModDef` 的文件名
- **THEN** 为 `TcsAttrModDef.h` / `TcsAttrModDef.cpp`（非 `UTcsAttrModDef.*`）

#### Scenario: 值类型与接口同样去前缀

- **WHEN** 检查承载 `FTcsAttributeName`、`TTcsInstancePool`、`ITcsTimeSource` 的文件名
- **THEN** 依次为 `TcsAttributeName.h`、`TcsInstancePool.h`、`TcsTimeSource.h`

#### Scenario: 改名后 UHT 产物与 include 同步

- **WHEN** 重命名一个反射类型的头文件
- **THEN** 该头内的 `.generated.h` include 同步改名，全量编译通过（UHT 产物名跟随头文件名）

