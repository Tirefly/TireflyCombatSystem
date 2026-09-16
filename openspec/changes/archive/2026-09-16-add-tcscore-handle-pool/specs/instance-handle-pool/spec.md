## ADDED Requirements

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
