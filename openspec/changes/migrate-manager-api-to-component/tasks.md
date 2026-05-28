# 任务 —— migrate-manager-api-to-component

> 任务按 Phase 顺序组织；每个任务独立可验证。Phase A-2 为独立 pre-PR；Phase C → D → E 严格串行（见 `design.md` D-7）。
>
> **⚠️ 权威执行依据（必读）**
>
> 本 `tasks.md` 是**任务索引与验收清单**，**不**是编码指令。
> 实际执行每一项任务时，**必须打开对应阶段的细化方案文档**，严格按其中的文件路径、行号、签名替换规则、调用链保序点落地：
>
> | Phase | 细化方案文档 |
> |-------|-------------|
> | A + B | `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/01_PhaseAB_语义对齐与基础设施.md` |
> | C | `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/02_PhaseC_Attribute业务迁移.md` |
> | D | `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/03_PhaseD_StateRemoval与生命周期迁移.md` |
> | E | `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/04_PhaseE_状态应用槽位链路与查询.md` |
> | F + G | `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/05_PhaseFG_兼容层与最终硬删除.md` |
>
> 勾选某一任务前，必须确认：(1) 细化方案中对应条目的全部替换规则已落地；(2) 本 `tasks.md` 与 specs 中的契约与不变量未被违反。

## 1. Phase A — 基线对齐与移除原因常量（pre-PR 友好）

- [x] 1.1 A-1：活跃文档基线化——扫描 `Plugins/TireflyCombatSystem/Documents/**`（排除 `_archive/`），列出仍描述 `PendingRemoval` / 两阶段移除 / `RemovalFlowPolicy` 的段落清单（本步骤产物：一份待修正文档列表，不改代码）
- [x] 1.2 A-2（**独立 pre-PR**）：在 `Public/State/TcsStateInstance.h` 新增 `TcsStateRemovalReasons` namespace，声明 `Removed` / `Cancelled` / `Expired` / `MergedOut` / `StackDepleted` 五个 `static const FName`
- [x] 1.3 A-2：批量替换现存字符串字面量为常量引用（预期 ≥ 8 处），`grep -n '"Removed"\|"Cancelled"\|"Expired"\|"MergedOut"\|"StackDepleted"'` 验收到零业务残留
- [x] 1.4 A-2：`TireflyGameplayUtilsEditor Win64 Development` 编译通过，作为 pre-PR 提交（commit message `#refactor TCS: unify state removal reasons`）
- [x] 1.5 A-3：清理 `GetStateDebugSnapshot` / `GetSlotDebugSnapshot` 注释中的 `PendingRemoval` 描述与 `RemovalStr` 占位字段
- [x] 1.6 A-3：同步修正 1.1 列出的活跃文档段落或标注"移入 `_archive`"

## 2. Phase B — 公共基础设施

- [x] 2.1 `Public/State/TcsStateManagerSubsystem.h`：`AllocateStateInstanceId()` public 化（`++GlobalStateInstanceIdMgr`）
- [x] 2.2 `Public/Attribute/TcsAttributeManagerSubsystem.h/.cpp`：新增 public `GetAttributeDefinition(FName) const` / `GetModifierDefinition(FName) const` / `AllocateAttributeInstanceId` / `AllocateModifierInstanceId` / `AllocateModifierChangeBatchId`
- [x] 2.3 `UTcsStateComponent` 新增 `TObjectPtr<UTcsStateManagerSubsystem> StateMgr` + `protected UTcsStateManagerSubsystem* ResolveStateManager()`（空时 `ensureMsgf` 诊断后补拉取）
- [x] 2.4 `UTcsAttributeComponent` 新增 `TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr` + `protected UTcsAttributeManagerSubsystem* ResolveAttributeManager()`
- [x] 2.5 两个 Component 的 `BeginPlay` 中预热 Manager 缓存，末尾加 `#if !UE_BUILD_SHIPPING` 块包裹的 `checkf(StateMgr, ...)` / `checkf(AttrMgr, ...)` 自测断言
- [ ] 2.6 等待开发者手动执行编辑器测试：关卡 PIE 启动一次，确认 `checkf` 不触发

