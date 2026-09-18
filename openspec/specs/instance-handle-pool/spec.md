# instance-handle-pool Specification

## Purpose
TBD - created by archiving change add-tcscore-handle-pool. Update Purpose after archive.
## Requirements
### Requirement: 强类型实例句柄

TcsCore MUST 提供 `TTcsInstanceHandle<TTagType>`（uint32 Index + uint32 Generation）：`InvalidIndex = 0xFFFFFFFF`、`IsValid()` 判索引有效、相等比较按 (Index, Generation) 全等；`TTagType` MUST 为编译期幻影标签（空结构，仅作类型区分、零运行时开销）——不同池的句柄类型不可互换。

#### Scenario: 句柄类型隔离

- **WHEN** 把 `TTcsInstanceHandle<TagA>` 传给期望 `TTcsInstanceHandle<TagB>` 的池 API
- **THEN** 编译失败（铁律 4 的句柄版——跨池误用在编译期拦截，无运行期成本）

### Requirement: 归属来源标识与发号

TcsCore MUST 提供 `FTcsSourceHandle`（uint64 Id，0 = 无效）与 `FTcsSourceHandleRegistry::Allocate()`（std::atomic 递增、首个分配为 1、进程内永不复用）；Registry 只发号、不做注销登记——级联撤销由消费方按 Source 分组移除（D2-2），来源失效不产生句柄语义分支。

#### Scenario: 发号唯一且非零

- **WHEN** 连续调用 `Allocate()` 两次
- **THEN** 返回两个互异且 `IsValid()`（非零）的来源 Id

### Requirement: 实例池的分配释放与解析

`TTcsInstancePool<TInstanceType, TTagType>` MUST 以稠密数组 + FreeList 复用 + 代际数组实现 `Allocate/Free/Resolve/IsValid/ForEach`：Free 使该槽代际 +1（旧句柄凭代际失配判悬空）；`Resolve` 对悬空/无效句柄 ensure（Development 编译期剔除后 Shipping 返回 nullptr）；`ForEach` 按槽位 Index 升序稳定遍历且跳过空闲槽（代际奇偶簿记：奇 = 已分配、偶 = 空闲）；Allocate/Free/Resolve/ForEach 进入点 MUST 断言游戏线程（D0-4，ensure(IsInGameThread())）；Free 不清零槽位内容——持有外部资源的实例由调用方在 Free 前自行清理。

#### Scenario: 悬空句柄解析

- **WHEN** Allocate → Free → 用旧句柄调用 `Resolve`（Development）
- **THEN** ensure 命中且返回 nullptr；池内其他槽位的句柄解析不受影响

#### Scenario: 槽位复用

- **WHEN** Allocate → Free → 再 Allocate
- **THEN** 新句柄复用同一 Index 且 Generation 较旧句柄递增（两句柄互异）

#### Scenario: 稳定序遍历跳过空闲槽

- **WHEN** 分配 A、B 后 Free A，执行 `ForEach`
- **THEN** 仅 B 被访问一次（Index 升序、无空洞）

### Requirement: 池占用统计

TcsCore MUST 提供统计口 `FTcsCorePoolStats`：`AddSlotCount()` / `RemoveSlotCount()` 为**无参对**（池槽位操作粒度恒为 1——正负号与数量歧义在签名上杜绝，2026-09-15 用户拍板拆分），`GetSlotCount()` 与控制台变量 `Tcs.Core.PoolSlots` 同源（跨池当前占用槽位总数，只读参考值）；FreeList 复用路径不改变计数；计数仅游戏线程读写（D0-4）。

#### Scenario: 统计随分配与回收变化

- **WHEN** 新建稠密槽两次、回收一次（FreeList 复用一次不计）
- **THEN** `Tcs.Core.PoolSlots` 读数净 +1

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

