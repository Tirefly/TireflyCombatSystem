# clock-expiry-heap Specification

## Purpose
TBD - created by archiving change add-tcscore-clock-expiry-heap. Update Purpose after archive.
## Requirements
### Requirement: 唯一取时入口与可注入时间源

TcsCore MUST 提供 `FTcsClock`（唯一"取时间"入口：`Frame` 帧号 uint64、`Elapsed` 累计 ScaledDt、`DeltaSeconds` 本帧步长；由门面 const 暴露，时间只经泵推进、封装内无旁路取时）与可注入时间源接口 `ITcsTimeSource`（抽象 C++ 接口，非反射——时间源是引擎管道设施而非 Def 配置数据）：`GetDeltaSeconds(World, RawDeltaSeconds)`；默认实现 = `RawDeltaSeconds × World.GetTimeDilation()`（slomo 减速语义）。确定性纪律（D0-1）落地：时钟封装内禁 wall-clock API（`FDateTime::UtcNow` / `FPlatformTime`——review 检查点）；回合制宿主可注入"回合即时间"源替换默认源。

#### Scenario: slomo 按比例减速

- **WHEN** `slomo 0.1` 后逐帧推进
- **THEN** 默认源产出步长为原始帧增量的约 1/10，`Elapsed` 累计增长同步减速

#### Scenario: 暂停冻结

- **WHEN** 世界暂停后继续泵
- **THEN** 步长为 0，`Elapsed` / `Frame` 不推进，到期堆无新到期回调

### Requirement: 到期最小堆（惰性取消 + 代际校验）

TcsCore MUST 提供 `FTcsExpiryHeap`：条目住实例池（复用 `TTcsInstancePool`——代际校验防悬空回调），到期时刻（累计 ScaledDt 时轴上的绝对时刻）最小堆排序，帧成本 = 到期项数（无到期时仅看堆顶）。接口：

- `Push(DueTime, OwnerId, OnDue)` → `FTcsTimeEntryHandle`（句柄强类型 phantom tag 防互串；回调签名 `TFunction<void(uint64)>`——堆零领域词汇）；
- `Cancel(Handle)`：惰性取消——即时标记无效，堆内残留条目出队时跳过并回收槽位；
- `AdvanceTo(Now)`：依到期时刻升序弹出全部 `DueTime ≤ Now` 的条目并回调（同到期时刻按入堆序——D0-1 稳定序）；回调完成后句柄失效（一次性）。

到期回调中新入堆的条目（无论是否已到期）归下一次 `AdvanceTo`（与总线冲洗换出同纪律——防自激、帧成本有界）。堆深度 MUST 有统计 CVar（`Tcs.Core.ExpiryHeapDepth`，实时反映有效条目数）。

#### Scenario: 到期按稳定序回调

- **WHEN** 依序入堆三笔（到期时刻交错、含一笔同刻）
- **THEN** `AdvanceTo` 按到期时刻升序回调，同刻按入堆序

#### Scenario: 取消后不回调

- **WHEN** `Cancel` 一笔未到期条目后推进越过其到期时刻
- **THEN** 该条目不回调，槽位回收且旧句柄代际校验失败不误触

#### Scenario: 回调内入堆不本轮递归

- **WHEN** 到期回调内 Push 一笔已到期条目
- **THEN** 该条目留待下一次 `AdvanceTo`（下一拍）回调，本轮不递归

### Requirement: 时钟泵子系统与固定泵序

`UTcsClockSubsystem`（世界级门面，仅 Game / PIE / GamePreview 世界实例化）MUST 在帧界泵点按固定泵序驱动四步：①时间源取步长（暂停帧步长 0）→ ②`FTcsClock` 推进（Frame+1、Elapsed 累加、DeltaSeconds 记录）→ ③事件总线帧末队列冲洗 → ④到期堆 `AdvanceTo(Elapsed)`。

泵点 MUST 早于全部 Actor tick 组（PrePhysics 语义）：引擎事实（2026-09-16 源码核实）Tickable 自 tick 位于 `UWorld::Tick` 尾部（晚于全部 tick 组与 TimerManager），故泵由 `FWorldDelegates::OnWorldTickStart`（`UWorld::Tick` 头部广播）驱动；子系统按文档口径采用 `UTickableWorldSubsystem` 壳类型但自 tick 全程停用（`GetTickableTickType` = Never）。子系统 MUST 转发到期堆的消费者入口——`PushExpiry` / `CancelExpiry`（对齐总线门面"转发而非暴露内核"模式）；`AdvanceTo` 保持泵私有（时间推进唯一驱动）。到期回调 MUST 打 `LogTcsCore` Log 级日志（含到期时刻——检查点 7 观测面）。`Deinitialize` MUST 解绑委托并确定性清空到期堆。不建时钟订阅者注册表（M3/M4/M5 消费经到期堆 Push——零消费者不预建）。

#### Scenario: 帧末事件在下一帧游戏逻辑前到达

- **WHEN** 帧 N 游戏逻辑期间发布帧末通道事件
- **THEN** 该事件在帧 N+1 泵点（该帧 Actor tick 前）派发，帧 N 末尾不派发

#### Scenario: 泵序固定可观测

- **WHEN** 同一泵点内既有待冲洗帧末事件又有到期条目
- **THEN** 按时间源 → 时钟推进 → 总线冲洗 → 到期堆的固定顺序执行（到期回调中发布的帧末事件归下一拍冲洗）

