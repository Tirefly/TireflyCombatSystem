# 变更：为 DefAsset 策略字段补默认值与编辑器勘误

## 背景

当前 TCS 的多个 DefAsset 已经使用策略模式 `UClass` 字段承载 Clamp、Merger、Evaluator 与 Selector 等可扩展行为，但这些字段的 authoring 体验并不一致：一部分字段没有稳定默认值，一部分默认候选是抽象类，一部分 DataTableRow / DefAsset 同步面还停留在旧的单类型结构上。

这会带来三个直接问题：
- 新建或同步出来的 DefAsset 容易出现空策略字段，后续运行时或编辑器验证才暴露问题。
- DataTable ↔ DefAsset 双向同步无法稳定把“应有的默认策略”沉淀为显式数据，导致资产面与表面状态漂移。
- `UTcsSkillModifierDefinition` 的 authoring 面目前只完整支持 Numeric Evaluator，无法把已存在的 Bool / Vector 执行器纳入 DefAsset 工作流。

## 变更内容

- 为受支持的 DefAsset 策略字段定义稳定默认值，并要求同步后的 DataTableRow 镜像这些默认值。
- 为 DefAsset 增加编辑器阶段的有效性校验与错误提示；DataTableRow 不增加对应勘误逻辑。
- 对 shared `FTcsStateParameter` 仅保留最小默认值归一化规则；各 DefAsset 的校验继续留在各自 `IsDataValid()` 中，不从 `Instance` 层导出公共校验入口。
- 将共享 `StateParam` 的 Bool / Vector Constant Evaluator 从“抽象基类”收敛为“可直接 authoring 的 concrete 默认 Evaluator”。
- 将 `UTcsSkillModifierDefinition` 与其对应 RowStruct 从“仅 Numeric EvaluatorClass”扩展为按 `TargetParamType` 暴露 Numeric / Bool / Vector 三个 EvaluatorClass，并为三者提供 concrete 默认执行器。

## 影响范围

- 受影响规范：`def-editor-authoring`、`state-parameter-management`、`skill-runtime`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public/Attribute/*Definition*.h`
  - `Source/TireflyCombatSystem/Public/Buff/TcsBuffDefinition.h`
  - `Source/TireflyCombatSystem/Public/State/TcsStateSlotDefinition.h`
  - `Source/TireflyCombatSystem/Public/State/TcsStateParamInstance.h`
  - `Source/TireflyCombatSystem/Public/State/StateParameter/*.h`
  - `Source/TireflyCombatSystem/Public/Skill/TcsSkillModifierDefinition.h`
  - `Source/TireflyCombatSystem/Public/Skill/SkillModExecution/*.h`
  - `Source/TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`
  - `Source/TireflyCombatSystem/Private/DataTableSync/*`
  - `Source/TireflyCombatSystem/Private/State/TcsStateDefinition.cpp`
  - `Source/TireflyCombatSystem/Private/Skill/TcsSkillDefinition.cpp`
  - 相关 DefAsset 的 `PostEditChangeProperty` / `IsDataValid` / 构造与同步逻辑