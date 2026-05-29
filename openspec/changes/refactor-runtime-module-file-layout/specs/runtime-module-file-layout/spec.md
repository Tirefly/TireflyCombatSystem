## ADDED Requirements

### Requirement: 超长运行时文件必须按稳定职责重组
TCS SHALL 将 Attribute、State、Buff 模块中超长的运行时实现文件按稳定职责边界重组，而不是仅按行数阈值机械切分。

#### Scenario: Buff 与 StateInstance 直接按职责拆分
- **WHEN** `TcsBuffComponent.cpp` 与 `TcsStateInstance.cpp` 已经存在相对稳定的职责块
- **THEN** 它们可以直接拆分为多个实现文件
- **AND** 拆分后的文件边界应优先反映 Owner / Duration / Lifecycle / Merge 或 Initialization / Parameters / StateTree 等职责，而不是平均分行数

#### Scenario: 高耦合流程不被机械拆散
- **WHEN** 某个实现主链内部存在强耦合流程
- **THEN** 结构重组不应为了满足行数阈值而把该主链机械拆散
- **AND** 允许在 design 中明确记录该局部完整性要求

### Requirement: 头文件职责边界可以先于 `.cpp` 拆分重整
TCS SHALL 允许在 `.cpp` 拆分之前，先重整目标头文件的 region 与声明布局，以便为后续实现文件切分提供稳定边界。

#### Scenario: AttributeComponent 先重整头文件再拆 `.cpp`
- **WHEN** `TcsAttributeComponent.h` 的当前 region 结构不足以支撑稳定的 `.cpp` 切分
- **THEN** 本次变更应先重整其头文件结构
- **AND** 随后的 `.cpp` 拆分应基于新的职责边界进行

#### Scenario: StateComponent 先重整头文件再拆 `.cpp`
- **WHEN** `TcsStateComponent.h` 的当前 region 结构无法直接映射到稳定的实现文件边界
- **THEN** 本次变更应先重整其头文件结构
- **AND** 随后的 `.cpp` 拆分应基于新的职责边界进行

### Requirement: StateManagerSubsystem 可以作为结构重组例外
TCS SHALL 允许对超过 500 行的运行时文件定义结构重组例外，只要该例外在 proposal 与 design 中被明确记录且职责边界仍然收敛。

#### Scenario: StateManagerSubsystem 保持单 `.cpp`
- **WHEN** `TcsStateManagerSubsystem.cpp` 的主要职责仍然收敛在定义加载、定义查询与跨 Actor apply 门面
- **THEN** 本次变更可以不拆分该 `.cpp`
- **AND** 仍应重整其头文件的 region、注释与声明布局

### Requirement: 目标头文件必须遵循统一的声明布局与注释规范
TCS SHALL 对纳入本次结构重组范围的目标头文件应用统一的声明布局、region 间距与成员注释规则。

#### Scenario: 顶部声明块按固定顺序分隔
- **WHEN** 一个目标头文件包含全局声明、委托声明与前置声明
- **THEN** 这三个声明块应按固定顺序组织
- **AND** 各声明块之间应使用三行空白分隔

#### Scenario: 类内 region 与成员注释统一
- **WHEN** 一个目标头文件被纳入本次结构重组范围
- **THEN** 相邻 `#pragma region` 之间应使用三行空白分隔
- **AND** 该头文件中的成员函数声明与成员变量声明应补齐注释

#### Scenario: 复杂实现函数补流程说明而非机械注释
- **WHEN** 本次变更触碰到实现相对复杂的函数
- **THEN** 对应 `.cpp` 应补充关键执行流程说明
- **AND** 不应把明显的简单语句强行扩写成高噪声注释