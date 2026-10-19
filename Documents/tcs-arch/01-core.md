# ARCH-01-core · TcsCore（M0 内核）

- **文档 ID**：`ARCH-01-core`
- **类型**：模块档案（现状 + 债务 + 未实现）
- **状态**：生效中
- **权威范围**：`TcsCore` **今天实际落地了什么**、对外契约的真实面、以及"该落而没落 / 落了但偏了"的账。**不承载**决策理由（见旧 `DEC-*`）与可执行需求原文（见 `openspec/specs/`）。
- **最后更新**：2026-10-17（首次审核）

---

## §1 模块定位与边界

**一句话**：为战斗系统提供**零战斗语义**的运行时底座——实例池与句柄寻址、事件总线、时钟泵与到期堆、数值载体、诊断。**本模块不知道"战斗"是什么。**

| 项 | 内容 |
|---|---|
| 代码规模 | **32 文件 / 2 599 行**（23 `.h` + 9 `.cpp`） |
| 编译期依赖 | **无**（仅 Engine：Core / CoreUObject / GameplayTags） |
| 被谁依赖 | 全部 10 个其它模块 |
| 对应可执行需求 | `openspec/specs/` 的 **`instance-handle-pool`**、**`event-bus`**、**`clock-expiry-heap`**、**`param-value`**（主）；另被 `cpp-module-structure`、`plugin-descriptor` 部分覆盖 |
| 旧模块规格 | `combat-system-design/spec/01-module-m0-core.md`（**`SPEC-00-core`，已冻结**） |

---

## §2 已落地

七组设施，全部【实测】。

### 2.1 实例句柄与池

| 设施 | 代码锚点 | 落地点 |
|---|---|---|
| `TTcsInstanceHandle<TTagType>`：`uint32 Index + uint32 Generation`，`InvalidIndex`、`IsValid()`、按 (Index, Generation) 全等比较；`TTagType` 为**编译期幻影标签**（空结构、零运行期开销） | `Source/TcsCore/Public/Handle/TcsInstanceHandle.h:15` | 跨池误用在编译期拦截 |
| `TTcsInstancePool<TInstanceType, TTagType, TGenerationIssuer>`：稠密数组 + **空闲槽位栈 `TArray<uint32> FreeList`** + 每槽代际（**奇 = 已分配，偶 = 空闲**） | `Public/Pool/TcsInstancePool.h:49`（存储见 `:329-341`） | 全插件唯一实例寻址机制 |
| 池 API 面：`Allocate` / `Free` / `Resolve`（可变 + 常量双口）/ `IsValid` / `IsSlotAllocated` / `MakeHandleForSlot` / `NumAllocated` / `ReleaseSlotByIndex` / `ResetSlotContent` / `ForEach`×2 / `ForEachForGC` / `Reset` | `Public/Pool/TcsInstancePool.h:57-341` | — |
| **代际发号可注入**：模板参 `TGenerationIssuer`，默认 `FTcsPoolLocalGenerationIssuer`（池内自增） | `Public/Pool/TcsInstancePool.h:26, 49` | per-entity 桶注入**进程级**发号器——否则跨桶扫描会命中错实体（本仓 2026-10-05 实测缺陷） |
| 单游戏线程纪律：`ensure(IsInGameThread())` 覆盖全部写入口与业务读口 | 命中 **7 处**（`:59, :89, :106, :201, :249, :271, :317`） | D0-4 |

**一处刻意的双口设计【实测】**：`ForEach`（只读）**断言**游戏线程，`ForEachForGC`（`:299`）**不断言**且唯一合法调用者是 ARO（GC 引用收集跑在非游戏线程）。代码注释明写"业务侧要只读遍历 MUST NOT 借用 `ForEachForGC` 绕开线程断言"。

### 2.2 身份发号（两类，都不是池）

