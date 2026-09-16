# Change: 落地 TcsCore 时钟与到期堆能力规格（clock-expiry-heap）

## Why
plan1 Task 3（时钟与到期堆）动工前的规格先行提案：`UTcsClockSubsystem` / `FTcsClock` / `ITcsTimeSource` / `FTcsExpiryHeap` 是 M0 内核时钟泵四件套（01 §2.3 / D0-1 / D0-5 / D3-6），承接事件总线帧末通道的冲洗驱动权（plan1 Task 2 既定移交："下一帧到达"语义由时钟泵接管），并为计划二 WaitDelay 与未来 M3 到期 / M4 定时步骤 / M5 冷却提供确定性时间底座。泵点接管同时需要修订 event-bus 能力中"帧末队列随子系统自 tick 冲洗"的过渡口径。

## What Changes
- 新增能力规格 `clock-expiry-heap`，覆盖三组设施：
  - `FTcsClock` 唯一取时入口（Frame / Elapsed 累计 ScaledDt / DeltaSeconds）+ 可注入时间源 `ITcsTimeSource`（默认 `World DeltaSeconds × TimeDilation`；回合制宿主注入"回合即时间"）；确定性纪律落地：封装内禁 wall-clock API（D0-1 review 检查点）。
  - `FTcsExpiryHeap` 到期最小堆：条目池化 + 代际校验（复用 Task 1 池机制）、惰性取消、到期升序稳定回调（同刻按入堆序）、到期回调内新入堆归下一拍；堆深度统计 CVar。
  - `UTcsClockSubsystem` 泵门面：泵点 = `FWorldDelegates::OnWorldTickStart`（`UWorld::Tick` 头部，早于全部 Actor tick 组——PrePhysics 语义达成）；固定泵序：时间源取步长 → 时钟推进 → 总线帧末冲洗 → 到期堆 AdvanceTo；到期回调打 `LogTcsCore`。
- 修订 `event-bus` 能力：帧末队列冲洗驱动权由总线子系统自 tick（`UWorld::Tick` 尾部 `TickObjects`）移交时钟泵；总线子系统自 tick 停用（`SetTickableTickType(ETickableTickType::Never)`）；帧末通道可观测时机由"本帧末"变为"下一帧泵点（该帧游戏逻辑前）"。
- **BREAKING**：帧末通道派发时机变化（本帧末 → 下一帧泵点）——对消费者是行为语义变化；当前零外部消费者（临时测试装置随提案更新），无迁移面。

## Impact
- Affected specs: `clock-expiry-heap`（新建能力）、`event-bus`（MODIFIED：世界级门面子系统——冲洗驱动权与可观测时机）。
- Affected code: 新建 `Source/TcsCore/Public/Clock/`（ITcsTimeSource.h / FTcsClock.h / FTcsExpiryHeap.h，header-only）+ `Private/Clock/UTcsClockSubsystem.cpp`；修改 `TcsCoreStats.h/.cpp`（堆深度统计）；修改 `UTcsEventBusSubsystem.h/.cpp`（停用自 tick、注释口径更新）；临时测试装置（`Private/Testing/`，不入库、不进规格）增补 `Tcs.Test.Clock` 命令并复核 `Tcs.Test.Bus` 到达帧口径。
- 决策依据：01 §2.3（D0-1/D0-5/D3-6）、plan1 Task 3、plan1 Task 2 实施注记（2026-09-11：泵接管帧末冲洗 + 停用总线自 tick）、竖切剧本检查点 7（ScaledDt 减速/冻结）。
- 引擎事实（2026-09-16 源码核实）：`FTickableGameObject::TickObjects` 位于 `UWorld::Tick` 尾部（LevelTick.cpp:1821，晚于全部 tick 组与 TimerManager——Task 2 实证复认），Tickable 自 tick 无法承担 PrePhysics 泵点；`FWorldDelegates::OnWorldTickStart` 于 `UWorld::Tick` 头部广播（LevelTick.cpp:1522）——泵点改由此委托钩子驱动。子系统仍按 01 §2.3 文档口径采用 `UTickableWorldSubsystem` 壳类型，但构造即 `SetTickableTickType(ETickableTickType::Never)`（自 tick 全程停用，泵全部经委托钩子）。暂停帧语义：暂停时 Tickable 不 tick、TimerManager 停摆——"暂停 → 步长 0 → 到期冻结"不变式成立（暂停判定的具体引擎信号在实现时对源码核实）。
- 提案内钉名（plan 未钉、供审阅否决）：堆深度 CVar `Tcs.Core.ExpiryHeapDepth`（对齐既有 `Tcs.Core.PoolSlots` 模式）；时间源接口为抽象 C++ 接口而非 UINTerface（引擎管道设施非 Def 配置数据——PV-1 反射要求只约束参数上下文）；默认源与接口同住 `ITcsTimeSource.h`。
