# 变更：重划 Attribute 操作数/操作符文件目录

## 背景
`Attribute/AttrModOperation/` 目录同时混放了「Operand 求值」与「Operator 应用」两类职责，且文件命名未遵循同级策略族目录（`AttrModMerger/`、`AttrClampStrategy/`）的「族基类 + 族短前缀具体文件」约定。

## 变更内容
- 新增 `Attribute/AttrModOperand/` 作为 Operand 求值族目录，归入 Payload 基类、NumericEvaluator 基类、OperandEvaluatorContext 与三个具体 Operand。
- 保留 `Attribute/AttrModOperation/` 作为 Operator 应用族目录，仅保留 `TcsAttributeModifierOperation` 与 `TcsAttributeModifierCustomOperator`。
- 具体 Operand 文件重命名为 `TcsAttrModOperand_<Kind>`；族基类文件重命名为 `TcsAttributeModifierOperandEvaluator`（及其 Context）。
- 仅调整文件路径与 `#include`，不改任何类型名、反射声明或运行时行为。

## 影响范围
- 受影响规范：`runtime-module-file-layout`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public|Private/Attribute/AttrModOperand/*`
  - `Source/TireflyCombatSystem/Public|Private/Attribute/AttrModOperation/TcsAttributeModifierOperation*`
  - `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent_AttrModEvaluation.cpp`
  - `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent_AttrModHelpers.h`
  - `Source/TireflyGameplayUtils/Public/ManualTest/TcsManualTestStrategies.h`
