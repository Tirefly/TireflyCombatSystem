# 设计 —— 将 TCS Manager API 迁移到 Components

## 背景

> **权威执行依据**：本 `design.md` 用于讨论技术决策与权衡。**具体编码步骤（文件/行号/签名替换规则/调用链保序）以 `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/` 下五份文档为准**，实施者必须严格遵循，不得自行发挥。若设计决策与细化方案冲突，应优先以细化方案为准并回写本文档。

当前 TCS 的 `State` / `Attribute` 核心业务集中在两个 `GameInstanceSubsystem`，Component 仅承担数据与事件广播。历史原因是早期 TCS 沿用"中心化管理器 + 薄组件"的模式，但随着 StateTree 集成、SourceHandle 因果链、对象池需求的引入，这套模式暴露出以下张力：

- **UE 组件化惯例**：Actor-local 业务逻辑理应挂在 ActorComponent 上，便于继承覆写与世界/关卡生命周期对齐。
- **StateTree 集成**：`UTcsStateComponent` 继承 `UStateTreeComponent`，StateTree 的 Task 回调天然产生对 Component 的 self-reference，业务逻辑挂在 Subsystem 上就意味着 Task → Subsystem → Component 绕一圈。
- **对象池**：`TireflyActorPool` 的归还回调通常调用 `RemoveAllStates()`，目前必须经由 Subsystem，池回收链路跨系统。

这是一次**归属迁移 + 耦合清理**，而不是语义重新设计。当前 `StateRemoval` 是**单阶段立即收敛**（`RequestStateRemoval → FinalizeStateRemoval`，通过 `SS_Expired` 短路保证幂等），迁移必须尊重这一事实，**不能**把旧 `PendingRemoval` / `FinalizePendingRemovalRequest` / `RemovalFlowPolicy` / `HardTimeout` 重新带回来——这一点在 `openspec/specs` 落地后写入契约。

**利益相关方**：
- TCS 插件维护者（本次主要实施者）
- TCS 外部 C++ 调用者（迁移期通过 deprecated 包装器过渡）
- 未来基于 TCS 构建游戏逻辑的策划/程序（获得可继承的清晰扩展点）

## 目标 / 非目标

**目标**：

- State/Attribute 所有 actor-local 业务下沉至对应 Component；Component 成为 C++ 子类扩展的唯一真源。
- Subsystem 降级为"定义缓存 + ID 工厂 + 跨 Actor 门面"三件事。
- 内部调用路径以 Component 为先，消除"先找 Actor → 走 Interface → 找 Component"的绕行。
- 修正槽位激活防重入的全局锁粒度错误（Actor-scope 隔离）。
- 边缘场景 S1（Owner 在回调中自毁）、S2（同帧多次 Apply）、S3（StateTree Tick 内部 Stop）得到最小且足够的防护。
- 议题完成前 deprecated 包装器与 `friend class` 声明全部删除——无"长期过渡层"。

**非目标**：

- **不**重新引入两阶段 `PendingRemoval` / `RemovalFlowPolicy` / `HardTimeout`。
- **不**支持跨 Actor 属性夹值引用（`FTcsAttributeRange` 的 Min/Max 属性仍只在单 Component 内解析）。
- **不**新增 `BlueprintNativeEvent` / `UFUNCTION(Blueprintable)` 覆写点；本轮面向 C++ 子类扩展。
- **不**做对象池深度集成（仅约束归还合约，不修改 `TireflyActorPool` 代码）。
- **不**扩展查询 API 为 `virtual`（见下文"关键决策"）。
- **不**重写 StateTree 自 Tick 停机的兜底机制（引擎已自保护，见决策 D-3）。

## 决策

### D-1. Subsystem 降级还是完全删除

**选定方案**：降级为全局门面，保留三类职责：定义缓存加载、全局 ID 工厂、跨 Actor 便捷门面（`TryApplyStateToTarget` / `CreateSourceHandle`）。

