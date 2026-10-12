# Tasks: 技能消耗的运行态

> 对应 `PLN-R6.5-1` Task 2 / Task 3（Cost 半）/ Task 4（`CostApplied`）。
>
> **★ 本清单是追述性的**：下列各项在补开本提案之前**已实现并取证**（流程偏离，见 `proposal.md`
> 的抬头注记）。清单勾选状态**如实反映事实**，MUST NOT 为"看起来像正常流程"而留空。

## 1. 判定与提交

- [x] 1.1 `CanAffordCost`——五类分开处置（`None` / `CustomFragment` / 无效 tag / 负值 / 零值 / 未持有 / 正常）
- [x] 1.2 三类"配错"fail-closed（无效 `ResourceAttr` / `Amount < 0` / 未持有属性）+ 具名日志
- [x] 1.3 `CommitCost`——`SetBaseValue` 直写（读基值 → 减 → 写回）；**MUST NOT** 用 `ApplyModifier`
- [x] 1.4 提交失败 MUST 被报出（`SetBaseValue` 返回值检查）
- [x] 1.5 `PayCostForEntry`——`ECT_Manual` 档专用入口 + **内部重查 `CanAffordCost`**（TOCTOU 防护）

## 2. 四个提交点

- [x] 2.1 `ECT_OnCastStarted` ⇒ 激活路径"门禁⑤ 之后、建 run 之前"
- [x] 2.2 `ECT_OnCastCompleted` ⇒ `FinishCastRun` 的 Timing 分派（**只在完成路**）
- [x] 2.3 `ECT_OnEvent` ⇒ `UTcsCostEventRelay` 总线订阅
- [x] 2.4 `ECT_Manual` ⇒ 框架零动作（宿主调 `PayCost`）
- [x] 2.5 门禁⑤ 真实现（复用同一函数；裁决④ 的落点写进注释）

## 3. `TcsAttribute` 的唯一改动

- [x] 3.1 `UTcsAttributeSubsystem::TryGetBaseValue`（非确保基值读口，转发现有实现）
- [x] 3.2 持有性判据走 `FindInstance`（**MUST NOT** 依赖 `GetBaseValue` 的 0.0 返回值判断）

## 4. 事件与中继

- [x] 4.1 `TcsEvent.Cast.CostApplied` + `FTcsCostEventPayload`（带导出宏）
- [x] 4.2 `BroadcastCostApplied` 统一出口；**`CanAfford` 失败不发** / 零动作档不发
- [x] 4.3 `UTcsCostEventRelay`——共享 CDO + 等待登记簿；同 tag 只订阅一次
- [x] 4.4 订阅寿命：撤销 / 注销单位 / 门面 `Deinitialize` MUST 退订（use-after-free 防护）
- [x] 4.5 `TriggerEventTag` 无效 ⇒ 具名 Warning + **回退 `OnCastStarted`**（MUST NOT 零动作）
- [x] 4.6 处理两段式（回调内会广播 ⇒ 先拷贝待处理句柄再逐个处理）

## 5. 验证

- [x] 5.1 Development Editor 编译 → `Result: Succeeded`，0 error / 0 warning
- [x] 5.2 Game Shipping 编译 → `Result: Succeeded`，0 error / 0 warning
- [x] 5.3 无头 `Tcs.Test.CooldownCost.Run` → 立即面 **20/0**、含跨帧尾段 **23/0**（两轮逐字相同）
      —— 其中 F1（按基值扣减）/ F2（`CostApplied` 载荷逐项）/ G1~G3（Manual 档）/ H1~H2（OnEvent 档）为消耗面判据
- [x] 5.4 无头 `Tcs.Test.CooldownCost.Reject` → **7/0**（c 无效 tag / d 负值 / e 未持有 / f 重复 PayCost 零半提交 / g 具名降级）
- [x] 5.5 R6 回归 `Tcs.Test.SkillLayer.Run` → **24/0** 且摘要与冻存基线逐字相同
- [ ] 5.6 **编辑器 PIE 复验**（TOCTOU / 跨 PIE 残留属"无头易给假绿灯"的那一类）
      —— 配方见 `Documents/combat-system-design/evidence/2026-10-11-r65-cooldown-cost.md` §4.3

## 6. 收束（归 `PLN-R6.5-1` Task 6 统一执行）

- [ ] 6.1 归档后**手写** `skill-cost` 的 `## Purpose`（归档器写 `TBD` 占位 = `TOOLS-9`）
- [ ] 6.2 `SPEC-04-skill` §3.6 回写落轮标注；§11.3 实现名对照补本变更的落地名
