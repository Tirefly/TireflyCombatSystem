## 1. 策略形状与入口

- [x] 1.1 `FStateStackPolicy` 补 `FInstancedStruct CustomDecision`（裸载体 + 手写 `meta = (BaseStruct = ...)`）；`EGroupByPolicy` 的"按分组词"档与 `GroupTag` 字段**裁撤**（裁定 9）
- [x] 1.2 `EOverflowPolicy` 裁为两档（`RejectNew` / `ReplaceExisting`）；`EGroupByPolicy` / `EValueStackPolicy` / `EStackDurationPolicy` 的注释口径订正（Custom 走高位置位、`PerStackValue` 本轮无行为、裁撤档留痕）
- [x] 1.3 `FTcsStateStackDecisionFragment`（新头 `Public/State/TcsStateStackFragment.h` + `.cpp`）：`IsSameGroup` / `ShouldAccept` / `ResolveStacks` 三个中性默认实现（`USTRUCT(meta = (Hidden))`，禁 `= 0` 与 `PURE_VIRTUAL`）；同头附决策词汇（`FTcsStateStackRequest` / `FTcsStateStackDecision`）
- [x] 1.4 `EApplyResult` 四档 ToolTip 与可达性口径订正（`Refreshed` = 同来源续杯；`Stacked` 起可达）

## 2. 决策与刷新接线

- [x] 2.1 `TcsStateOps_Stack.cpp`：组键解析（`None` / `PerSource` / `PerInstigator` / `Custom`；基座含 `DefTag` ⇒ 不同定义永不共组；Custom 载荷缺失/类型不符 ⇒ 退化 + `Warning`，且**一次施加只解析一次载荷**）
- [x] 2.2 决策树 + 四档回执（同来源 ⇒ `Refreshed`（层数不变）/ 异来源未满 ⇒ `Stacked` / 满仓 ⇒ `Rejected`（`Log`、无广播）或替换（移除旧 + 建新 ⇒ `Applied`）；**未声明来源按同来源处理**）
- [x] 2.3 叠层与刷新流水：层数写入 → 数值（`KeepMax` 不随层 / `AddValues` 在物化边界按层成倍）→ 快照重建 → 修正器按来源重挂 → 时长（`ESD_None` 保留剩余 / `ESD_RefreshRemainingToTotal` 回满额，`ScheduleTime` 按轴改）+ 周期走既有 `EPR_*`
- [x] 2.4 广播顺序与纪律：叠层路径 **`StackChanged` 先于 `Refreshed`**；层数未变（同来源刷新 / 替换）⇒ 不广播 `StackChanged`；替换 ⇒ `Removed` + `Applied`
- [x] 2.5 `TcsStateOps.cpp` 的暂用判据替换 + 重入纪律复核（提交会广播 ⇒ 决策后仍须重新取桶与实例）

## 3. 编译与冒烟

- [x] 3.1 UBT Development Editor 编译通过（零 warning / 零 error）
- [x] 3.2 UBT Game Shipping 编译通过（含新 `UPROPERTY` 与 `USTRUCT` 面 ⇒ 本任务必跑）
- [x] 3.3 宿主装置（**LAC 仓** `Source/TcsDev/`）改造：观测者状态事件槽位 5 → 6（纳入 `StackChanged`）并加 `LastRefreshedSeq` / `LastStackChangedSeq` 两条定序读数；新增宿主决策样本 `TcsDevStackDecisionSample.h/.cpp`；`DefaultGameplayTags.ini` 补两枚定义身份探针词（19c / 21e **复核后无需改动**——未声明来源按同来源处理）
- [x] 3.4 装置新增检查 **22a–22m**（读数形态先定后写）：不分组 + 无限 + 取最大（首施 / 同来源续杯 / 异来源叠层且数值不变 / 定序 / 不同定义永不共组）· 满仓替换（句柄换、层数回 1、无 `StackChanged`）· 满仓拒绝（`Rejected` + 实例不动 + 四类事件零增量）· 按发起者 + 累加档（叠层 ⇒ 账本值成倍；换发起者 ⇒ 另建实例）· 按来源（同来源续杯 / 换来源另建）· 时长两档（回满额 / 保留剩余）· Custom 宿主样本（恒不同组 ⇒ 两次都 `Applied` + 在册 2 条）
- [x] 3.5 拒绝面新增检查 **K**（自定义载荷缺失 ⇒ 退化 + 2 条预期 Warning）；拒绝面头部预期红字数同步（7 → 9）
- [x] 3.6 PIE 实测：`Tcs.Test.Slice.Run`（**零非预期红字**，头部预期红字清单仍为 3 条）+ `Tcs.Test.Slice.Reject` 全绿；`StackChanged` 首次真实广播、`EAR_Stacked` 首次可达；**实跑后按日志核对两处头部红字数**，不符则改文案并重编重跑

## 4. 收口

- [x] 4.1 归档提案（`openspec archive add-state-stacking-policies --yes`），新能力 `## Purpose` **手写**（归档器只为新能力生成占位符）
- [x] 4.2 `openspec validate --all --strict --no-interactive` 全绿 + `openspec/changes/` 零活动提案
- [x] 4.3 `PLN-R5` Task 5 勾选 + 落地记录（含对本计划文本的订正：Step 1 的 `PerTag` 与 `Overflow` 三档、Step 2 括号注、Step 4 载体形态、Step 5 首行等价表的近似性）
- [x] 4.4 四处设计文档订正（`SPEC-02-states` §3.2 的 Custom 值 / `GroupBy` 四态 / 等价表 / 溢出档；`LOG-DECISIONS` D3-4 同句）
- [x] 4.5 证据文档（区段行号 + 复算脚本 + 边界清单）+ 台账三条（`STAT-6` 按层取值零行为 / `STAT-7` 跨定义共享层数组 / `STAT-8` 逐层来源归属）+ 两册日志 + `SPEC-02-states` §12.9 + `ledger` 收束块