**理由**：
- 定义资产的 Asset Manager 加载、DeveloperSettings 合并、Tag→Name 索引是**天然全局**，不应每个 Component 各持一份。
- 全局单调 ID（`StateInstanceId` / `AttributeInstanceId` / `ModifierInstanceId` / `ModifierChangeBatchId`）必须跨 Actor 唯一，Component 无法承担。
- `TryApplyStateToTarget(TargetActor, ...)` 的调用者（技能、AI、投射物逻辑）通常不持有 Target 的 `StateComponent` 引用，提供一个全局门面避免调用方重复写"接口解析→Component 获取→TryApplyState"样板。

**备选方案**：
- *完全删除 Subsystem*：被否决。ID 工厂与资产缓存无合理宿主；`AGameMode`/`GameState` 不适合（TCS 必须能在编辑器预览、PIE、Server/Client 各种场景启动）。
- *改用 `UWorldSubsystem`*：被否决。定义资产跨 World 共享（例如 Streaming Level），GameInstance 作用域更贴切。

### D-2. 查询 API 不设为 virtual

**选定方案**：`GetStatesInSlot` / `GetStatesByDefId` / `GetAllActiveStates` / `HasStateWithDefId` / `HasActiveStateInSlot` 五个查询**不标 `virtual`**，子类不得覆写。

**理由**：
- 查询 API 的正确性依赖 `StateInstanceIndex` 的完整性，而索引维护绑定写路径（`TryAssignStateToStateSlot` / `FinalizeStateRemoval`）。允许子类覆写查询即意味着子类必须同时承担索引一致性契约，扩面不划算。
- 纯读取语义，子类覆写容易"偷偷"加副作用（例如惰性 `RefreshInstances()`），违反"查询不改状态"。
- 写路径（`CreateStateInstance` / `TryApplyState` / `TryApplyStateInstance`）是 virtual，已覆盖合理扩展需求；查询差异化应通过"新 API + 业务层组合"实现。

**备选方案**：
- *全部 `virtual`*：对称性好，但契约复杂度上升、`StateInstanceIndex` 的内部性被打破。

### D-3. StateTree 在 Tick 中停机时不引入额外兜底 Flag

**选定方案**：依赖 UE 5.7 引擎自带的 `FStateTreeExecutionContext::Stop()` 延迟机制（`Exec.CurrentPhase != Unset → Exec.RequestedStop`），TCS 仅增加 `ensureMsgf` 诊断。

**理由**（经引擎源码审计，`StateTreeExecutionContext.cpp:1263`）：
- 引擎已对"Tick / EnterState / Start 中同步调用 Stop"做了原生保护：`RequestedStop` 记账，当前 Phase 结束后由 Tick 循环真正收敛 `ExitState`。
- TCS 若再引入 `bIsInStateTreeCallback` + `bPendingRemoveAllStates` 双 `UPROPERTY` 兜底，会形成**双延迟机制叠加**——引擎延一次、TCS 延一次，时序更难预测且易与未来引擎行为冲突。
- 仍存在的次级风险是 `FinalizeStateRemoval` 的 Step 2-8（包括 `MarkPendingGC`）会同步执行于 Task 的 `ExitState` 之前——这是**时序**问题而非崩溃问题。处理方式：对象池归还合约要求"帧间调用 `RemoveAllStates`"，`ensureMsgf` 在违反时留下诊断。

**备选方案**：
- *保留原 S3 双 Flag 兜底*：被否决。冗余、维护负担、与引擎行为纠缠。
- *完全忽略*：被否决。`ensureMsgf` 成本可忽略且能在违反合约时定位调用栈。

### D-4. 槽位防重入 Flag 的迁移粒度

**选定方案**：`bIsUpdatingSlotActivation` / `PendingSlotActivationUpdates` 从 Subsystem 迁到 `UTcsStateComponent` 实例成员，并使用 `TGuardValue<bool>` 管理置位。

**理由**：
- `UpdateStateSlotActivation` 的 8 步流程**全部**在单 Component 的 `StateSlotsX` 内完成，无跨 Actor 副作用——全局锁是历史设计错误，粒度与操作不匹配。
- `TGuardValue` 保证异常/提前 `return` 路径上 Flag 必定还原，避免"卡死防重入"。

### D-5. Modifier 清理路径

**选定方案**：`FinalizeStateRemoval.Step 5` 使用 `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(SourceHandle)`，**不经** Manager 转发。

