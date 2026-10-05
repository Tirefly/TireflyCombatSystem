## ADDED Requirements

### Requirement: 属性门面的解析点

`TcsAttribute` MUST 提供**唯一的门面查找点** `UTcsAttributeSubsystem::Resolve(const UWorld*)`（静态；**非确保**：非游戏世界或该世界无本子系统时返回 `nullptr`）：

- **消费方 MUST 只经它取门面**：`TcsState` 与 `TcsEffect` 各自的"属性访问解析点"（`FTcsStateAttributeAccess` / `FTcsEffectAttributeAccess`）MUST 以它为实现，MUST NOT 各自散写 `World->GetSubsystem<UTcsAttributeSubsystem>()`；
- **白名单仍归各消费模块**：解析点 MUST 是**薄包装**（暴露面 = 该模块自己的白名单方法，而不是把门面指针交出去）；两个模块的白名单**不合并**——各自的审计面不同（`TcsState` 侧窄，`TcsEffect` 侧另需取值口 `EvaluateCurrent`）；
- **该点是注入契约的替换点**：将来若补属性访问注入契约（`ITcsAttributeAccess`，台账 `R-7`），MUST 只改这一处，两个消费模块的解析点不动；
- 解析失败 MUST NOT `ensure`：返回 `nullptr`，调用方按"单位无属性账本"的既有分支处置（`Log` 级、零红字）。

#### Scenario: 两个消费模块共用同一查找点

- **WHEN** 检视 `TcsState` 与 `TcsEffect` 两侧解析点的实现
- **THEN** 两者都经 `UTcsAttributeSubsystem::Resolve` 取门面，且各自的转发面只有自己的白名单方法

#### Scenario: 解析失败不 ensure

- **WHEN** 传入非游戏世界或该世界无属性子系统
- **THEN** 返回 `nullptr`，不 ensure、不崩溃
