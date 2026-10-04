# Tasks: 状态参数快照、等级源与 Duration-Period 到期堆

> 步骤编号沿用 [`PLN-R5`](../../../Documents/combat-system-design/plans/plan-r5-state-layer.md) Task 3 的 Step 1–8 / 10（Step 9 属另一份提案 `add-param-source-host-slot`）。

## 1. 快照类型与读取适配器

- [x] 1.1 新建 `Source/TcsState/Public/State/TcsStateSnapshot.h`：`FTcsParamSnapshotEntry{ FGameplayTag Key; double Value; FInstancedStruct SourceRef; }` + `FTcsParamSnapshot{ TArray<FTcsParamSnapshotEntry> Entries; }` 与 `TryGetNumericParam(FGameplayTag, double&) const`
- [x] 1.2 新建 `FTcsStateParamTableReader`（**纯 C++ 类**：持快照只读指针 + 键查询转发；本轮不派生 `UObject`、不实现 `ITcsParamTableReader`——唯一消费方是 C++ 直调，而该接口不是 `Blueprintable`）
- [x] 1.3 `FTcsStateInstance` 补 `ParamSnapshot` 字段（Task 2 注释留白处）+ 补 `PeriodEntry` 字段（周期条目独立锚点）

## 2. 快照构建

- [x] 2.1 `Apply` 流程：逐行求值 `Def.Params`——`Overrides` 命中即取覆盖值，否则 `Def.Base.Evaluate(Ctx)`
- [x] 2.2 求值上下文组装集中一处（`FTcsStateOps::MakeContext`）：`ParamTable` / `Subject` / `Instigator`（无效取 `Target`）/ `EffectiveLevel` = `LevelBase` / `LevelProvider` = 门面登记的宿主读口
- [x] 2.3 `ValueConvention` 转换在此写入点发生（`FTcsValueConvention::ConvertToCanonical`，**全库首次点亮**）；`SourceRef` 存该行的数值来源副本；能力位为假的行按"不转换"降级
- [x] 2.4 刷新路径按同规则**重建快照**（`BuildSnapshot` 先 `Reset` 再填——新 payload 覆盖旧的）

## 3. 等级源与等级读口

- [x] 3.1 新建 `Source/TcsState/Public/Host/TcsEntityLevelProvider.h`：`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) int32 GetEntityLevel(FTcsCombatEntityHandle)`；**同文件**放派生上下文 `FTcsStateEvaluateContext`（唯一新增字段 `LevelProvider`，覆写 `GetScriptStruct`）
- [x] 3.2 `UTcsStateSubsystem` 加 `SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider>)` / `GetEntityLevelProvider()`（`UPROPERTY` 持有；未注入 = nullptr 是配置状态不是错误）
- [x] 3.3 快照构建与时值求值一律装配 `FTcsStateEvaluateContext`（不是基类上下文）
- [x] 3.4 四个源：`_StateLevelArray` / `_StateLevelMap`（`Context.EffectiveLevel` → 下标/键，数组越界落最后一档）/ `_InstigatorLevelArray` / `_InstigatorLevelMap`（`Context.Instigator` → provider → level）；四型继承 `FTcsParamEnumerableSource` 并覆写 `GetIndexForLevel`；上下文类型判定失败落兜底
- [x] 3.5 `ApplyState` 补两个可选形参：`Instigator`（无效 = 取 `Target`）与 `ParamTable`（空 = 引用类源落兜底）
- [x] 3.6 `ValueConvention` 白名单校验（`UTcsBuffDefAsset::IsDataValid`）补一条：能力位为假而行上配了非 `VCF_None` ⇒ Error

## 4. Duration 与 Period 落堆

- [x] 4.1 入堆 / 撤堆集中封装（`FTcsStateOps::PushExpiry` / `PushPeriod` / `CancelTimeEntries` / `RescheduleExpiry`）
- [x] 4.2 `Finite`：`DurationRemaining` 由 `DurationTime` 求值（与快照同装配的上下文），入堆；`Infinite`：不入堆、字段置 0
- [x] 4.3 `Period > 0`：入周期条目，回调广播 `TcsEvent.State.Periodic`（载荷带当前 `Stacks` + `Level`）后**重新入堆**
- [x] 4.4 `PeriodRefresh`：`Keep`（保留剩余）/ `Reset`（满额重建）/ `Immediate`（同步执行一次再满额重建）——作用于刷新路径

## 5. 生命周期操作与撤销

- [x] 5.1 `ExtendDuration` / `SetRemaining`：撤销旧条目 + 按新余量重入堆；`Infinite` / 定义未登记 = `Warning` + 无操作
- [x] 5.2 移除路径：`Remove` 先撤销该实例两个条目，再走 `Expiring → 广播 → 归还槽位`
- [x] 5.3 `Deinitialize`：清空桶之前逐条撤销待撤销条目（Task 2 待办①）
- [x] 5.4 到期回调：`UWorld` 弱引用 + 句柄代际校验 → `ExpireState` → `EStateRemoveCause::Expired`
- [x] 5.5 GC 面：`AddReferencedObjects` 扩到在册实例快照的 `SourceRef`（逐条走 `FInstancedStruct::AddStructReferencedObjects`）

## 6. 校验面收口（作者期）

- [x] 6.1 `UTcsBuffDefAsset::IsDataValid` 补 `ValueConvention` 白名单校验（见 3.6）

## 7. 编译与冒烟

- [x] 7.1 UBT Development Editor + Game Shipping 双配置编译，零 error 零 warning —— **✅ 2026-10-04 双绿**（删净临时诊断后的最终版：Editor / Shipping 各 `Result: Succeeded`，两日志 error / warning 命中数全 0）
- [x] 7.2 宿主装置（LAC 仓）：`Tcs.Test.Slice.Run` 扩检查块——快照冻结（覆盖 + 默认两路）/ 等级源按档取值（两条路各取各自等级）/ 越界与兜底 / 周期若干次 + 到期 `Expired` + 周期随之停止 / `ExtendDuration` 生效 —— **✅ 实测即时 32/0 + 延迟段 39/0**（新增 `20a`–`20e` 同步面 + `20f`–`20l` 时间面 + 一条"夹具就绪"判定）
- [x] 7.3 `Tcs.Test.Slice.Reject` 扩拒绝面：`Infinite` 上调时长操作、未登记定义上的时长操作 —— **✅ 实测 9/0**（新增检查 H / I）
- [x] 7.4 常规命令零红字；证据文档落锚点与区间哈希 —— **✅** 证据 = `EVID-2026-10-04-state-param-snapshot-and-level-sources`（区段 L2416–L2760、345 行 / 49,691 字节 / SHA-256 `7b82a338…`、复算脚本、9 条边界）
  - **一处如实说明（非本轮机制缺陷）**：区段内 4 条 `状态时值非正` Warning 来自**内容侧**——验收资产 `DA_Check_BuffDef` 是 `Finite` 但 `DurationTime` 求值为 0（Task 3 新引入的读数）。它不影响任何断言，但让"零红字"严格意义上不成立；修法（内容侧配明确时长 / 或校验里放行"显式 0 = 立即到期"）记入证据 §5 边界④
