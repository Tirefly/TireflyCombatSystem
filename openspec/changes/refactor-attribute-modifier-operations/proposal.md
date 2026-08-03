# 变更：重构 AttributeModifier Operation 运行时与创作链

## Why

当前 AttributeModifier 仍是“单 Attribute + ModifierMode + Operands Map + Execution 类 + OperandBindings”模型。它无法表达一次 Application 内的多目标 Operation、Instant BaseValue 原子结算与 Ongoing CurrentValue 聚合的统一入口，也把 StateParam 绑定、Execution 与数值写入层混在一起。

Change 1 与 Change 2 已提供 SourceHandle 工厂与 Attribute Base/Current 底盘。现在需要纵向替换 Definition、DataTable、Apply 入口、Merger 输入语义、StateTree 调用方和相关规格，形成可编译、可使用、可验收的新 AttributeModifier 中间态，并为后续依赖链重算与伤害模块提供稳定入口。

## What Changes

- **BREAKING**：建立 `OperandPayload -> OperandEvaluator -> EvaluatedOperand -> Operator` 模型；AttributeModifier 固定为 Numeric / `float`。
- **BREAKING**：`UTcsAttributeModifierDefinition` 改为 `TMap<FName, FTcsAttributeOperationDefinition>`（Key = 稳定 `OperationId`）；删除 Def 级 `AttributeId`、`ModifierMode`、`Operands`、`ModifierType`。
- **BREAKING**：删除 `UTcsAttributeModifierExecution` 及其内建实现；内建 Operator 改为枚举 / Custom Operator 策略。
- **BREAKING**：删除 `CreateAttributeModifier`、`CreateAttributeModifierWithBindings`、`ApplyModifier`、`ApplyModifierWithSourceHandle`、`OperandBindings` 与旧 `Operands` 权威读取口径。
- 建立唯一入口 `ApplyAttributeModifier(Request, OutResult)`：
  - `Instant`：原子写入目标 Attribute 的 BaseValue。
  - `Ongoing`：创建可撤销父实例并参与 CurrentValue 聚合。
- 所有有效请求（含 Instant）必须携带有效 `SourceHandle`；任一 Operation 失败则整次 Application 零提交。
- 引入只读 `AttributeEvaluationSnapshot`；Ongoing 重算时虚拟排除当前父实例全部旧结果。
- Merger 只用于 Ongoing，且在 Operator 前处理 EvaluatedOperand；兼容规则由 `TcsDeveloperSettings` 以二元 `Allowed` / `Forbidden` 表达。
- **BREAKING**：所有 Ongoing 必须经由 StateInstance 持有与施加；SkillInstance 不得直接 Ongoing；Instant 可直接作用于 TargetAttributeComponent（含跨 Actor）。
- **BREAKING**：删除 `TcsSTTask_ApplyAttributeModifierToTarget`；迁移 Owner 侧 StateTree Task 到新 Request 模型。
- 保留 `ModifierInstId`；**BREAKING**：删除 `ModifierChangeBatchId` 及其静态计数器。
- `FTcsAttributeModifierDefRow` 与新 Def 非标识字段 1:1 对齐；不迁移旧 Row / 旧资产。
- `RemoveAttribute` 被任意 Ongoing Operation 作为目标引用时硬拒绝且零修改。

## Impact

- 受影响规范：
  - `attribute-modifier-runtime`（整能力重写）
  - `attribute-management`
  - `def-editor-authoring`
  - `skill-runtime`
  - `combat-manager-subsystems`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public|Private/Attribute/TcsAttributeModifier*.h|cpp`
  - `Source/TireflyCombatSystem/Public|Private/Attribute/TcsAttributeComponent*.h|cpp`
  - `Source/TireflyCombatSystem/Public|Private/Attribute/AttrModExecution/*`
  - `Source/TireflyCombatSystem/Public|Private/Attribute/AttrModMerger/*`
  - AttributeModifier Definition / DataTable Row / Def sync
  - `TcsDeveloperSettings` Operator/Merger 兼容规则
  - StateTree Apply AttributeModifier Task
  - Buff / Skill / State 调用方与相关自动化测试

## Non-Goals

- 不实现 Ongoing 依赖链自动标脏、Dirty Flush、拓扑排序或循环检测（Change 4）。
- 不实现伤害、治疗、护盾、暴击或战斗记录业务模块（Change 5）。
- 不为旧 AttributeModifier Definition、DataTable 行、OperandBindings 或 Execution 类提供兼容层或自动迁移。
- 不重做 SkillModifier 的完整 Operand / Operator 架构。
- 不引入跨 Actor Ongoing、跨 Actor 全局依赖图或业务贡献分类账本。
- 不把动态 Merger 下拉过滤的纯编辑器交互增强拆成独立兼容规则；若实现量过大，可后置 UI 过滤，但规则结构、Data Validation 与运行时防御必须在本 change 完成。
