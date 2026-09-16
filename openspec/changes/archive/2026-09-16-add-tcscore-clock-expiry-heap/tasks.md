## 1. Implementation

- [x] 1.1 `Public/Clock/ITcsTimeSource.h`（header-only）：抽象时间源接口（`GetDeltaSeconds(World, RawDeltaSeconds)`）+ 默认源（`RawDeltaSeconds × World.GetTimeDilation()`）
- [x] 1.2 `Public/Clock/FTcsClock.h`（header-only）：`{Frame: uint64, Elapsed: double, DeltaSeconds: double}` 纯数据推进结构
- [x] 1.3 `Public/Clock/FTcsExpiryHeap.h`（header-only）：条目池化（`TTcsInstancePool` 代际校验）+ 最小堆数组 + 稳定序（DueTime 升序、同刻按入堆序）；`Push` / `Cancel`（惰性）/ `AdvanceTo`；到期回调内新入堆归下一拍；堆深度统计接 TcsCoreStats
- [x] 1.4 `TcsCoreStats.h/.cpp`：`FTcsCoreHeapStats` 维护口 + CVar `Tcs.Core.ExpiryHeapDepth`
- [x] 1.5 `Public/Clock/UTcsClockSubsystem.h` + `Private/Clock/UTcsClockSubsystem.cpp`：`UTickableWorldSubsystem` 壳（自 tick 全程停用——重写 `GetTickableTickType()` 返回 Never：引擎在 Initialize 内以该返回值注册自 tick，构造期 SetTickableTickType 会被重注册覆盖）；世界类型过滤（Game/PIE/GamePreview，对齐总线门面）；`OnWorldTickStart` 绑定（世界标识过滤）+ `Deinitialize` 解绑并清堆；固定泵序：源取步长（暂停帧 0）→ 时钟推进 → 总线 `FlushFrameEndQueue` → 堆 `AdvanceTo(Elapsed)`；到期回调打 `LogTcsCore` Log 级（含到期时刻）。暂停判定 = `UWorld::IsPaused()`、TimeDilation 读 `WorldSettings->TimeDilation`（实现期源码核实完毕）。**实施增补**：`PushExpiry` / `CancelExpiry` 转发入口（`AdvanceTo` 保持泵私有）——spec R3 已同步
- [x] 1.6 `UTcsEventBusSubsystem.h/.cpp`：停用自 tick（重写 `GetTickableTickType()` 返回 Never）、移除自 tick 冲洗调用、头注释口径更新（冲洗归时钟泵；`FlushFrameEndQueue` 保留为泵驱动入口）

## 2. Verification

- [x] 2.1 Development Editor 编译通过（UBT，LAC 项目，2026-09-16 一次通过）
- [x] 2.2 临时测试装置（`Private/Testing/`，不入库）增补 `Tcs.Test.Clock`（A 段本地堆机制 5 项 + B 段泵集成——入堆经 TimerManager 回调执行（恒晚于同帧泵点，帧号判定确定性），轮询预算实时 20 秒覆盖 slomo、中间静默）；故意 ensure 的悬空 Cancel 拆为 `Tcs.Test.Clock.Dangling`（opt-in——引擎 ensure 每站点每进程只上报一次，混入常规命令会致首次运行断点卡顿）；`Tcs.Test.Bus` 帧末发布同款阶段 2 化 + "到达帧 > 发布帧"检查——**用户 PIE 实测通过**（`Tcs.Test.Clock` 14/14 PASS、`Tcs.Test.Bus` 全项 PASS、装置三轮校准闭环）
- [x] 2.3 用户 PIE 检查点 7 提前验——**slomo 半边已实测确认**（slomo 0.1 vs slomo 1 双跑对照：游戏时间增量恒 ≈0.50s，实时时长按 1/TimeDilation 从 54 帧拉长至 400+ 帧，ScaledDt 语义成立；slomo 1 全项 14/14 PASS、迟滞 0.005s）；**`pause` 冻结项未单独回报**（观测方式：暂停期轮询随 TimerManager 停摆 + 到期回调静默，解除后补触发）——如需补验执行 `Tcs.Test.Clock` 后 0.5s 窗口内 `pause` 观察
