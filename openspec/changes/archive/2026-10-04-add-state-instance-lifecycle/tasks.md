## 1. 状态实例与注册表

- [x] 1.1 `TcsStateEnums.h` 补 `EStatePhase` / `EStateRemoveCause` / `EApplyResult`（`EDurationPolicy` / `ETcsPeriodRefresh` 已于 Task 1 落地，不重写）—— **✅ 2026-10-04**
- [x] 1.2 新增 `Public/State/TcsStateHandle.h`：`FTcsStateHandle`（`USTRUCT(BlueprintType)`、`Index` + `Generation`、`IsValid`、相等比较、`GetTypeHash`）—— **✅ 2026-10-04**（`HashCombine` 需显式 include `Templates/TypeHash.h`，不在 `CoreMinimal` 传递闭包内）
- [x] 1.3 新增 `Public/State/TcsStateInstance.h`：`FTcsStateInstance` 纯数据（身份 / 数值 / 时间 / 阶段），MUST NOT 持策略、载荷、订阅句柄、`UObject` 引用 —— **✅ 2026-10-04**（**实现名 `FTcsStateInstance`**；**新增 `Unit` 字段**——原提案未列，理由见 `PLN-R5` Task 2 落地记录；`ParamSnapshot` 类型归 Task 3 故按注释留白）
- [x] 1.4 新增 `Public/State/TcsStateRegistry.h` + `Private/State/TcsStateRegistry.cpp`：per-unit 桶（`TUniquePtr` 间接层）、槽位分配 / 释放、代际校验、桶清理 —— **✅ 2026-10-04**
- [x] 1.5 注册表对脏句柄（代际失配 / 下标越界 / 单位不存在）的拒绝面：拒绝 + `LogTcsState` Warning，不 `ensure` —— **✅ 2026-10-04**（行为级实测 = `.Reject` 检查 G）

## 2. 门面与定义登记

- [x] 2.1 新增 `Public/TcsStateSubsystem.h` + `Private/TcsStateSubsystem.cpp`：`UTcsStateSubsystem : UWorldSubsystem`、`DoesSupportWorldType`（仅 Game / PIE / GamePreview）、`Deinitialize` 全量清理 —— **✅ 2026-10-04**（发号器**不复位**——`Id` 契约是"进程内永不复用"）
- [x] 2.2 施加与查询：`ApplyState` / `GetState` / `IsStateActive` / `ForEachState`（`TFunctionRef`，按槽位下标升序稳定遍历）—— **✅ 2026-10-04**（另加 `GetStateCount` / `GetTotalStateCount` 两个观测口）
- [x] 2.3 移除与生命周期操作：`RemoveState` / `ExpireState` / `UnregisterUnit` / `ExtendDuration` / `SetRemaining`（后两者本轮为"签名 + 字段落点"：`Infinite` 上调它们 = Warning + 无操作，堆同步归 Task 3）—— **✅ 2026-10-04**
- [x] 2.4 定义登记口：`RegisterStateDef` / `UnregisterStateDef` / `GetRegisteredStateDef`（登记表自持定义副本的 `TUniquePtr`，按 `DefTag` 键控；未登记 / 重复 / 空 `DefTag` 的拒绝面）—— **✅ 2026-10-04**（**签名实为 `RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef& Def)`**：身份归资产、内容归数据 struct，登记口显式收身份；理由与备选见 `SPEC-02-states` §12.6 第 1 条）
- [x] 2.5 `Source` 发号：每个实例 `ApplyState` 时经 `FTcsSourceHandleRegistry::Allocate()`（Task 0 的统一发号器）取来源句柄，作为**同一状态 = 同一来源**的级联锚点 —— **✅ 2026-10-04**（验收读数：三次施加得到来源 14 / 15 / 16，互异非 0）
- [x] 2.6 TcsIntegration 侧逐世界登记：`UTcsDefinitionSubsystem` 在已装配世界把缓存状态定义登记进 `UTcsStateSubsystem`（沿用既有 `SeedWorld` 单出口；`TcsState` MUST NOT 反向依赖 `TcsIntegration`）—— **✅ 2026-10-04**（`SeedWorld` 的"无内容即返回"判据随之改为看三类定义）

## 3. 生命周期事件