| 设施 | 形态 | 代码锚点 |
|---|---|---|
| `FTcsCombatEntityHandle` | **单 `int64 Id`（0 = 无效），无代际段** | `Public/Handle/TcsCombatEntityHandle.h:29` |
| `FTcsCombatEntityHandleRegistry::Allocate()` | `std::atomic<int64>` 递增，**首分配为 1、永不复用**；只发号不做注销登记 | `Public/Handle/TcsCombatEntityHandle.h:64` |
| `FTcsSourceHandle` | **单 `uint64 Id`（0 = 无效）** | `Public/Handle/TcsSourceHandle.h:20` |
| `FTcsSourceHandleRegistry::Allocate()` | 同上语义，但**出线**：定义住 `.cpp` 的匿名 namespace | `Public/Handle/TcsSourceHandle.h:55` + `Private/Handle/TcsSourceHandle.cpp` |

**为什么来源发号要出线【实测】**：把计数器留成头文件内联变量，MSVC 的 COMDAT 折叠**不跨 DLL 合并** ⇒ 每个镜像各留一份计数器、各自从 1 开始（台账 `WAIT-6` 的实测症状）；而 monolithic Shipping 又会合并成一份 ⇒ **症状随构建配置漂移**。出线后全进程只此一份。

### 2.3 事件总线

| 设施 | 代码锚点 | 要点 |
|---|---|---|
| `FTcsEventBus` | `Public/EventBus/TcsEventBus.h:92` | 内核，纯逻辑结构体 |
| 订阅 / 退订 | `:106` / `:141` | `TMultiMap<FGameplayTag, FTcsEventSubscriptionHandle> TagIndex` + `TTcsInstancePool` 订阅记录池；**句柄配对清理**，悬空句柄 `ensure` 并忽略 |
| 双通道 | `PublishImmediate`(`:177`) / `PublishFrameEnd`(`:189`) | 立即 = 当场同步派发、载荷 const 引用透传零复制；帧末 = 入队持副本 |
| 帧末冲洗 | `FlushFrameEnd`(`:207`) | **先换出队列再逐条派发** ⇒ 冲洗期间新入队归下一拍（防自激），与到期堆同纪律 |
| 僵尸订阅惰性摘除 | `DispatchToChannel`(`:264`) | Handler 弱引用失效时当场 `Unsubscribe` |
| `UTcsEventHandler` | `Public/EventBus/TcsEventHandler.h:21` | `Abstract` + `Blueprintable`；`HandleEvent` 为 BlueprintNativeEvent，C++ 覆写 `_Implementation`；**EventTag 随载荷一并传入**（同一 Handler 可订多 Tag 而不失真） |
| `UTcsEventBusSubsystem` | `Public/EventBus/TcsEventBusSubsystem.h:35` | 世界级门面，转发而非暴露内核 |
| BP/CS 动态监听面 | `Public/EventBus/TcsAsyncAction_ListenForCombatEvent.h:32` + `ETcsEventMatchType`(`:18`) | Tag 过滤 + 载荷类型匹配 + 精确/部分匹配，走 `MatchesFilter`(`:87`) |
| **全量事件流观察者** | `EventObserver` 成员 `TFunction<void(FGameplayTag, const FInstancedStruct&)>`（`:239`） | 与订阅表无关地喂**每一条**事件；由 `TcsEventBusSubsystem.cpp:10` 挂上 |

### 2.4 时钟泵与到期堆