**理由**：
- `UTcsStateInstance` 已缓存 `OwnerAttributeComponent`（弱引用），直达成本极低且语义清晰。
- 经 `AttrMgr->RemoveModifiersBySourceHandle(OwnerActor, SourceHandle)` 意味着 Manager 再次走 `ITcsEntityInterface`/`FindComponentByClass` 路径——多一次查找、多一次分叉风险。
- 若 `OwnerAttributeComponent` 弱引用失效（极端 GC 时序），fallback 可使用 `OwnerActor` 解析一次（可选降级），**但主路径必须直达**。

### D-6. 前置 PR 策略（A-2 独立合入）

**选定方案**：`TcsStateRemovalReasons` namespace 引入 + 8 处字面量替换作为**独立前置 PR** 先合入 main，并与 Phase A-1/A-3（文档/注释清理）合批，命名为 `TCS: unify state removal reasons`。

**理由**：
- 纯机械操作、零语义风险，与后续 Phase B-G 零耦合。
- 减少主迁移 PR diff 体积，review 聚焦业务迁移。
- 可独立回滚，不影响主进度。

### D-7. Phase 串行还是并行

**选定方案**：`Phase A（前置 PR） → B → C → D → E → F → G` 严格串行；只有 Phase E 内部三个子项（状态应用 / 槽位链路 / 查询）可在 Phase D 合入后并行推进。

**理由**：
- Phase D 的 `FinalizeStateRemoval.Step 5` 依赖 Phase C 产出的 `UTcsAttributeComponent::RemoveModifiersBySourceHandle`。
- 把该依赖 mock / stub 会使 D 阶段测试结果失真；保留 `AttrMgr->RemoveModifiersBySourceHandle(Owner, ...)` 旧调用违反 §5.3（内部调用必须以 Component 为先），引入一次不必要的"二次替换"。

### D-8. Phase B 自测断言 `checkf` 与 `ensureMsgf`

**选定方案**：`BeginPlay` 末尾使用 `checkf(StateMgr, ...)`（仅 Debug/Development，Shipping 关闭）；运行时 `ResolveXxxManager()` 内部仍使用 `ensureMsgf`。

**理由**：
- Phase B 的预热断言意图是"立即停机、暴露环境问题"，`checkf` 语义合适。
- 运行时 helper 中 `ensureMsgf` 处理的是"已发生的不可恢复情况下的诊断、然后返回 nullptr 让调用方自决"。
- 两者定位不同、场合不同，不应统一。

## 风险 / 取舍

| 风险 | 影响 | 缓解 |
|------|------|------|
| 迁移期 deprecated 包装器被"临时 → 永久" | Manager 与 Component 双实现长期共存，违背完成定义 | 在 G 阶段做"grep + 空文件断言"验收：`git grep DeprecatedFunction Plugins/TireflyCombatSystem` 必须空 |
| Phase C 未合入前 Phase D 阻塞 | 研发并行度下降 | 接受。mock `RemoveModifiersBySourceHandle` 的代价（测试失真 + 二次替换）比串行开销更大 |
| 子类覆写 `FinalizeStateRemoval` 跳过 `StateInstanceIndex` 维护 | 查询 API 返回过期状态 | 在基类方法的文档注释中强制要求"若覆写必须调用 Super 或自行维护索引"；查询 API 保持 non-virtual 避免一错双错 |
| `TGuardValue` 例外路径下 Flag 还原后 `PendingSlotActivationUpdates` 残留 | 下一次 `UpdateStateSlotActivation` 处理已过期的 SlotTag | 在 `DrainPendingSlotActivationUpdates` 入口过滤无效 Slot；并等待开发者手动执行编辑器测试覆盖同帧多次 Apply 合批场景 |
| S1 `IsBeingDestroyed()` 检查漏掉某条回调路径 | Owner 已死但 loop 继续触发 crash | 统一在 `ExpireState` **之后**检查，不在 Step 5/6 内部零散检查；并等待开发者手动执行编辑器测试覆盖 DestroyOwner 典型路径 |
| 外部 C++ 调用者未及时迁移 | 项目编译阶段大量 deprecation warning | 接受。TCS 当前无外部消费者；deprecation warning 本就是预期信号 |