- [x] 3.1 新增 `Public/State/TcsStateEvents.h` + `Private/State/TcsStateOps_Events.cpp`：六枚原生 tag（`TcsEvent.State.Applied` / `Refreshed` / `StackChanged` / `Expired` / `Removed` / `Periodic`，`UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG_COMMENT`，常量带 `TCSSTATE_API`）—— **✅ 2026-10-04**
- [x] 3.2 载荷 `FTcsStateEventPayload`（`USTRUCT(BlueprintType)`）：`Handle` / `DefTag` / `Source` / `Instigator` / `Stacks` / `Level` / `Cause`（`Cause` 仅 `Expired` / `Removed` 有意义）—— **✅ 2026-10-04**（`Source` **不是** `UPROPERTY`：`FTcsSourceHandle` 非反射）
- [x] 3.3 广播走 `UTcsEventBusSubsystem::PublishImmediate`（同一个提交内到达，便于装置在同一帧断言）—— **✅ 2026-10-04**

## 4. 广播点与阶段机

- [x] 4.1 `ApplyState` 的共存结果是**恰好一枚**事件：无同组在册 ⇒ `Applied`；命中同组 ⇒ `Refreshed`（本轮判据 = 同单位 + 同 `DefTag`，**是 Task 5 五轴决策的暂用位**，Task 5 在原处替换为策略驱动）—— **✅ 2026-10-04**（代码注释已就地标明替换点）
- [x] 4.2 `ExpireState` ⇒ `Expired`（原因 = `ESRC_Expired`）；`RemoveState` ⇒ `Removed`（原因由调用方给）—— **✅ 2026-10-04**
- [x] 4.3 阶段机 `EStatePhase` 迁移校验：`Inactive → Active`（施加）→ `Expiring`（移除中）→ `Inactive`（槽位释放）；非法迁移 `ensure`；撤销顺序 = 先广播后释放槽位（订阅者在回调里仍能 `GetState` 读到实例）—— **✅ 2026-10-04**（另加第四条合法边 `Expiring → Active`：刷新是"过渡后挂回"）
- [x] 4.4 `UnregisterUnit`：批量移除该单位全部实例（逐条广播 `Removed`）后删桶 —— **✅ 2026-10-04**

## 5. 编译与验收

- [x] 5.1 UBT **Development Editor** 编译通过（0 error / 0 warning）—— **✅ 2026-10-04**
- [x] 5.2 UBT **Game Shipping** 编译通过（本轮含新 `UPROPERTY` 与 `WITH_EDITOR` 无关面，仍需双配置）—— **✅ 2026-10-04**
- [x] 5.3 宿主装置（**LAC 仓** `Source/TcsDev/.../TcsDevSliceRig.cpp`）扩检查块：直接调 `ApplyState` 建实例 → 总线订阅者收到 `TcsEvent.State.Applied` → `RemoveState` 后再收 `TcsEvent.State.Removed`；**常规验收命令零红字** —— **✅ 2026-10-04**（落成 **19a–19f** 六条：19a 定义逐世界登记 / 19b 施加 + `Applied` 到达且回调内可读实例 / 19c 重复施加 ⇒ `Refreshed` / 19d 移除 ⇒ `Removed` 原因可读 + 旧句柄被拒 / 19e 单位注销逐条 `Removed` / 19f 未登记定义被拒；另在 `.Reject` 命令扩 **E/F/G** 三条拒绝面，其中 **G 用真实路径造陈旧句柄**并验证"拒绝不误伤"）
- [x] 5.4 证据文档 `Documents/combat-system-design/evidence/2026-10-04-state-instance-lifecycle.md`（逐项判据锚点 + 活动日志区段 SHA-256 + 边界清单）—— **✅ 2026-10-04**（**双区段**：常规 L2420–L2711 / 拒绝面 L2416–L2499，各带 SHA-256 与复算脚本；七条边界）
- [x] 5.5 计划 / 台账 / 两册日志同步（`PLN-R5` Task 2 勾选与落地记录、`SPEC-02-states` §3.3 的 `Combat.State.Periodic` 旧名改 `TcsEvent.State.Periodic`）—— **✅ 2026-10-04**（另加：`INDEX` 证据 10 → 11 与两处计数、台账《Task 2 收束》块、`LOG-DECISIONS` 四条裁定、`SPEC-02-states` 新增 **§12.6** 六条落地口径）