| 设施 | 代码锚点 | 要点 |
|---|---|---|
| `FTcsClock` | `Public/Clock/TcsClock.h:12` | 唯一取时入口：`Frame`（uint64）+ `Elapsed`（累计 ScaledDt）+ `DeltaSeconds` |
| `ITcsTimeSource` + `FTcsTimeSource_Default` | `Public/Clock/TcsTimeSource.h:17, :38` | 可注入；默认 = `RawDeltaSeconds × World.GetTimeDilation()`（slomo 减速） |
| `UTcsClockSubsystem` | `Public/Clock/TcsClockSubsystem.h:27` | 世界级泵；`GetTickableTickType()` = **`Never`**（`:52`）——泵由 `FWorldDelegates::OnWorldTickStart`（`HandleWorldTickStart`，`:107`）驱动，**早于全部 Actor tick 组** |
| 门面四口 | `SetTimeSource`(`:72`) / `PushExpiry`(`:88`) / `CancelExpiry`(`:94`) / `DoesSupportWorldType`(`:36`) | `AdvanceTo` 保持泵私有（时间推进唯一驱动） |
| `FTcsExpiryHeap` | `Public/Clock/TcsExpiryHeap.h:60` | 最小堆；`Push`(`:67`) / `Cancel`(`:88`) / `AdvanceTo`(`:106`) / `Reset`(`:130`) |
| **惰性取消 + 代际校验** | `Cancel`(`:88`) | `Cancel` **即刻回收槽位**（代际 +1），堆内残留条目出队时凭**代际失配跳过** ⇒ 不做堆内定位删除 |
| 稳定序 | `Sequence` 字段（`:48`） | 到期时刻升序、**同刻按入堆序**回调（D0-1 确定性） |
| 回调内入堆纪律 | `AdvanceTo`(`:106`) | **先换出当前到期集再逐项回调** ⇒ 回调内新入堆（哪怕已到期）归下一次 |
| 帧成本 | `AdvanceTo` 首行 | 无到期时**仅看堆顶 O(1)**，故帧成本 = 到期项数 |
| 单游戏线程 | `ensure(IsInGameThread())` 覆盖 `Push`/`Cancel`/`AdvanceTo`/`Reset` | `:69, :90, :108, :132` |

### 2.5 数值载体族（PV 系列）

| 设施 | 代码锚点 | 要点 |
|---|---|---|
| `FTcsParamValue`：`FInstancedStruct Source` + `Evaluate(Ctx)` 便利转发 | `Public/Parameter/TcsParamValue.h:35, :63` | `Source.IsValid()` 兜底 0 |
| `FTcsParamValueSource`（抽象基类） | `Public/Parameter/TcsParamValueSource.h:85` | `USTRUCT(meta=(Hidden))` + **默认体**（不是纯虚——UHT 无条件生成 TCppStructOps 需可默认构造）；`Evaluate`(`:109`) / `AllowsValueConvention`(`:131`) |
| `FTcsParamEvaluateContext` | `Public/Parameter/TcsParamValueSource.h:21` | 上下文（含 `Subject` / `EffectiveLevel` / 参数表只读） |
| 内置源 | `FTcsParamSource_Literal`（`TcsParamSource_Literal.h:18`）、`FTcsParamSource_ParamRef`（`TcsParamSource_ParamRef.h:18`） | ParamRef 支持嵌套 + Fallback |
| `FTcsParamEnumerableSource` | `Public/Parameter/TcsParamEnumerableSource.h:27` | 只承担「等级 → 档位下标」`GetIndexForLevel`(`:41`) |
| 宿主脚本插槽 | `ITcsParamSourceHost`（`TcsParamSourceHost.h:38`）+ `FTcsParamSource_HostDelegate`(`:88`) | `UINTERFACE + BlueprintNativeEvent` ⇒ 任意脚本语言可实现 |
| 参数表读口 | `ITcsParamTableReader`（`TcsParamTableReader.h:23`） | `TryGetNumericParam`(`:39`)，**形参不带 const**（UHT thunk 签名硬规则） |

### 2.6 诊断

