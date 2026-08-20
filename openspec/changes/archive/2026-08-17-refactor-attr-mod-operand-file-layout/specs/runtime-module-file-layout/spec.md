## ADDED Requirements
### Requirement: Attribute 策略族按子目录归组
TCS SHALL 将 Attribute 模块中同一策略族的头文件与实现文件归入专属子目录，子目录名与族职责一致。

#### Scenario: Operand 求值族与 Operator 应用族分目录
- **WHEN** AttributeModifier 同时存在 Operand 求值与 Operator 应用两类职责
- **THEN** Operand 求值文件（Payload 基类、NumericEvaluator 基类、OperandEvaluatorContext 与具体 Operand）SHALL 归入 `Attribute/AttrModOperand/`
- **AND** Operator 应用文件（Operator 枚举、Operation Spec、Apply 入口与 Custom Operator）SHALL 归入 `Attribute/AttrModOperation/`

#### Scenario: 具体策略文件使用族短前缀命名
- **WHEN** 一个策略族包含多个具体实现文件
- **THEN** 具体实现文件 SHALL 采用 `TcsAttr<族缩写>_<策略>` 命名（如 `TcsAttrModOperand_Constant`、`TcsAttrModMerger_UseNewest`）
- **AND** 族基类文件保留 `TcsAttribute<族名>` 描述性命名（如 `TcsAttributeModifierOperandEvaluator`）
