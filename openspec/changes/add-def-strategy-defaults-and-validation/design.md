## 背景

这次变更同时跨越了三层边界：
- DefAsset 的 authoring 默认值与编辑器勘误
- shared `StateParam` evaluator 的默认 constant 语义
- `SkillModifierDefinition` 从 Numeric-only 向 typed evaluator authoring 扩展

如果只改其中一层，会留下明显的不一致：比如 DefAsset 已经声明默认值，但 DataTableRow 仍然允许空类；或者 runtime 已支持 Bool / Vector SkillModifier 执行器，但 DefAsset 层永远配不进去。

## 目标 / 非目标

- 目标：
  - 为 TCS DefAsset 中需要“稳定默认策略”的字段补齐 concrete 默认类
  - 保证默认值在 DefAsset 与 DataTableRow 之间可双向镜像
  - 为 DefAsset 增加明确的编辑器有效性校验与报错提示
  - 让 `SkillModifierDefinition` 完整支持 Numeric / Bool / Vector 三类 evaluator authoring
- 非目标：
  - 不为 `AttributeModifierDefinition.ModifierType` 增加默认值
  - 不为 `ActiveConditions` 自动注入默认元素
  - 不在 DataTableRow 层复制 DefAsset 的勘误逻辑
  - 不把 `UTcsSkillEntrySelector` 纳入本次默认值范围

## 决策

- 决策：对以下字段补齐 concrete 默认类
  - `UTcsAttributeDefinition::ClampStrategyClass` → `UTcsAttrClampStrategy_Linear`
  - `UTcsAttributeModifierDefinition::MergerType` → `UTcsAttrModMerger_NoMerge`
  - `UTcsBuffDefinition::MergerType` → `UTcsBuffMerger_NoMerge`
  - `UTcsStateSlotDefinition::SamePriorityPolicy` → `UTcsStateSamePriorityPolicy_UseNewest`
  - `FTcsStateParameter` 的 Numeric / Bool / Vector evaluator → 各自 shared constant evaluator
  - `UTcsSkillModifierDefinition` 的 Numeric / Bool / Vector evaluator → `UTcsSkillModExec_Addition` / `UTcsSkillModExec_SetBool` / `UTcsSkillModExec_SetVector`

- 决策：`UTcsStateBoolParamEvaluator` 与 `UTcsStateVectorParamEvaluator` 调整为 concrete constant evaluator
  - 它们已经实现了“从 constant payload 读取值”的默认行为，只是当前错误地保留了 `Abstract`
  - 三类 shared evaluator 的 `Evaluate` 形参命名统一收敛为 `Payload`，并继续按各自 constant payload 结构解析

- 决策：shared `FTcsStateParameter` 只共享“默认值归一化”，不共享 DefAsset 校验入口
  - Numeric / Bool / Vector evaluator 的默认值补齐规则仍然由 shared `FTcsStateParameter` 复用
  - 但 `UTcsStateDefinition`、`UTcsSkillDefinition` 等 DefAsset 的有效性校验继续放在各自 `IsDataValid()` 中
  - 不单独引入 `StateParamAuthoring` 文件，也不从 `Instance` 层暴露 `ValidateStateParameterStrategySelection` 之类的公共校验 helper

- 决策：`UTcsSkillModifierDefinition` 改为类型化 evaluator authoring
  - 保留单一 `TargetParamType`
  - 将当前单一 `EvaluatorClass` 改为 `NumericEvaluatorClass` / `BoolEvaluatorClass` / `VectorEvaluatorClass`
  - 三者通过 `EditCondition` / `EditConditionHides` 与 `TargetParamType` 绑定
  - 仍然保留单一 `EvaluatorConfig`，由当前激活的 evaluator 类决定其 payload 解释方式

- 决策：DataTable 同步负责“默认值归一化”，不负责“DefAsset 级勘误”
  - Row → Asset 同步时，如果遇到可默认化字段为空，则写入对应 concrete 默认类
  - Asset → Row 同步时，把归一化后的 concrete 默认类显式写回 RowStruct
  - RowStruct 本身不做错误日志、编辑器通知或 `IsDataValid` 级别勘误

- 决策：`StateParamModifierExecution` 的三条默认值必须使用 concrete 执行器，而不是抽象基类
  - 本设计将“`StateParamModifierExec` 不适用抽象类”解释为“默认值不能落到抽象类”
  - 因此默认值使用 `Addition` / `SetBool` / `SetVector`
  - 抽象执行基类是否也要取消 `Abstract` 不作为本提案的既定范围，除非后续明确提出

## 风险 / 取舍

- 风险：已有旧资产可能长期保存着空策略字段
  - 缓解：在保存、校验和同步路径上统一做默认值归一化，并给出勘误提示

- 风险：`SkillModifierDefinition` 字段形状变化会影响 DataTableRow 映射与现有 authoring 代码
  - 缓解：保持单一 `TargetParamType + EvaluatorConfig` 模型，只扩展 evaluator 类字段，不引入新的多参数模型

- 风险：如果把 DataTableRow 也做成和 DefAsset 一样的勘误入口，会把表编辑工作流变得过重
  - 缓解：RowStruct 仅镜像默认值，不复制错误通知与验证职责

- 风险：如果把 shared `FTcsStateParameter` 的 authoring 校验抽成公共入口，容易再次把 DefAsset 规则错误地挂到 `Instance` 层
  - 缓解：仅共享最小默认值归一化逻辑；参数合法性判断保持在具体 DefAsset 的本地校验代码中

## 迁移计划

1. 调整 shared evaluator 的 concrete / payload 契约
2. 扩展 `SkillModifierDefinition` 与 `FTcsSkillModifierDefRow` 的字段结构
3. 为受支持的 DefAsset 策略字段补齐默认值与勘误逻辑
4. 更新 DataTable ↔ DefAsset 双向同步描述符与 direct-assignment 映射
5. 编译与 authoring 验证

## 开放问题

- 当前无；本提案按“`StateParamModifierExec` 默认值不能使用抽象类，但抽象执行基类本身是否去抽象化不在本次范围内”执行。