## 3. Phase C — Attribute 业务下沉

- [x] 3.1 `UTcsAttributeComponent` 头文件新增本地 public API：`AddAttribute` / `AddAttributeByTag`（非 virtual 包装）/ `SetAttributeBaseValue` / `SetAttributeCurrentValue` / `ResetAttribute` / `RemoveAttribute` / `CreateAttributeModifier` / `CreateAttributeModifierWithOperands` / `ApplyModifier` / `RemoveModifier` / `HandleModifierUpdated` / `RemoveModifiersBySourceHandle`（全部 `virtual`，除 `AddAttributeByTag`）
- [x] 3.2 `UTcsAttributeComponent` 新增 protected virtual 计算核心：`RecalculateAttributeBaseValues` / `RecalculateAttributeCurrentValues` / `MergeAttributeModifiers` / `ClampAttributeValueInRange` / `EnforceAttributeRangeConstraints`
- [x] 3.3 从 `TcsAttributeManagerSubsystem.cpp` 迁移实现到 `TcsAttributeComponent.cpp`，严格遵循替换规则：`CombatEntity → GetOwner()`、`GetAttributeComponent(CombatEntity) → this`、`++GlobalAttributeInstanceIdMgr → ResolveAttributeManager()->AllocateAttributeInstanceId()` 等（见整合版 §C-1）
- [x] 3.4 保留原有行为顺序不变：`BatchId`/`ApplyTimestamp`/`UpdateTimestamp` 写入顺序、`SourceHandleIdToModifierInstIds` 与 `ModifierInstIdToIndex` 维护、事件广播顺序、`EnforceAttributeRangeConstraints()` 终点调用
- [x] 3.5 `TcsAttributeManagerSubsystem` 旧 API 改为薄转发包装器，集中放入 `#pragma region Deprecated_MigrationOnly`，每个加 `UFUNCTION(... meta=(DeprecatedFunction, DeprecationMessage="Use AttributeComponent::XXX"))` 或 `UE_DEPRECATED`
- [x] 3.6 `TireflyCombatSystem` 内部所有调用点改走 Component（`grep` 验收不再出现 `AttrMgr->AddAttribute` / `AttrMgr->CreateAttributeModifier` 等业务调用）
- [ ] 3.7 等待开发者手动执行编辑器测试，覆盖既有 Attribute / Modifier 主路径；编译验证保持为前置完成项

## 4. Phase D — StateRemoval 与生命周期下沉

- [x] 4.1 `UTcsStateComponent` 头文件新增 public API：`TryApplyState`（stub，实现留给 Phase E）/ `RequestStateRemoval` / `RemoveState` / `RemoveStatesByDefId` / `RemoveAllStatesInSlot` / `RemoveAllStates`（全部 `virtual`）
- [x] 4.2 新增 protected virtual 生命周期方法：`ActivateState` / `DeactivateState` / `HangUpState` / `ResumeState` / `PauseState` / `FinalizeStateRemoval` / `IsStateStillValid`
- [x] 4.3 新增 non-virtual 包装辅助：`CancelState` / `ExpireState`
- [x] 4.4 迁移 `FinalizeStateRemoval` 实现，严格保留 8 步顺序（见整合版 §D-2）：校验 → StopStateTree → SS_Expired 幂等短路 → 清本地缓存 → Modifier 清理 → 事件广播 → 槽位移除 → MarkPendingGC
- [x] 4.5 `FinalizeStateRemoval.Step 5` Modifier 清理改走 `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(...)` 直达（不经 `AttrMgr`）；弱引用失效的 fallback 允许存在但不作为主路径
- [x] 4.6 `UTcsStateInstance::SetStackCount()` 中 `StateMgr->RequestStateRemoval(this, ...)` 改为 `OwnerStateCmp->RequestStateRemoval(this, ...)`
- [x] 4.7 `UTcsStateComponent::UpdateActiveStateDurations()` 中 `StateMgr->ExpireState(...)` 改为本地 `ExpireState(...)` 调用
- [x] 4.8 **S1 防护**：`UpdateActiveStateDurations` 二阶段循环中每次 `ExpireState` 返回后加 `if (IsBeingDestroyed() || !IsValid(GetOwner())) return;`
- [x] 4.9 **S3 诊断**：`RemoveAllStates` 入口加 `ensureMsgf(!IsInStateTreeUpdateContext(), ...)`；为此可选引入 1 个 `bIsInStateTreeCallback` 成员，仅在 `OnStateTreeStateChanged`/StateTree Tick 入口用 `TGuardValue` 置位，**仅服务于 `ensure`，不影响控制流**
- [ ] 4.10 等待开发者手动执行编辑器测试，覆盖 S1 DestroyOwner 崩溃防护以及 S3 RemoveAllDuringStateTreeTick / PoolReclaim 时序场景
- [ ] 4.11 `TireflyGameplayUtilsEditor Win64 Development` 编译通过，并等待开发者手动执行编辑器测试，补充确认 Phase D 生命周期迁移后的整体行为稳定性

