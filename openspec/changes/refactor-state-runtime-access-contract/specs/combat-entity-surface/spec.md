## ADDED Requirements

### Requirement: Combat Entity 接口应显式暴露 BuffComponent
TCS SHALL 在 `ITcsEntityInterface` 中为实现该接口的战斗实体提供 BuffComponent 访问出口，使 State / Buff / Skill / Attribute 组件访问面保持一致。

#### Scenario: 运行时调用方通过统一接口解析 BuffComponent
- **WHEN** 一个运行时系统需要从实现 `ITcsEntityInterface` 的 Actor 解析 BuffComponent
- **THEN** 它应能够通过统一接口出口取得 BuffComponent
- **AND** 不应要求调用方默认回退到 ad-hoc 的组件查找路径

### Requirement: 组件访问契约扩展应同步约束调用方
TCS SHALL 要求任何新增的 combat entity 组件访问出口在辅助库、缓存层或主要调用路径中保持一致使用方式。

#### Scenario: 接口新增组件出口后同步影响主要调用面
- **WHEN** `ITcsEntityInterface` 新增一个稳定组件访问出口
- **THEN** 相关辅助入口与核心调用路径应能够以同一契约消费它
- **AND** 后续补充内容应明确哪些调用点必须同步收敛
