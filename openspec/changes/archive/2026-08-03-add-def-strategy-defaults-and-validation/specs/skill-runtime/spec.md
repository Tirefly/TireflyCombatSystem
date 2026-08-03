## MODIFIED Requirements
### Requirement: SkillModifierDefinition 定义资产

`UTcsSkillModifierDefinition` SHALL 是一个 DefAsset，声明修改目标、与 `TargetParamType` 对应的 typed evaluator 类、优先级和互斥策略。一个 Def 只修改一个 StateParam。`TargetParamType` SHALL 决定 Numeric / Bool / Vector 三个 evaluator 字段中哪一个是当前有效的 authoring 输入；当当前类型对应字段为空时，系统 SHALL 归一化为对应的 concrete 默认执行器。

#### Scenario: 配置 Numeric SkillModifierDef
- **WHEN** 创建一个 Numeric 类型的 `UTcsSkillModifierDefinition`
- **AND** `TargetParamType = Numeric`
- **THEN** authoring 面 SHALL 使用 Numeric evaluator 字段
- **AND** 若该字段为空，系统 SHALL 将其归一化为 `UTcsSkillModExec_Addition`

#### Scenario: 配置 Bool 或 Vector SkillModifierDef
- **WHEN** 创建一个 Bool 或 Vector 类型的 `UTcsSkillModifierDefinition`
- **THEN** authoring 面 SHALL 分别使用 Bool 或 Vector evaluator 字段
- **AND** 若对应字段为空，系统 SHALL 分别归一化为 `UTcsSkillModExec_SetBool` 或 `UTcsSkillModExec_SetVector`

#### Scenario: 只有匹配类型的 evaluator 字段可编辑
- **WHEN** 开发者切换 `TargetParamType`
- **THEN** 只有与当前类型匹配的 evaluator 字段应保持可编辑
- **AND** 其他类型字段不应继续作为当前 Def 的有效 authoring 输入

#### Scenario: 修改多个参数需多个 Def
- **WHEN** 天赋需要同时 +Level 和 +DamageFactor
- **THEN** 必须创建两个独立的 SkillModifierDef