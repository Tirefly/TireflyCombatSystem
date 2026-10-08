## MODIFIED Requirements

### Requirement: 实例池的分配释放与解析

`TTcsInstancePool<TInstanceType, TTagType>` MUST 以稠密数组 + FreeList 复用 + 代际数组实现 `Allocate/Free/Resolve/IsValid/ForEach/Reset`：Free 使该槽代际 +1（旧句柄凭代际失配判悬空）；`Resolve` 对悬空/无效句柄 ensure（Development 编译期剔除后 Shipping 返回 nullptr）；`ForEach` 按槽位 Index 升序稳定遍历且跳过空闲槽（代际奇偶簿记：奇 = 已分配、偶 = 空闲）；Allocate/Free/Resolve/ForEach/**Reset** 进入点 MUST 断言游戏线程（D0-4，ensure(IsInGameThread())）；Free 不清零槽位内容——持有外部资源的实例由调用方在 Free 前自行清理。

`Reset`（宿主子系统 `Deinitialize` 的确定性清空口，与 `FTcsExpiryHeap::Reset` 同款纪律，2026-09-18 补）：按"当前已分配槽位数"**一次性回落池占用统计**（不逐槽 Free）后清空三数组——全部既有句柄凭代际失配失效；与 Free 同款零策略：**持有外部资源的实例由调用方在 Reset 前自行清理**（池不代为释放）。

**GC 专用只读遍历 `ForEachForGC`（2026-10-08 新增，MUST）**：池 MUST 另提供 `ForEachForGC(TFunctionRef<void(const TInstanceType&)>)`，供**宿主门面的 `AddReferencedObjects`** 补引用时使用：

- **为什么必需**：GC 的引用收集**不在游戏线程上**，而本需求规定了池的每个正常进入点都断言游戏线程 ⇒ 在 ARO 里调 `ForEach` / `Resolve` 会**每次 GC 触发一次 `ensure(IsInGameThread())`**（实测 callstack：`FRealtimeGC::CollectReferencesForGC` → `AddReferencedObjects` → `TTcsInstancePool::ForEach`）。**该症状极具隐蔽性**：`ensure` 每站点每进程只报一次 ⇒ 表现为"偶尔一条红字"，且**与池空不空无关**；
- **`ForEachForGC` MUST NOT 断言游戏线程**（这正是它存在的理由），也 MUST NOT 被任何常规逻辑消费——**它的唯一合法调用者是 ARO**；拿它做业务遍历等于静默放弃单线程纪律；
- **判据 MUST 与 `ForEach` 逐字相同**（按槽位 Index 升序、按代际奇偶跳过空闲槽）——MUST NOT 引入第二套"哪些槽在册"的判定；
- **MUST 纯读**：MUST NOT 在遍历中改任何状态（GC 期改状态是另一类错误）。

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

#### Scenario: GC 遍历不触发线程断言

- **WHEN** 在**非游戏线程**上下文（GC 引用收集）对池调用 `ForEachForGC`，池内既有在册槽也有空闲槽
- **THEN** **不产生任何 ensure**，且回调恰好收到全部在册槽各一次（与 `ForEach` 的可见集逐项一致）；作为对照，同上下文调用 `ForEach` **MUST** 触发 `ensure(IsInGameThread())`——两条读数共同证明"专用口"确有必要且判据一致