注：本提案不再要求新增或保留专门测试源码；Phase D / Phase E 的关键场景统一改为等待开发者手动执行编辑器测试。

## 迁移计划

**分阶段方案（对应整合版 §7）**：

1. **Phase A（前置 PR + 基线）**：
   - A-1 活跃文档以当前源码为基线
   - A-2 `TcsStateRemovalReasons` namespace + 8 处字面量替换（**独立前置 PR**）
   - A-3 清理遗留注释与 `RemovalStr` 调试残影
2. **Phase B（基础设施）**：Subsystem ID 工厂 public 化、Component `ResolveXxxManager()` helper、`BeginPlay` 末尾 `checkf` 自测。
3. **Phase C（Attribute 迁移）**：属性 CRUD + Modifier 管线下沉；Subsystem 旧 API 变薄转发包装器；内部调用以 Component 为先。
4. **Phase D（StateRemoval + 生命周期迁移）**：移除主链路下沉、`FinalizeStateRemoval` 八步顺序保留、`SetStackCount` / `UpdateActiveStateDurations` 改本地调用、S1 防护。
5. **Phase E（Apply + 槽位 + 查询）**：`TryApplyState` 主流程、槽位激活（含 `TGuardValue` / S2 防护）、查询 API 下沉到 `StateInstanceIndex`。
6. **Phase F（可选临时 deprecated 层）**：若需要分阶段迁移，Subsystem 旧 API 可临时集中在 `#pragma region Deprecated_MigrationOnly` 内，并统一标记 `DeprecatedFunction` / `UE_DEPRECATED`；若直接完成硬删除，则可跳过本阶段。
7. **Phase G（硬删除）**：删除 Deprecated 区域、删除 `friend class UTcsStateManagerSubsystem`、删除活跃文档中旧表述。

**回滚策略**：
- Phase A 的前置 PR 可独立回滚。
- Phase C-E 每个阶段的 PR 独立，任何一步失败可回滚到上一步而不影响已合入阶段。
- Phase G 之前整套 deprecated 包装器仍可用；Phase G 合入后若发现外部调用遗漏，只能前向补迁移（不回滚 G）。

**验收命令与人工步骤**：
```
# 编译
"E:\UnrealEngine\UE_5.7\Engine\Build\BatchFiles\Build.bat" TireflyGameplayUtilsEditor Win64 Development -Project="%CD%\TireflyGameplayUtils.uproject" -WaitMutex

# Phase G 完成度验收（两条均须空输出）
git grep "Deprecated_MigrationOnly" -- "Plugins/TireflyCombatSystem/Source/**/*.h" "Plugins/TireflyCombatSystem/Source/**/*.cpp"
git grep "friend class UTcsStateManagerSubsystem" -- "Plugins/TireflyCombatSystem/Source/**/*.h"
```

并等待开发者在编辑器中手动覆盖 S1 / S2 / S3 场景、StateRemoval / Attribute Modifier / Slot Activation 主路径，以及子类覆写扩展点。

## 开放问题

1. **`GetAttributeComponent(const AActor*)` fallback 的最终去向**：当前实现同时支持 `ITcsEntityInterface` 与 `FindComponentByClass` 兜底。议题完成后只保留前者，是否在 Phase G 完全删除 Subsystem 上的 `GetAttributeComponent`，由调用方自行走 `ITcsEntityInterface`？
   - *倾向*：是。但需确认所有现存调用路径都已迁移。
2. **`UTcsStateInstance::GetOwnerStateComponent()` 是否提升为 `UFUNCTION(BlueprintPure)`**：当前为 C++ getter，迁移后使用频率会上升（`SetStackCount` 等内部调用改走它）。若将来蓝图侧也需使用，是否本轮一并加 `BlueprintPure`？
   - *倾向*：否。本轮不扩面蓝图。
3. **`TryApplyStateToTarget` 跨 Actor 门面中的 Parent SourceHandle 校验强度**：当前仅结构校验，不检查 `ParentSourceHandle.Instigator` 与调用方的关系。是否在门面层加断言？
   - *倾向*：暂不加，保持门面轻薄；由调用者自负。
