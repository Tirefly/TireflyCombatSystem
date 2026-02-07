# Capability: 状态同优先级排序策略

## ADDED Requirements

### Requirement: 策略接口定义

系统 MUST 提供可扩展的同优先级排序策略接口。

#### Scenario: 策略基类存在

**Given** 系统需要支持不同的同优先级排序规则
**When** 定义策略接口
**Then** 存在基类 `UTcsStateSamePriorityPolicy`
**And** 基类提供 `GetOrderKey` 方法
**And** 基类可选提供 `ShouldAcceptNewState` 方法

#### Scenario: 策略可扩展

**Given** 开发者需要自定义同优先级排序规则
**When** 创建新的策略类
**Then** 可以继承 `UTcsStateSamePriorityPolicy`
**And** 可以重写 `GetOrderKey` 方法
**And** 可以在蓝图中使用

### Requirement: 内置策略实现

系统 MUST 提供常用的内置策略实现。

#### Scenario: UseNewest 策略

**Given** 槽位配置使用 `UTcsStateSamePriorityPolicy_UseNewest` 策略
**When** 多个状态具有相同优先级
**Then** 按 `ApplyTimestamp` 降序排序
**And** 最新的状态排在最前面
**And** 适用于 Buff 槽位

#### Scenario: UseOldest 策略

**Given** 槽位配置使用 `UTcsStateSamePriorityPolicy_UseOldest` 策略
**When** 多个状态具有相同优先级
**Then** 按 `ApplyTimestamp` 升序排序
**And** 最旧的状态排在最前面
**And** 适用于技能队列槽位

### Requirement: 槽位定义集成

槽位定义 MUST 支持配置同优先级排序策略。

#### Scenario: 槽位配置策略

**Given** 定义一个新的状态槽位
**When** 配置槽位定义
**Then** 可以指定 `SamePriorityPolicy` 字段
**And** 字段类型为 `TSubclassOf<UTcsStateSamePriorityPolicy>`
**And** 默认值为 `UTcsStateSamePriorityPolicy_UseNewest`

### Requirement: 策略应用

系统 MUST 在槽位激活时正确应用同优先级排序策略。

#### Scenario: 策略排序生效

**Given** 槽位有多个同优先级状态
**And** 槽位配置了特定的排序策略
**When** 执行槽位激活更新
**Then** 对同优先级组应用策略排序
**And** 选择排序后的第一个作为 Active
**And** 其它保持 Inactive

#### Scenario: 不同策略产生不同结果

**Given** 两个槽位有相同的状态列表
**And** 槽位 A 使用 UseNewest 策略
**And** 槽位 B 使用 UseOldest 策略
**When** 执行槽位激活更新
**Then** 槽位 A 激活最新的状态
**And** 槽位 B 激活最旧的状态

### Requirement: 移除硬编码排序规则

现有的硬编码同优先级排序规则 MUST 移除，改为使用策略。

#### Scenario: 使用策略替代硬编码

**Given** 系统需要对同优先级状态排序
**When** 执行排序逻辑
**Then** 不使用硬编码的 `ApplyTimestamp` 比较
**And** 使用槽位配置的策略进行排序
**And** 排序结果由策略决定
