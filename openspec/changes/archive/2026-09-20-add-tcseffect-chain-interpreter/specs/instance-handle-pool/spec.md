## MODIFIED Requirements

### Requirement: 实例池的分配释放与解析

`TTcsInstancePool<TInstanceType, TTagType>` MUST 以稠密数组 + FreeList 复用 + 代际数组实现 `Allocate/Free/Resolve/IsValid/ForEach/Reset`：Free 使该槽代际 +1（旧句柄凭代际失配判悬空）；`Resolve` 对悬空/无效句柄 ensure（Development 编译期剔除后 Shipping 返回 nullptr）；`ForEach` 按槽位 Index 升序稳定遍历且跳过空闲槽（代际奇偶簿记：奇 = 已分配、偶 = 空闲）；Allocate/Free/Resolve/ForEach/**Reset** 进入点 MUST 断言游戏线程（D0-4，ensure(IsInGameThread())）；Free 不清零槽位内容——持有外部资源的实例由调用方在 Free 前自行清理。

`Reset`（宿主子系统 `Deinitialize` 的确定性清空口，与 `FTcsExpiryHeap::Reset` 同款纪律，2026-09-18 补）：按"当前已分配槽位数"**一次性回落池占用统计**（不逐槽 Free）后清空三数组——全部既有句柄凭代际失配失效；与 Free 同款零策略：**持有外部资源的实例由调用方在 Reset 前自行清理**（池不代为释放）。

#### Scenario: 悬空句柄解析

- **WHEN** Allocate → Free → 用旧句柄调用 `Resolve`（Development）
- **THEN** ensure 命中且返回 nullptr；池内其他槽位的句柄解析不受影响

#### Scenario: 槽位复用

- **WHEN** Allocate → Free → 再 Allocate
- **THEN** 新句柄复用同一 Index 且 Generation 较旧句柄递增（两句柄互异）

#### Scenario: 稳定序遍历跳过空闲槽

- **WHEN** 分配 A、B 后 Free A，执行 `ForEach`
- **THEN** 仅 B 被访问一次（Index 升序、无空洞）

#### Scenario: 重置后旧句柄全部失效且统计回落

- **WHEN** 分配若干槽位后调用 `Reset`，再用旧句柄调用 `IsValid` / `Resolve`，并读取池占用统计
- **THEN** 全部旧句柄判为无效（`IsValid` false、`Resolve` 返回 nullptr 且 hit ensure），池占用统计回落至重置前的基线（`Tcs.Core.PoolSlots` 不残留上一生命周期的槽位）

### Requirement: 池占用统计

TcsCore MUST 提供统计口 `FTcsCorePoolStats`：`AddSlotCount()` / `RemoveSlotCount()` 为**无参对**（池槽位操作粒度恒为 1——正负号与数量歧义在签名上杜绝，2026-09-15 用户拍板拆分），`GetSlotCount()` 与控制台变量 `Tcs.Core.PoolSlots` 同源（跨池当前占用槽位总数，只读参考值）；FreeList 复用路径不改变计数；**`Reset` 按重置时的已分配槽位数一次性调用 `RemoveSlotCount()`**（统计不得因重置而残留）；计数仅游戏线程读写（D0-4）。

#### Scenario: 统计随分配与回收变化

- **WHEN** 新建稠密槽两次、回收一次（FreeList 复用一次不计）
- **THEN** `Tcs.Core.PoolSlots` 读数净 +1

#### Scenario: 重置使统计回落

- **WHEN** 某池当前占用 N 个槽位时调用 `Reset`
- **THEN** `Tcs.Core.PoolSlots` 读数净 −N（与逐槽 Free 的净效果一致）
