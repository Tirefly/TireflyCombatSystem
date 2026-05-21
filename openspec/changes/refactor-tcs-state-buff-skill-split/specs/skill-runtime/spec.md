## ADDED Requirements

### Requirement: Skill 自有 Learned 与 Cooldown 语义
TCS SHALL 在 Skill 自有的定义、实例和组件中建模 learned skill、cooldown、激活门槛以及 Skill 侧 snapshot 配置。

#### Scenario: 已学会技能在没有活动运行时状态时仍然持久存在
- **WHEN** 一个实体学会了技能但当前并未执行它
- **THEN** learned/cooldown 状态 SHALL 仍然存在于 `UTcsSkillInstance` 与 `UTcsSkillComponent` 中
- **AND** 仅为了记住该技能存在，不应要求额外依赖通用 `State Core` 对象

#### Scenario: Skill snapshot 配置留在 Skill 领域内
- **WHEN** 一个技能需要激活时快照行为
- **THEN** 该 snapshot 配置 SHALL 由 Skill 自有数据结构 authoring 并消费
- **AND** 通用 state 参数 SHALL NOT 承载 skill-only 的 snapshot 开关

### Requirement: Skill 激活桥接到 State Runtime
TCS SHALL 让 Skill 自有激活逻辑在通过 Skill 侧校验后，再向 `UTcsStateComponent` 请求运行时状态。

#### Scenario: Skill 激活通过 State Core 创建运行时效果
- **WHEN** 一个技能通过 learned/cooldown/cost/activation 检查后
- **THEN** 该技能激活路径 MAY 向 `UTcsStateComponent` 请求一个或多个运行时状态
- **AND** 这些运行时状态 SHALL 通过共享 State Core 生命周期编排执行

#### Scenario: Skill 校验先于运行时状态请求发生
- **WHEN** 一个技能处于冷却中或因其他原因无法激活
- **THEN** `UTcsSkillComponent` SHALL 在向 `UTcsStateComponent` 请求任何运行时状态应用之前，就先拒绝这次激活

### Requirement: Skill 专属语义不污染共享 State 基类
TCS SHALL 让 Skill-only 的归属引用和激活元数据停留在共享 State 基类之外，除非它们确实被每一种运行时状态类型共享。

#### Scenario: 共享状态实例排除 Skill-only 归属元数据
- **WHEN** 定义共享运行时状态实例数据时
- **THEN** skill-only 的 owner 引用、learned-state 元数据和 cooldown 元数据 SHALL NOT 仅为 Skill 方便而存放到通用 state 基类上
- **AND** 激活时所需的任何桥接数据，都 SHALL 通过显式的 Skill 自有接口或 Skill 自有运行时类型传递