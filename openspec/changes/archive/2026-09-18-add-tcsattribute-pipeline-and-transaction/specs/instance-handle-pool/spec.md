## MODIFIED Requirements

### Requirement: 战斗实体身份句柄与发号

TcsCore MUST 提供 `FTcsCombatEntityHandle`（**`USTRUCT(BlueprintType)`**，`int64 Id`，0 = 无效）与 `FTcsCombatEntityHandleRegistry`（`std::atomic<int64>` 递增发号、首个分配为 1、进程内永不复用）——与既有的 `FTcsSourceHandle` / `FTcsSourceHandleRegistry` 同域（Core 的词汇句柄，同住 `Handle/` 目录）。MUST 提供 `IsValid`、相等比较与 `GetTypeHash`（供 `TMap` 键控）。

**反射性（2026-09-18 升格）**：本句柄是**身份词汇**（token 化，非裸整数），MUST 可出现在反射载荷与反射字段里——属性变更事件 `FTcsAttributeChangedEvent.Unit`（总线载荷 `FInstancedStruct` MUST 反射可见）与 PV-1 规划的上下文 `Subject`（`UPROPERTY`）是首批消费者。因此：结构体 MUST 带模块导出宏（反射类型有 UHT 生成符号 → 按导出宏纪律必须带；纯内联的 `FTcsSourceHandle` 反而不得带——两者差异正是该纪律的两面）；`Id` MUST 为 `int64`（UHT 不支持 `uint64` 作为属性类型；句柄 Id 恒为正，无符号性差异无实际影响）。

该类型是 **Core"零战斗词汇"边界上的一处明示让步**（PV-1 记录：`FTcsParamEvaluateContext` 等 Core 设施不得反向依赖域模块，故实体身份是全插件通用键、唯一由 Core 持有的战斗实体词汇；02 §2.2a / 06 已按通用实体键使用）。与池化实例句柄的区别：`FTcsCombatEntityHandle` **无代际段**——Id 单调递增且永不复用，悬空表现为"查不到"而非"误命中回收槽"；代际校验只属于 `TTcsInstancePool` 的槽位复用语义。**发号方归属**：R3 由 M2 门面（`UTcsAttributeSubsystem::RegisterUnit`）发号；M6 世界注册表落地后移交发号（句柄类型与消费者签名不变——见 design.md D1）。

#### Scenario: 发号唯一且非零

- **WHEN** 连续调用 `Allocate()` 两次
- **THEN** 返回两个互异且 `IsValid()`（非零）的实体 Id

#### Scenario: 句柄可作 TMap 键

- **WHEN** 以同一句柄两次读写同一 `TMap<FTcsCombatEntityHandle, …>`
- **THEN** 命中同一槽位（相等 + 哈希一致）

#### Scenario: 句柄可进反射载荷

- **WHEN** 把 `FTcsCombatEntityHandle` 作为某个反射 USTRUCT 的 `UPROPERTY` 字段（如属性变更事件的 `Unit`）
- **THEN** UHT 编译通过、载荷可经总线（`FInstancedStruct`）传递，且宿主脚本可读该字段

#### Scenario: 与来源句柄类型隔离

- **WHEN** 把 `FTcsCombatEntityHandle` 传给期望 `FTcsSourceHandle` 的 API（级联撤销）
- **THEN** 编译失败（"被修饰主体"与"归属来源"不可互换——避免同型误传在运行期才暴露）
