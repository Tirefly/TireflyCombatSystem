## ADDED Requirements

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
