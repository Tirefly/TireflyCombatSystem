## ADDED Requirements

### Requirement: 战斗实体身份句柄与发号

TcsCore MUST 提供 `FTcsCombatEntityHandle`（uint64 `Id`，0 = 无效）与 `FTcsCombatEntityHandleRegistry`（`std::atomic` 递增发号、首个分配为 1、进程内永不复用）——形式与既有的 `FTcsSourceHandle` / `FTcsSourceHandleRegistry` 对齐（同为 Core 的词汇句柄，同住 `Handle/` 目录）。MUST 提供 `IsValid`、相等比较与 `GetTypeHash`（供 `TMap` 键控）。

该类型是 **Core"零战斗词汇"边界上的一处明示让步**（PV-1 记录：`FTcsParamEvaluateContext` 等 Core 设施不得反向依赖域模块，故实体身份是全插件通用键、唯一由 Core 持有的战斗实体词汇；02 §2.2a / 06 已按通用实体键使用）。与池化实例句柄的区别：`FTcsCombatEntityHandle` **无代际段**——Id 单调递增且永不复用，悬空表现为"查不到"而非"误命中回收槽"；代际校验只属于 `TTcsInstancePool` 的槽位复用语义。**发号方归属**：R3 由 M2 门面（`UTcsAttributeSubsystem::RegisterUnit`）发号；M6 世界注册表落地后移交发号（句柄类型与消费者签名不变——见 design.md D1）。

#### Scenario: 发号唯一且非零

- **WHEN** 连续调用 `Allocate()` 两次
- **THEN** 返回两个互异且 `IsValid()`（非零）的实体 Id

#### Scenario: 句柄可作 TMap 键

- **WHEN** 以同一句柄两次读写同一 `TMap<FTcsCombatEntityHandle, …>`
- **THEN** 命中同一槽位（相等 + 哈希一致）

#### Scenario: 与来源句柄类型隔离

- **WHEN** 把 `FTcsCombatEntityHandle` 传给期望 `FTcsSourceHandle` 的 API（级联撤销）
- **THEN** 编译失败（"被修饰主体"与"归属来源"不可互换——避免同型误传在运行期才暴露）
