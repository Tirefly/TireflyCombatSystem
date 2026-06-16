# 变更：将 TCS Manager 的 Actor 本地业务 API 迁移到 Components

## 背景

TCS 当前把 `State` / `Attribute` 的核心业务逻辑（创建、应用、生命周期、移除、槽位、查询、属性 CRUD、Modifier 管线）集中在两个 `GameInstanceSubsystem`（`UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem`）上，而 `UTcsStateComponent` / `UTcsAttributeComponent` 仅作为数据容器与事件广播者。这带来三个实际问题：

1. **扩展点错位**：开发者继承 Component 无法覆写真正的核心流程；想替换移除语义、槽位激活策略、Modifier 清理路径必须改 Subsystem，不符合 UE 组件化惯例。
2. **耦合过深**：Manager 直接操作 Component 内部状态（含 `friend class` 放行），两边职责重叠、内部调用路径不统一（`StackDepleted → StateMgr->RequestStateRemoval`、`Duration 到期 → StateMgr->ExpireState`、Modifier 清理绕道 `AttrMgr` 再回 Owner）。
3. **全局锁粒度错误**：`bIsUpdatingSlotActivation` / `PendingSlotActivationUpdates` 挂在 Subsystem 上，导致 Actor A 的槽位激活阻塞无关的 Actor B ——这是原有设计缺陷，不是迁移引入的问题，但迁移是修复它的最佳窗口。

整合版执行文档 `Plugins/TireflyCombatSystem/Documents/执行文档：Manager API迁移到Component（整合版）.md` 已沉淀完整方案（阶段、API 去向、边缘场景 S1/S2/S3、单 Component 夹值边界、UE 5.7 引擎自保护机制审计），并由 `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/` 目录下的五份文档（00 总览 / 01 PhaseAB / 02 PhaseC / 03 PhaseD / 04 PhaseE / 05 PhaseFG）精确到**文件路径 + 行号**。本提案把上述方案转写为 OpenSpec 规格。

> **权威执行依据**
>
> 本提案的 `proposal.md` / `design.md` / `tasks.md` / `specs/**/spec.md` 属于**规格层**文档，用于描述契约与验收标准。
> **实际编码时必须严格按照 `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/` 目录下的细化方案执行**，包括文件路径、行号、签名替换规则与调用链保序点。
> 当规格层与细化方案存在描述差异时，以细化方案中的**具体执行步骤**为准；但不得违反规格层定义的契约与不变量。

## 变更内容

- **下沉 Attribute 业务到 `UTcsAttributeComponent`**：属性增删改、Modifier 创建/应用/移除/更新、`RemoveModifiersBySourceHandle`、重算、`EnforceAttributeRangeConstraints` / `ClampAttributeValueInRange` 全部成为 Component 成员（多数 `virtual`）。
- **下沉 State 业务到 `UTcsStateComponent`**：状态创建/应用、参数评估、条件检查、槽位分配与激活（包括防重入 Flag）、生命周期阶段转换、`RequestStateRemoval` / `FinalizeStateRemoval` 主链路、查询 API 全部下沉。
- **Subsystem 降级为全局门面**：两个 Manager 最终只保留定义缓存加载、定义/Tag 查询、全局 ID 工厂、`CreateSourceHandle`、`TryApplyStateToTarget` 跨 Actor 便捷门面。
- **Modifier 清理路径改走 OwnerAttributeComponent 直达**：`FinalizeStateRemoval` 不再经 `AttrMgr->RemoveModifiersBySourceHandle(Owner, SourceHandle)`，而是 `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(SourceHandle)`。
- **内部调用 component-first**：`SetStackCount` / `UpdateActiveStateDurations` / `BeginPlay` / `SetSlotGateOpen` / `OnStateTreeStateChanged` 等调用点一并改走 Component 入口，不允许"只搬 API 不搬调用"。
- **查询 API 改用 `StateInstanceIndex`**：`GetStatesInSlot` / `GetStatesByDefId` / `GetAllActiveStates` / `HasStateWithDefId` / `HasActiveStateInSlot` 优先走索引，不再全表扫描 `StateSlotsX`；查询 API **有意不标 `virtual`**（索引维护契约强耦合写路径）。
- **`TryApplyStateInstance` 归属校验（细化方案新增防护）**：迁到 Component 后，`TryApplyStateInstance` 入口校验 `StateInstance->GetOwner() == GetOwner()`，不匹配则 `NotifyStateApplyFailed(InvalidInput, ...)` 并返回 `false`；防止将 A Actor 的实例误注入 B Actor 的组件。
- **统一移除原因常量**：引入 `TcsStateRemovalReasons` namespace，现有 8 处字符串字面量（`"Removed"` / `"Cancelled"` / `"Expired"` / `"MergedOut"` / `"StackDepleted"` 等）批量替换——作为独立 pre-PR 先于主迁移合入。
- **Manager 缓存策略明确化**：`BeginPlay` 预热 `StateMgr` / `AttrMgr` + `ResolveXxxManager()` helper 内 `ensureMsgf` 诊断 + `BeginPlay` 末尾 `checkf` 自测断言（Debug/Development）。不做"运行时自愈"。
- **边缘场景 S1/S2/S3 防护**：
  - S1：`UpdateActiveStateDurations` 每次 `ExpireState` 后 `IsBeingDestroyed()` + `IsValid(GetOwner())` 双检，自毁即 `break`。
  - S2：`UpdateStateSlotActivation` 用 `TGuardValue<bool>` 包裹防重入 Flag。
  - S3：引擎层（`FStateTreeExecutionContext::Stop()` 的 `RequestedStop` 延迟机制）已自保护；TCS 仅在 `RemoveAllStates` 入口加 `ensureMsgf` 诊断 + 对象池归还合约约束。
- **BREAKING**：`UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` 上所有 actor-local 业务 API 最终**硬删除**，`friend class UTcsStateManagerSubsystem` 一并移除。迁移期可短暂保留 deprecated 薄转发包装器（`DeprecatedFunction` / `UE_DEPRECATED`），但议题完成前必须清零——"还存一个包装器就不算完成"。
- **不做**：不重新引入 `PendingRemoval` / `FinalizePendingRemovalRequest` / `RemovalFlowPolicy` / `HardTimeout`；不支持跨 Actor 属性夹值（Clamp Context 仍绑定单 Component）；本轮不扩面 `BlueprintNativeEvent`（当前无蓝图引用）。

## 影响范围

- **受影响规范（新增）**：
  - `state-management` —— State Component 作为主业务入口
  - `attribute-management` —— Attribute Component 作为主业务入口
  - `combat-manager-subsystems` —— 两个 Subsystem 降级后的全局职责
- **受影响代码**：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateInstance.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateInstance.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeManagerSubsystem.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Attribute/TcsAttributeManagerSubsystem.cpp`
 - **受影响验证**：不再要求 AI 侧新增或执行专门测试代码；相关行为验证统一等待开发者手动执行编辑器测试，重点覆盖 StateRemoval、属性 Modifier、槽位激活，以及 S1 / S2 / S3 关键场景。
- **受影响文档**：非 `_archive` 活跃文档若仍描述 `PendingRemoval` 两阶段模型，本次一并修正或移入 archive。
- **对使用方的影响**：仓库内当前**无蓝图引用**旧 Manager API，因此不涉及 redirector / 蓝图节点迁移；外部 C++ 调用方在迁移期可通过 deprecated 包装器过渡一段时间，但**最终必须**迁移到 Component 入口。
