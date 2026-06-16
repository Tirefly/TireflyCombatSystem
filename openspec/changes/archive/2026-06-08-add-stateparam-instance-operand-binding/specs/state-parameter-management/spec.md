## ADDED Requirements

### Requirement: StateParamInstance 运行时实例化

系统 SHALL 在 State 激活时为 Definition 中的每个 StateParam 创建对应的 `FTcsStateParamInstance`，存储于 `UTcsStateInstance::StateParamInstances`，Key 为 GameplayTag。

Instance SHALL 包含：
- 从 Definition 复制的 ParamData、EvaluatorClass、bIsSnapshot 配置
- CDO 缓存（Initialize 时获取并校验，失败则拒绝创建）
- 三种类型的值缓存（Numeric/Bool/Vector，根据 ParamType 只有对应字段有效）
- bHasEvaluated 守卫（Snapshot 首次求值后跳过后续重算）

#### Scenario: Instance 初始化成功
- **WHEN** StateDef.Parameters 包含 Numeric 参数，EvaluatorClass 有效
- **THEN** Initialize 返回 true，CachedEvaluator 非空，ParamTag 正确设置

#### Scenario: CDO 获取失败
- **WHEN** EvaluatorClass 指定的 CDO 获取为 nullptr
- **THEN** Initialize 返回 false，OutError 包含失败描述，StateInstance 跳过该 Param

### Requirement: Snapshot 求值策略

`Evaluate()` SHALL 根据 bIsSnapshot 决定求值行为：
- bIsSnapshot == true：首次求值后 bHasEvaluated 为 true，后续调用直接返回缓存
- bIsSnapshot == false：每次调用都重新求值

#### Scenario: Snapshot 参数只求值一次
- **WHEN** bIsSnapshot=true，首次调用 Evaluate
- **THEN** 求值器执行，Value 更新，bHasEvaluated=true
- **WHEN** 再次调用 Evaluate
- **THEN** 直接返回，求值器不再执行

#### Scenario: 非 Snapshot 参数每次求值
- **WHEN** bIsSnapshot=false，每次调用 Evaluate
- **THEN** 每次都重新调求值器，Value 更新

### Requirement: GameplayTag 统一标识

`TcsStateDefinition::Parameters` SHALL 使用 FGameplayTag 作为 Key。原有的 `TagParameters` 字段 SHALL 删除。

#### Scenario: 参数 Key 为 GameplayTag
- **WHEN** 配置 `StateParam.Attack.Power` 参数
- **THEN** Parameters Map 中以该 GameplayTag 为 Key 存储

### Requirement: 六容器替换

`UTcsStateInstance` 上原有的六个纯值容器（NumericParams/FName、NumericParams/Tag、BoolParams/FName 等）SHALL 删除，统一由 `StateParamInstances` 替代。

#### Scenario: 通过 Instance 获取参数值
- **WHEN** 调用 GetNumericParam(GameplayTag)
- **THEN** 路由到 StateParamInstances.Find(Tag)->GetNumeric()