## 5. Phase E — 状态应用、槽位链路与查询下沉

- [x] 5.1 E-1：迁移 `TryApplyState` / `CreateStateInstance` / `EvaluateAndApplyStateParameters` / `TryApplyStateInstance` / `CheckStateApplyConditions` 到 `UTcsStateComponent`；`CreateStateInstance` 不再接收 `Owner` 参数，直接用 `GetOwner()`；ID 走 `ResolveStateManager()->AllocateStateInstanceId()`；`SourceHandle` 仍经 `UTcsAttributeManagerSubsystem::CreateSourceHandle(...)`
- [x] 5.2 E-1：保留 `TryApplyStateInstance` 既有 5 步顺序（见整合版 §E-1）：初始化检查 → 应用条件 → 槽位分配 → 槽位激活后再写索引 → 广播 `NotifyStateApplySuccess`
- [x] 5.2.1 E-1：`TryApplyStateInstance` 入口新增归属校验——若 `StateInstance->GetOwner() != GetOwner()`，执行 `NotifyStateApplyFailed(ETcsStateApplyFailReason::InvalidInput, ...)` 并直接返回 `false`；Phase F 内 `UTcsStateManagerSubsystem::TryApplyStateInstance` 藄忘包装器转发到 `Instance->GetOwnerStateComponent()->TryApplyStateInstance(Instance)`
- [x] 5.3 E-1：`UTcsStateManagerSubsystem::TryApplyStateToTarget` 精简为"校验 Target → 解析 StateComponent → 调用 `StateComp->TryApplyState(...)`"三步门面
- [x] 5.4 E-2：迁移槽位链路到 `UTcsStateComponent`：`InitStateSlotMappings` / `TryAssignStateToStateSlot` / `RefreshSlotsForStateChange` / `RequestUpdateStateSlotActivation` / `DrainPendingSlotActivationUpdates` / `UpdateStateSlotActivation` / `EnforceSlotGateConsistency` / `ProcessStateSlotMerging` / `MergeStateGroup` / `RemoveUnmergedStates` / `ProcessStateSlotByActivationMode` / `ProcessPriorityOnlyMode` / `ProcessAllActiveMode` / `ApplyPreemptionPolicyToState` / `CleanupInvalidStates` / `RemoveStateFromSlot` / `SortStatesByPriority`（仅 `SortStatesByPriority` 保持 `virtual`）
- [x] 5.5 E-2：新增 `protected bool bIsUpdatingSlotActivation = false;` + `protected TSet<FGameplayTag> PendingSlotActivationUpdates;` 到 `UTcsStateComponent`
- [x] 5.6 **S2 防护**：`UpdateStateSlotActivation` 用 `TGuardValue<bool> Guard(bIsUpdatingSlotActivation, true);` 包裹，保证异常/提前 return 路径 Flag 还原
- [x] 5.7 E-3：`BeginPlay` 改为直接调用本地 `InitStateSlotMappings()`；`SetSlotGateOpen` 改为直接调用本地 `RequestUpdateStateSlotActivation(SlotTag)`；`OnStateTreeStateChanged` 改为直接调用本地 `RefreshSlotsForStateChange(...)`
- [x] 5.8 E-4：迁移查询 API 到 `UTcsStateComponent`（**non-virtual**）：`GetStatesInSlot` / `GetStatesByDefId` / `GetAllActiveStates` / `HasStateWithDefId` / `HasActiveStateInSlot`；优先走 `StateInstanceIndex`，不再遍历 `StateSlotsX`
- [x] 5.9 E-4：清理任何查询路径中"惰性 `RefreshInstances()`"之类的副作用调用
- [ ] 5.10 等待开发者手动执行编辑器测试，覆盖 S2 同帧多次 Apply 合批场景
- [ ] 5.11 `TireflyGameplayUtilsEditor Win64 Development` 编译通过，并等待开发者手动执行编辑器测试，覆盖整合版 §9.2 的全部行为场景