| 设施 | 代码锚点 | 要点 |
|---|---|---|
| 独立日志通道 | `Public/TcsCoreLogChannel.h` + `Private/TcsCoreLogChannel.cpp` | `LogTcsCore`；**声明独立成通道文件**，使用方只 include LogChannel 头、不 include `Module.h` |
| 池占用 CVar | `FTcsCorePoolStats`（`Public/TcsCoreStats.h:12`）+ `Tcs.Core.PoolSlots` | `AddSlotCount`/`RemoveSlotCount`/**无参对**（粒度恒 1，签名上杜绝正负号歧义）；只在**新建稠密槽**时 +1，FreeList 复用不计 |
| 到期堆深度 CVar | `FTcsCoreHeapStats`（`TcsCoreStats.h:29`）+ `Tcs.Core.ExpiryHeapDepth` | 同款无参对 |
| CVar 注册 | `Private/TcsCoreStats.cpp:11, :35` | `FAutoConsoleVariableRef` |

### 2.7 开发者设置

`UTcsDeveloperSettings`（`Public/TcsDeveloperSettings.h:13`）：`Config = Game, DefaultConfig`，`GetCategoryName()` 返回 **`"Tcs"`**（项目设置里独立分类）。**当前是空壳**——无任何配置项。

---

## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）

| 契约 | 稳定性依据 |
|---|---|
| `TTcsInstanceHandle<TTag>` 的 **(Index, Generation) 全等**语义 | 全部池化设施（订阅、到期条目、状态实例、技能运行态）都建立在它上面 |
| 池的 **奇 = 已分配 / 偶 = 空闲** 代际编码 | `ForEach`/`ForEachForGC`/`Reset` 的跳过判据都读它，外部若另有假设会分叉 |
| 事件总线的**双通道**语义（立即 vs 帧末）与**换出后再派发** | 自激防护的全部依据 |
| `FTcsClock.Elapsed` 是**唯一**时间轴 | 到期堆 `DueTime`、状态时长、冷却全部以它为轴 |
| `FTcsCombatEntityHandle` / `FTcsSourceHandle` 的「**只发号、不注销**」 | 级联撤销靠消费方按来源分组移除，不靠句柄失效 |

### 3.2 MUST NOT 依赖（内部细节）

- **`FreeList` 是栈而非队列**：复用顺序是 LIFO 尾部复用。外部 MUST NOT 依赖"槽位分配序"做任何判定。
- **`EventObserver` 是单函数成员属性**：直接赋值会**覆盖**前一个观察者（当前只有 `UTcsEventBusSubsystem` 在用）。**它不是多播**。
- **`TagIndex` 是 `TMultiMap`**：同 Tag 多订阅的**遍历顺序不保证稳定**（`MultiFind` 的输出序）。需要确定性顺序的消费方 MUST 自己排序。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向

```
TcsCore ──被依赖──► 全部 10 个模块
   ├─ 池/句柄 ─────► TcsState（per-entity 桶）、TcsBuff、TcsSkill（运行态池）、TcsEffect
   ├─ 事件总线 ────► 全部模块（发事件都经它）
   ├─ 时钟/到期堆 ─► TcsState（时长到期）、TcsEffect（定时步骤）、TcsSkill（时段/冷却）
   ├─ ParamValue ──► TcsAttribute、TcsState、TcsEffect、TcsSkill
   └─ 实体/来源句柄 ► TcsIntegration（实体）、TcsAttribute（来源分组）
```

### 4.2 时序位置（最容易踩错的地方）

**泵序是硬契约**：`OnWorldTickStart` → ①时间源取步长 → ②`FTcsClock` 推进 → ③**事件总线帧末队列冲洗** → ④到期堆 `AdvanceTo`。

⇒ **帧末事件的派发早于到期回调**。任何"事件处理里改时长、时长在同帧到期"的场景都会观察到这个顺序。

### 4.3 四条跨模块影响细节【实测】

1. **来源发号出线是跨模块问题，不只是本模块实现细节**：`TcsDev` 等镜像也会调 `Allocate()`。若把计数器留在头里，**不同 DLL 的模块会拿到重复的来源 Id**，级联撤销会误摘别人的条目。这是 `WAIT-6` 的实测症状。
2. **代际发号器必须注入**：`TTcsInstancePool` 默认「每池自增」⇒ per-entity 桶直接用会**各从代际 1 起发**，而桶是 per-entity 的、句柄不带实体段 ⇒ 两个实体的首实例得到相同 `(Index, Generation)` ⇒ 跨桶扫描式解析**命中错实体**（`FTcsBuffOps::Find` 即此类）。`TcsState` 因此必须注入进程级发号器。
3. **`ForEachForGC` 不能被业务借用**：ARO 在非游戏线程跑，若业务侧也调它绕开线程断言，就失去了 D0-4 的整个保护面。
4. **`LogTcsCore` 的 Verbose 日志量**：总线订阅/退订/发布每次一条 `Verbose`。帧末通道逐条打 ⇒ 高事件率场景下开销可观，但因为是 Verbose 档、默认关闭，**可接受**。

---

## §5 技术债

> 判据：**需求说了 / 设计定了，代码存在但不达标或做偏了**。代码根本不存在的一律归 §6。

### D-1 旧模块规格的类型名**整体滞后**一个改名族【实测】

| 项 | 内容 |
|---|---|
| **期望** | 旧 `SPEC-00-core` 是模块规格，读者按它写的代码应能编译 |
| **实际** | 旧规格用的全是 `Combat*` 一族，代码已全量 `Tcs*`。**旧名在 `Source/` 内命中 0 个文件** |
| **偏差证据** | `UCombatClockSubsystem`/`FCombatEventBus`/`FCombatEntityHandle`/`TCombatInstanceHandle`/`TInstancePool`/`FExpiryHeap`/`FCombatClock`/`ITimeSource`/`UCombatEventBusSubsystem` → 代码命中全部 **0**；对应新名命中 20/3/99/15/19/9/6/3/25 个文件 |
| **影响面** | 读者按旧规格写代码**编译不过**；旧规格无法作为"当前契约"使用 |
| **是否已登记** | 本档案 §2 已用新名重写；旧文档族整体冻结（见 `ARCH-INDEX` §1）。**旧规格正文未改，属有意冻结** |

**★ 这一条不是"修文档"就能完的**：它证明旧文档族的**整体可信度**已不可按篇判断——必须逐篇回到代码核对，这正是本档案存在的理由。

### D-2 旧规格对实体句柄形态的描述与代码不符【实测】

| 项 | 内容 |
|---|---|
| **期望** | 旧 `SPEC-00-core` 把实体句柄与实例句柄并列在「句柄与池」小节，形态上像同族的两段句柄 |
| **实际** | `FTcsCombatEntityHandle` 是**单 `int64 Id`、无代际段**；与 `TTcsInstanceHandle`（Index+Generation）**不是同一机制** |
| **偏差证据** | `TcsCombatEntityHandle.h:29-52` 只有 `int64 Id` 一个字段 |
| **正确定性** | `openspec` 的 `instance-handle-pool` Purpose 写的是「…以及**不带代际段**的战斗实体身份句柄」——**openspec 是准的**，旧规格是模糊的 |
| **影响面** | 读者可能以为实体句柄可代际校验（**不能**）。实体身份永不复用是**有意语义**（实体就是实体，不是可回收槽位） |

### D-3 两个统计 CVar 的「将来时」措辞已过期【实测】

| 项 | 内容 |
|---|---|
| **期望** | 旧规格 §2.4 只写"统计 CVar（池占用/堆深度，随对应设施落地）"——**将来时** |
| **实际** | **两个都已在**：`Tcs.Core.PoolSlots` + `Tcs.Core.ExpiryHeapDepth`，且实现方式（无参对、零原子）比规格描述更精细 |
| **影响面** | 无功能影响，属**文档滞后**；但"将来时"会让读者以为还没做 |

### D-4 参数源的「可枚举能力」只兑现一半，且**代码注释比规格诚实**【实测】

| 项 | 内容 |
|---|---|
| **期望** | PV-10 原案的 `FTcsParamEnumerableSource` 有**两个**入口：`GetIndexForLevel` **和** `Enumerate(Level, OutValues, OutCurrentIndex)`（含"表项总数"派生形态） |
| **实际** | **只有 `GetIndexForLevel`**。`Enumerate` 与"表项总数"**均未建** |
| **偏差证据** | `TcsParamEnumerableSource.h` 类注释自陈："PV-10 原案的另一半 `Enumerate` 与其派生形态『表项总数』**均未建**——消费者是展示层的 Series 视图（`TcsNotation`，R8），按『零消费者不预建』留白" |
| **影响面** | 零（无消费者）；`TcsNotation` 的 Series 视图落地时需补 |
| **是否已登记** | 代码注释已登记；本档案 §6 U-4 记入 |

**这条的判据价值**：它示范了"**有意留白**"与"**技术债**"的界线——**有意留白且写明理由的不是债**，只是未实现。

---

## §6 未实现

> 判据：**代码根本不存在**。每条注明「设计是否还成立」。

### U-1 轻量 facade `FCombatCore::Get(World)`【实测：不存在】

| 项 | 内容 |
|---|---|
| 旧规格声称 | `SPEC-00-core` §3：「`FCombatCore::Get(World)`：轻量 facade，一次解析双子系统指针」 |
| 实际 | `CombatCore` 在 `Source/TcsCore/` **零命中** |
| **设计是否还成立** | **存疑**。当前消费方直接 `GetWorld()->GetSubsystem<...>()` 已自足；facade 的唯一收益是"少写一次解析"，而它会引入**第二个门面层**（与既有两个子系统门面并立）。按「零消费者不预建」判据，**在没有真实痛点前不建议补** |
| 前置条件 | 若将来出现"同时要两个子系统且解析开销被实测到"的场景再议 |

### U-2 原生侧层级匹配订阅【实测：未做】

| 项 | 内容 |
|---|---|
| 旧规格声称 | §2.2：「'原生侧层级匹配'…升为**采用三层命名的配套前置**」——即"父标签订阅收全子树"未落地前，原生消费者要全域订阅只能按叶子逐条订 |
| 实际 | `DispatchToChannel` 用 `TagIndex.MultiFind(Tag, Targets)` ⇒ **精确匹配**。BP/CS 动态层支持**部分匹配**（`ETcsEventMatchType`） |
| 现状影响 | C++ 侧订 `TcsEvent.Attribute` **收不到**子域事件——只能逐叶子订。旧规格自己说"本域事件数少，可接受" |
| **设计是否还成立** | **成立，且仍是缺口**。但它是**结构选型**问题（`TMultiMap` vs 按 Tag 分桶 vs 层级树缓存），不是纯功能缺失 |
| 附带判断 | 旧规格还说"未落地不等于命名要降级（改名成本 > 等落地）"——这个判断**今天仍成立** |

### U-3 网络镜像模式接口位【实测：不存在】

| 项 | 内容 |
|---|---|
| 旧规格声称 | §4：「池与总线**预留镜像模式接口位**：镜像侧的 Allocate/写入只接受复制操作流重放；本期只留接口不实现」 |
| 实际 | `Source/TcsCore/` 内"镜像/Mirror/Replication"的命中**全是无关注释**（讲 COMDAT 折叠时说的"镜像"指 DLL 镜像，**不是网络镜像**） |
| **设计是否还成立** | **需重新核对，倾向撤销**。这条的前提是"将来要做网络同步"；但本仓至今无任何网络代码，且 `FInstancedStruct` 载荷的网络面旧规格自己写着"未来讨论"。⇒ 按**真·零消费者**处理：不预留接口位，等网络需求真实出现再设计（**预留一个没有消费者的接口位 = 预建**） |
| 前置条件 | 网络同步需求立项 |

### U-4 `FTcsParamEnumerableSource::Enumerate` 与"表项总数"【实测：有意留白】

见 §5 D-4。消费者 = `TcsNotation` 的 Series 视图（R8）。**有意留白，不是遗漏。**

### U-5 其它"未来位"（旧规格明写、代码无）

| 项 | 旧规格原话 | 定性 |
|---|---|---|
| `showdebug` 面板 | "showdebug 面板 = 未来接口位（届时日志路由随面板回归）" | **有意留白**，归 M8 |
| K2 强类型引脚节点 | "K2 强类型引脚节点 = 未来项（M8 轮）" | **有意留白**，归 M8 |
| 类型化委托（per-event 强类型 delegate） | "类型化委托 = 未来增量（哪个事件用得痛再单独加）" | **有意留白**，按需 |
| 时钟订阅者注册表 | 「不建时钟订阅者注册表（M3/M4/M5 消费经到期堆 Push——零消费者不预建）」 | **有意留白**，且已写进 `openspec` 需求正文 |

---

## §7 文档资产对照

| 资产 | 位置 | 与本模块的关系 | 时效判定 |
|---|---|---|---|
| `SPEC-00-core` | `combat-system-design/spec/01-module-m0-core.md` | 旧模块规格 | **❌ 类型名整体滞后**（D-1）、**❌ facade 声称未落地**（U-1）、**❌ 镜像接口位未落地**（U-3） |
| `instance-handle-pool` | `openspec/specs/` | 可执行需求（池/句柄/来源） | ✅ **与代码一致**，且比旧规格准确（明写"不带代际段"） |
| `event-bus` | `openspec/specs/` | 可执行需求（总线） | ✅ 与代码一致（新名） |
| `clock-expiry-heap` | `openspec/specs/` | 可执行需求（时钟/到期堆） | ✅ 与代码一致；且**记载了 `GetTickableTickType = Never` 这个关键实现细节**，旧规格未提 |
| `param-value` | `openspec/specs/` | 可执行需求（数值载体） | ✅ 与代码一致 |
| `entity-runtime-mechanism` | `openspec/specs/` | **部分**——池的 `ForEachForGC` 与**代际发号可注入**写在这里，**不在** `instance-handle-pool` | ⚠️ **同一设施的规格被拆到两个能力**，检索时必须两处都搜 |
| `DEC-00-constitution` | `combat-system-design/` | 模块物化规定（§9） | 历史理由，**是 `M` 编号的权威表** |

---

## §8 审核记录

**方法**：①读 `openspec` 三个主能力 → ②提取全部公开符号建 API 面 → ③与旧 `SPEC-00-core` 逐项对照 → ④反查跨模块使用点 → ⑤标注置信度。

| 核对项 | 结果 | 置信度 |
|---|---|---|
| 旧规格 9 个 `Combat*` 类型名 → 代码实际名 | **全部 0 命中，已改名** | 【实测】 |
| `FCombatCore::Get(World)` facade | **不存在** | 【实测】 |
| 原生侧层级匹配 | **未做**（`MultiFind` 精确） | 【实测】 |
| 网络镜像接口位 | **不存在** | 【实测】 |
| 池 freelist | **存在**（`TArray<uint32> FreeList`，LIFO 尾部复用） | 【实测】 |
| 池单游戏线程断言 | **7 处 `ensure(IsInGameThread())`** | 【实测】 |
| 代际发号可注入 | **存在**（模板参 `TGenerationIssuer`） | 【实测】 |
| 到期堆惰性取消 | **Cancel 即刻回收槽位 + 代际失配跳过** | 【实测】 |
| 时钟泵 tick 类型 | **`Never`**（由 `OnWorldTickStart` 驱动） | 【实测】 |
| 两个统计 CVar | **都已存在** | 【实测】 |
| `EventObserver` | **代码有、两份文档都没写** | 【实测】 |
| `ForEachForGC` / 代际发号器 | **`openspec` 写了（在 `entity-runtime-mechanism`）、旧规格没写** | 【实测】 |

**未覆盖项（如实声明）**：
- 未逐条执行 `openspec` 的 `#### Scenario:` 做行为级验证。本模块无专属 PIE 证据档，行为面置信度**低于** API 面。
- 未审 `UTcsAsyncAction_ListenForCombatEvent` 的 100 行实现体，只核了接口面与过滤函数签名。

---

## §9 结论

**`TcsCore` 的代码质量属"干净"一档**：单游戏线程纪律贯穿全部入口、代际语义一致、两个"换出后再遍历"纪律（总线冲洗 / 到期堆推进）同款落地、来源发号出线的跨 DLL 问题有明确记载。

**它的债几乎全是文档债，不是代码债**：
- 唯一的**真实设计缺失**是 U-2（原生层级匹配），且它是结构选型而非功能漏洞。
- U-1 / U-3 是**旧规格开了空头**（声称提供、实际没有）。建议按"零消费者不预建"**正式撤销**这两条设计，而不是补实现。
- D-1 是**旧文档族不可按篇信任**的实证——一个模块就足以证明"重新梳理"是必要的。

**给下一个模块（`TcsNotation`）的三个关注点**：
1. 它只有 92 行，但旧规格 `SPEC-07-notation` 标 `PENDING` 却有 19 299 字节 —— 判**实际落地度与规格的落差**。
2. `FTcsParamEnumerableSource` 的消费者 Series 视图声称住 `TcsNotation`（R8）—— 核实这个计划是否仍在。
3. 它被 `TcsAttribute` / `TcsState` / `TcsBuff` / `TcsSkill` 依赖，却只有 3 `.h` + 2 `.cpp` —— 依赖面与体量不成比例，**可能是极薄的契约层**。
