# Tasks: 冷却多轨道运行态

> 对应 `PLN-R6.5-1` Task 1 / Task 3（冷却半）/ Task 4（三枚冷却事件）。
>
> **★ 本清单是追述性的**：下列各项在补开本提案之前**已实现并取证**（流程偏离，见 `proposal.md`
> 的抬头注记）。清单勾选状态**如实反映事实**，MUST NOT 为"看起来像正常流程"而留空。

## 1. 冷却运行态

- [x] 1.1 `FTcsCooldownOps::StartCooldownOnEntry`——起全部自身轨 + 组轨（CDR 快照一次）
- [x] 1.2 组轨写**单位级共享槽**（`UTcsSkillSubsystem::CooldownGroups`）；同组已有活槽**复用不重起**
- [x] 1.3 `IsEntryOnCooldown`——"全部自身轨清空 + 所属组槽清空"；空 `Tracks` 恒可放 / 陈旧组槽记 Log
- [x] 1.4 `EvaluateCooldownPct`——`CooldownPct` 折叠 + 钳制 `[0,1]` + 非有限值 Warning
- [x] 1.5 `AdjustTrack` / `ClearTrack`——改后**重挂到期堆**；`Total` 只增不减
- [x] 1.6 到期回调 `ExpireTrack` / `ExpireGroup`——**第一条语句即整体复位**（防悬空句柄 ensure）
- [x] 1.7 生命周期配对 `ReleaseEntryCooldowns` / `ReleaseUnitCooldowns`（撤销路径排在释放槽位之前）
- [x] 1.8 门禁③ 真实现（`TcsCastOps.cpp`）——复用同一函数，日志含"滞留在哪一轨/哪个组"

## 2. 公开口

- [x] 2.1 查询四个：`IsOnCooldown` / `GetCooldownRemaining` / `GetCooldownTotal` / `GetCooldownTrackCount`
- [x] 2.2 可变性两个：`AdjustCooldown` / `ResetCooldown`
- [x] 2.3 `StartCooldown`（`ECT_Manual` 档必需；与自动档**完全同一条路径**）

## 3. 终结侧触发点

- [x] 3.1 `DispatchCooldownTiming` 挂在 `FinishCastRun`（唯一汇聚点）
- [x] 3.2 分派表：`Interrupted`/`Replaced` ⇒ 打断档；`Completed`（**含瞬发**）⇒ 完成档；`Manual` 零动作
- [x] 3.3 `OnCastStarted` 档在激活路径起冷（建 run 之后、发 `OnCastStarted` 之前）

## 4. 事件

- [x] 4.1 三枚 tag + `FTcsCooldownEventPayload`（落既有 `TcsEvent` 根；带导出宏）
- [x] 4.2 四个广播点接上（起冷 / 调轨 / 清轨 / 到期），统一出口 `BroadcastCooldownEvent`
- [x] 4.3 起冷**一次起冷发一枚**（不逐轨发 N 枚）；组轨到期按"配了本组的每个条目"各发一枚

## 5. 验证

- [x] 5.1 Development Editor 编译 → `Result: Succeeded`，0 error / 0 warning
- [x] 5.2 Game Shipping 编译 → `Result: Succeeded`，0 error / 0 warning
- [x] 5.3 无头 `Tcs.Test.CooldownCost.Run` → 立即面 **20/0**、含跨帧尾段 **23/0**（两轮逐字相同）
- [x] 5.4 R6 回归 `Tcs.Test.SkillLayer.Run` → **24/0** 且摘要与冻存基线逐字相同
- [ ] 5.5 **编辑器 PIE 复验**（真 GCD / 跨 PIE 残留属"无头易给假绿灯"的那一类）
      —— 配方见 `Documents/combat-system-design/evidence/2026-10-11-r65-cooldown-cost.md` §4.3

## 6. 收束（归 `PLN-R6.5-1` Task 6 统一执行）

- [ ] 6.1 归档后**手写** `skill-cooldown` 的 `## Purpose`（归档器写 `TBD` 占位 = `TOOLS-9`）
- [ ] 6.2 `SPEC-04-skill` §3.5 回写落轮标注；§11.3 实现名对照补本变更的落地名