## 6. Phase F — 临时 deprecated 兼容层（若采用）

- [x] 6.1 若本轮保留 `#pragma region Deprecated_MigrationOnly`，检查两个 Subsystem 的覆盖面是否齐全；本次实现已直接进入最终硬删除，源码中无该 region，故本项不适用
- [x] 6.2 若本轮保留废弃 API，确认每个都只是**薄转发**：只做参数校验 + 目标 Component 解析 + 转发，零业务逻辑、零状态维护；本次实现未保留 deprecated 包装器，故本项不适用
- [x] 6.3 `grep` 扫描整个 TCS 内部代码，确认不再有业务调用走 manager actor-local API 或 deprecated 包装器（外部调用者迁移不在本议题范围）

## 7. Phase G — 最终硬删除（完成定义）

- [x] 7.1 删除 `UTcsStateManagerSubsystem` 中全部 actor-local 废弃 API（声明 + 实现 + metadata）
- [x] 7.2 删除 `UTcsAttributeManagerSubsystem` 中全部 actor-local 废弃 API（声明 + 实现 + metadata）
- [x] 7.3 删除 `UTcsStateComponent` 中的 `friend class UTcsStateManagerSubsystem` 声明
- [x] 7.4 删除所有 `UE_DEPRECATED` 标记、`DeprecatedFunction` metadata、只为废弃包装器服务的 include / helper
- [x] 7.5 删除活跃文档中残留的 `PendingRemoval` / 两阶段移除 / `RemovalFlowPolicy` 描述（或将文档移入 `_archive/`）
- [x] 7.6 验收命令必须返回空：`git grep "Deprecated_MigrationOnly" Plugins/TireflyCombatSystem/Source` / `git grep "friend class UTcsStateManagerSubsystem" Plugins/TireflyCombatSystem/Source`

## 8. Final — 完成定义验证

- [x] 8.1 `TireflyGameplayUtilsEditor Win64 Development` 编译通过（0 error / 0 warning 来自 TCS 新增代码）
- [ ] 8.2 等待开发者手动执行编辑器测试，覆盖 S1 / S2 / S3 关键场景与既有回归场景
- [ ] 8.3 等待开发者手动执行编辑器测试：派生 `UMyCustomStateComponent : public UTcsStateComponent` 覆写 `FinalizeStateRemoval`，确认覆写确实被调用（不再被 Manager 绕过）
- [ ] 8.4 等待开发者手动执行编辑器测试：派生 `UMyCustomAttributeComponent : public UTcsAttributeComponent` 覆写 `ClampAttributeValueInRange`，确认覆写确实被调用
- [ ] 8.5 完成定义勾选清单（整合版 §10 六条）全部满足
- [x] 8.6 `openspec validate migrate-manager-api-to-component --strict --no-interactive` 通过
- [ ] 8.7 运行 `openspec archive migrate-manager-api-to-component --yes`，把变更归档到 `changes/archive/<date>-migrate-manager-api-to-component/`
