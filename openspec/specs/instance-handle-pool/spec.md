# instance-handle-pool Specification

## Purpose
定义 TCS 的实例身份词汇：强类型「索引 + 代际」句柄、归属来源标识与发号、池的分配/释放/解析，以及不带代际段的战斗实体身份句柄——全插件的实例寻址契约由此一处定义。

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

`TTcsInstancePool<TInstanceType, TTagType, TGenerationIssuer = FTcsPoolLocalGenerationIssuer>` MUST 以稠密数组 + FreeList 复用 + 代际数组实现 `Allocate/Free/Resolve/IsValid/ForEach/Reset`：Free 使该槽代际 +1（旧句柄凭代际失配判悬空）；`Resolve` 对悬空/无效句柄 ensure（Development 编译期剔除后 Shipping 返回 nullptr）；`ForEach` 按槽位 Index 升序稳定遍历且跳过空闲槽（代际奇偶簿记：奇 = 已分配、偶 = 空闲）；Allocate/Free/Resolve/ForEach/**Reset** 进入点 MUST 断言游戏线程（D0-4，ensure(IsInGameThread())）；Free 不清零槽位内容——持有外部资源的实例由调用方在 Free 前自行清理。

**★ 第三模板参 = 代际发号策略（2026-10-14 新增，MUST 读）**：池 MUST 接受 `TGenerationIssuer` 作为**代际发号策略**，其静态接口为 `uint32 IssueAllocation(uint32 PriorGeneration)`（`PriorGeneration` = 该槽当前代际，**全新槽传 0**），返回值 MUST 为**正奇数**（奇偶簿记依赖它）。**默认档 `FTcsPoolLocalGenerationIssuer` = 池内自增（`PriorGeneration + 1`）= 本参引入前的行为逐字不变** ⇒ 既有四个使用者（到期堆 / 事件总线 / 链运行态 / 施法运行态）MUST NOT 因此产生任何行为差异。

- **为什么需要可注入**：**per-entity 桶**场景下，桶按实体各持一个池；若各池各从代际 1 起发，则两个实体各自的首实例会得到**相同的 `(Index, Generation)`**，而句柄**不带实体段** ⇒ "跨桶扫描式解析"（`FTcsBuffOps::Find` 即此类）会**命中错实体**（本仓 2026-10-05 实测过的缺陷）。⇒ 该场景 MUST 注入**进程级**发号器（见 `entity-runtime-mechanism` 能力的「统一 per-entity 桶与注册表」需求）；
- **发号策略 MUST 无状态**：进程级计数器 MUST **出线**到模块 `.cpp`（匿名 namespace 变量），**MUST NOT** 用头内联 `static` / inline 变量——模块化构建下每个引用它的 DLL 各留一份，于是各模块各自从 1 起发号（症状随构建配置漂移）；
- **分配身份只在 `Allocate` 产生**（经发号策略）；`Free` 的 `+1` **只承担"置偶数"**（旧句柄凭"非奇数"与"数值失配"双重失效），MUST NOT 被当作分配身份。

**`IsValid` 判据 MUST 含奇偶校验（2026-10-14 补）**：`IsValid(Handle)` = `Handle.IsValid()` **且** `Index` 未越界 **且** 代际数值相等 **且** **该槽代际为奇数**。前三项**不足以**证明槽位在册——释放把代际 +1 置偶，而一个**手工拼装**的偶代际句柄（如 `{0, 2}`）会与空闲槽"恰好相等" ⇒ 缺奇偶校验会把空闲槽判成在册、并返回一个已释放（未清零）的实例。对良构句柄（分配产物恒为奇）**行为中性**。

**其余公开口（2026-10-14 补，供 per-entity 桶复用而无需自写槽位逻辑）**：
- `Resolve(Handle) const`——**只读且不 ensure** 的重载（查询面"时序竞态 ⇒ 静默 nullptr"语义需要它；`const` 门面上无法调非常量重载，缺它就只能 `const_cast` 或自行重实现代际判定）；
- `ForEach(Fn) const`——**只读但断言游戏线程**的重载（常规业务遍历用；**MUST NOT** 用 `ForEachForGC` 代替它做业务遍历）；
- `IsSlotAllocated(uint32) const` / `MakeHandleForSlot(uint32) const` / `NumAllocated() const` / `ReleaseSlotByIndex(uint32)` / `ResetSlotContent(uint32)`——按**槽位下标**驱动的口。`ReleaseSlotByIndex` 对已空闲 / 越界下标 MUST **静默返回 false**（重复释放属正常竞态收尾），与 `Free` 对**句柄**口径的 `ensure` 拦截刻意分开；`ResetSlotContent` 只重置内容、不改代际不动 FreeList。

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

#### Scenario: 注入进程级发号器后跨桶身份不撞

- **WHEN** 两个 per-entity 桶各自注入**同一个进程级**发号器，并各自分配桶内第 0 个槽位
- **THEN** 两个句柄的 `Index` **可以相同**，而 `Generation` **MUST 互异**（⇒ "跨桶扫描式解析"不会把 A 实体的句柄命中到 B 实体）；**作为对照**，同一场景改用**默认档**（池内自增）时两者的 `(Index, Generation)` **会相同**——这正是必须注入的原因

#### Scenario: 默认档不改变既有池行为

- **WHEN** 不指定第三模板参（即用默认 `FTcsPoolLocalGenerationIssuer`）做 Allocate → Free → 再 Allocate
- **THEN** 代际读数与本参引入前**逐字相同**（新槽从 1 起、复用 +1）⇒ 既有四个池使用者零行为差异

#### Scenario: 手工拼装的偶代际句柄被判无效

- **WHEN** 对一个**空闲**槽构造一个 `Generation` 等于该槽当前（偶）值的句柄，并调 `IsValid`
- **THEN** 返回 false（奇偶校验拦住它）——**缺该判据时会误判为在册并返回已释放的实例**；而任一 `Allocate` 产物（恒为奇）仍返回 true

### Requirement: 池占用统计

TcsCore MUST 提供统计口 `FTcsCorePoolStats`：`AddSlotCount()` / `RemoveSlotCount()` 为**无参对**（池槽位操作粒度恒为 1——正负号与数量歧义在签名上杜绝，2026-09-15 用户拍板拆分），`GetSlotCount()` 与控制台变量 `Tcs.Core.PoolSlots` 同源（跨池当前占用槽位总数，只读参考值）；FreeList 复用路径不改变计数；**`Reset` 按重置时的已分配槽位数一次性调用 `RemoveSlotCount()`**（统计不得因重置而残留）；计数仅游戏线程读写（D0-4）。

#### Scenario: 统计随分配与回收变化

- **WHEN** 新建稠密槽两次、回收一次（FreeList 复用一次不计）
- **THEN** `Tcs.Core.PoolSlots` 读数净 +1

#### Scenario: 重置使统计回落

- **WHEN** 某池当前占用 N 个槽位时调用 `Reset`
- **THEN** `Tcs.Core.PoolSlots` 读数净 −N（与逐槽 Free 的净效果一致）

### Requirement: 战斗实体身份句柄与发号

TcsCore MUST 提供 `FTcsCombatEntityHandle`（**`USTRUCT(BlueprintType)`**，`int64 Id`，0 = 无效）与 `FTcsCombatEntityHandleRegistry`（`std::atomic<int64>` 递增发号、首个分配为 1、进程内永不复用）——与既有的 `FTcsSourceHandle` / `FTcsSourceHandleRegistry` 同域（Core 的词汇句柄，同住 `Handle/` 目录）。MUST 提供 `IsValid`、相等比较与 `GetTypeHash`（供 `TMap` 键控）。

**类型反射可见性（2026-09-18 升格）**：本句柄是**身份词汇**（token 化，非裸整数），MUST 可出现在 `FInstancedStruct` 载荷与 `UPROPERTY` 字段里——属性变更事件 `FTcsAttributeChangedEvent.Entity`（总线载荷 `FInstancedStruct` 的成员 MUST 为 `USTRUCT()`）与 PV-1 规划的上下文 `Subject`（`UPROPERTY`）是首批消费者。因此：结构体 MUST 带模块导出宏（`USTRUCT()` 类型有 UHT 生成符号 → 按导出宏纪律必须带；纯内联的 `FTcsSourceHandle` 反而不得带——两者差异正是该纪律的两面）；`Id` MUST 为 `int64`（UHT 不支持 `uint64` 作为属性类型；句柄 Id 恒为正，无符号性差异无实际影响）。措辞口径见 `Documents/combat-system-design/ledger/reflection-terminology.md`。

该类型是 **Core"零战斗词汇"边界上的一处明示让步**（PV-1 记录：`FTcsParamEvaluateContext` 等 Core 设施不得反向依赖域模块，故实体身份是全插件通用键、唯一由 Core 持有的战斗实体词汇；02 §2.2a / 06 已按通用实体键使用）。与池化实例句柄的区别：`FTcsCombatEntityHandle` **无代际段**——Id 单调递增且永不复用，悬空表现为"查不到"而非"误命中回收槽"；代际校验只属于 `TTcsInstancePool` 的槽位复用语义。**发号方归属**：R3 由 M2 门面（`UTcsAttributeSubsystem::RegisterEntity`）发号；M6 世界注册表落地后移交发号（句柄类型与消费者签名不变——见 design.md D1）。

#### Scenario: 发号唯一且非零

- **WHEN** 连续调用 `Allocate()` 两次
- **THEN** 返回两个互异且 `IsValid()`（非零）的实体 Id

#### Scenario: 句柄可作 TMap 键

- **WHEN** 以同一句柄两次读写同一 `TMap<FTcsCombatEntityHandle, …>`
- **THEN** 命中同一槽位（相等 + 哈希一致）

#### Scenario: 句柄可进 FInstancedStruct 载荷

- **WHEN** 把 `FTcsCombatEntityHandle` 作为某个 `USTRUCT()` 的 `UPROPERTY` 字段（如属性变更事件的 `Entity`）
- **THEN** UHT 编译通过、载荷可经总线（`FInstancedStruct`）传递，且宿主脚本可读该字段

#### Scenario: 与来源句柄类型隔离

- **WHEN** 把 `FTcsCombatEntityHandle` 传给期望 `FTcsSourceHandle` 的 API（级联撤销）
- **THEN** 编译失败（"被修饰主体"与"归属来源"不可互换——避免同型误传在运行期才暴露）
