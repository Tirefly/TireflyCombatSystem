## ADDED Requirements
### Requirement: Shared StateParam constant evaluator 可直接 authoring
TCS SHALL 将 Numeric / Bool / Vector 三类 shared `StateParam` evaluator 统一收敛为可直接 authoring 的 concrete constant evaluator，并保持三者对 typed payload 的默认解析语义一致。

#### Scenario: Bool 与 Vector evaluator 可作为 concrete 默认类被选择
- **WHEN** 开发者 authoring 一个 Bool 或 Vector 类型的 `FTcsStateParameter`
- **THEN** `UTcsStateBoolParamEvaluator` 与 `UTcsStateVectorParamEvaluator` SHALL 可作为可选的 concrete evaluator 类出现
- **AND** 它们 SHALL 不再因为抽象标记而失去默认值候选资格

#### Scenario: Shared evaluator 继续解析各自的 constant payload
- **WHEN** Numeric / Bool / Vector shared evaluator 在没有自定义子类覆写的情况下执行默认逻辑
- **THEN** 它们 SHALL 分别从各自 typed constant payload 中读取值
- **AND** 这种默认行为 SHALL 继续作为对应参数类型的稳定基线语义

#### Scenario: Shared StateParameter 只共享默认值规则
- **WHEN** DefAsset authoring 需要为 `FTcsStateParameter` 的 evaluator 字段补齐默认值
- **THEN** 系统 SHALL 允许复用 shared `FTcsStateParameter` 的默认值归一化逻辑
- **AND** DefAsset 级别的参数合法性校验 SHALL 留在具体 DefAsset 自身
- **AND** shared `FTcsStateParameter` SHALL NOT 成为通用 DefAsset 校验入口