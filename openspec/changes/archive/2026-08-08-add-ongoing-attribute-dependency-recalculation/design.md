## 背景

Change 3 提供了：

- `ApplyAttributeModifier` 唯一入口
- 父 Ongoing 实例 + AppliedOperations
- Snapshot 自排除
- 受控 `RecalculateAttributeCurrentValues`

仍缺少：依赖值变化 → 自动、惰性、无重入地使受影响 Ongoing 收敛。

既有设计文档：`Documents/后续优化内容/ModifierRedesign/设计：OngoingAttrMod依赖链惰性重算（后续提案）.md`。

## 目标 / 非目标

### 目标

- 外部依赖变更只标脏，不在生产者路径同步递归重算。
- 父 Ongoing 多 Operation 整体 Dirty / 整体原子重算。
- 同帧多次依赖变更合并为一次 Flush。
- Flush 策略 **C（已确认）**：同调用栈事务末尾可同步 Flush；跨调用栈 Dirty 帧末兜底。
- 首版仅自动观察目标 Component Attribute Current + 本地 Buff StateParam effective。
- 已注册父实例形成循环：记录 Error，临时跳过最小循环 SCC，并从剩余有效贡献重建 Current。

### 非目标

- 跨 Actor 全局图、每帧全量 Tick、N 次迭代兜底、SourceHandle 对象注册表。

## 决策

### 1. 依赖模型

```text
DependencyKey:
  AttributeCurrentValue[TargetAttributeComponent, AttributeId]
  StateParamEffectiveValue[SourceBuffInstance, ParamTag]

DependencyRevision: 生产者成功提交真实变化时递增

ParentModifierDependencyRecord:
  ModifierInstId -> DependencyKey[]

ReverseDependencyIndex:
  DependencyKey -> ModifierInstId[]

DirtyOngoingModifierSet:
  等待 Flush 的父实例集合
```

依赖精度：**父实例**，不是单 Operation。

### 2. 收集依赖

- Evaluator 只能经只读 Context / Snapshot 读取可观察值。
- Context 在每次读取时自动登记 DependencyKey（当前重算的父实例）。
- Custom Evaluator 不得绕过 Context 私读 Attribute/StateParam 而不登记依赖（实现上应尽量封死旁路）。

### 3. Flush 策略 C（硬约束）

```text
路径内（Apply / SetBase / Commit / 显式 Recalculate 等受控入口）:
  变更提交 -> 可能 MarkDirty
  -> 路径安全点: 若 Dirty 非空则 FlushDirtyOngoingModifiers()

路径外（StateParam 失效通知、其他系统只 MarkDirty）:
  MarkDirty only
  -> 请求帧末 Flush（合并同帧多次）
  -> 帧末: FlushDirtyOngoingModifiers()

禁止:
  在 Attribute/StateParam 的 setter 或广播回调中同步递归进入完整 Ongoing 重算图
```

Flush 内容：

1. 取出并清空本轮 Dirty 集合（同帧新 Dirty 可再入队）
2. 扩展依赖闭包（被重算父实例写出的 Attribute 可能弄脏下游父实例）
3. Working Snapshot + 自排除
4. 拓扑序 / 稳定序；发现环 → 将最小循环 SCC 加入本事务局部排除集，记录 Error 后重新构图
5. Evaluator / Operator / Merger / Current Clamp 失败 → 按归属粒度加入局部排除集，清空对应候选 AppliedOperations 后重新计算
6. 原子提交注册表候选 AppliedOperations + Clamp + 范围传播 + 最终事件

### 4. 自排除与环

- 重算父实例 P 时，Snapshot 虚拟排除 P 的全部旧贡献（Change 3 已有语义，必须保留）。
- 不同父实例之间的环：检测后失败，不迭代逼近。

### 5. StateParam

- effective 值成功提交且相对上次 Revision 有真实变化 → 递增 Revision → 反向索引 MarkDirty。
- **不得**在 StateParam 路径内调用 `RecalculateAttributeCurrentValues` 全图同步重算。

### 6. 显式回退 API

```cpp
// 名称以实现为准，语义必须是：
RequestOngoingModifierRecalculation(SourceHandle)
// 仅将匹配 SourceHandle 的 Ongoing 父实例加入 Dirty，由 Flush 执行
```

用于未自动观察的依赖（跨 Actor、装备字段等）。

### 7. 与 Change 3 路径关系

| 路径 | 行为 |
|---|---|
| Apply Ongoing / Instant 后 | 可路径末 Flush Dirty（Instant 改 Base 也可能弄脏读该 Base 派生 Current 的 Ongoing——首版观察 Current；Base 变化若影响 Current 聚合需在设计中：Current 重算由 Base+Ongoing 推导，Base 变应触发受影响 Ongoing 的 Operand 重评若 Operand 读了 Snapshot Current/Base） |
| 首版 Attribute 观察 | **CurrentValue**（文档约定）。Base 变化会经现有路径重算 Current；若 Ongoing Operand 只读 Snapshot Current，则 Base→Current 路径末 Flush 即可覆盖 |
| RecalculateAttributeCurrentValues | 可复用为 Flush 内核或由其调用 Flush |

### 8. 规格备注

- 修改对象是 Change 3 归档后的 Ongoing 契约，不是 OperandBindings。
- `attribute-modifier-runtime` 的 Purpose 段仍残留旧 OperandBindings 描述；本 change 可在归档时一并修正 Purpose（实现/归档任务中处理）。

### 9. 单一注册表与无效贡献临时跳过（已确认）

#### 单一 Ongoing 注册表

```text
RegisteredOngoingModifiers:
  保存每个曾成功 Apply 的父实例
  保存 ModifierInstId / ModifierDefId / SourceHandle / OwningState / Overrides
  保存上次成功 DependencyRecords

AppliedOperations 非空:
  该父实例当前有有效贡献，参与 Snapshot、Merger、Operator 与 Current 聚合

AppliedOperations 为空:
  该父实例本轮计算失败且无有效贡献
  仍保留注册身份，并在后续 Attribute 事务重新尝试
```

系统 MUST NOT 为失败父实例新增 Quarantined、Disabled 或 Retry 容器。`CurrentValue` 始终只由 Base 与本轮 `AppliedOperations` 非空的有效父实例推导。

#### 有界有效贡献筛选

每个 Attribute 事务使用仅存在于调用栈内的局部排除 ID 集合：

```text
1. 重新尝试注册表中的全部父实例，包括 AppliedOperations 为空的父实例
2. Initial Apply 新候选若在完整候选图中失败，则整次 Application 零提交
3. 已注册父实例失败时，将对应父实例或不可分割组加入本事务局部排除集
4. 被排除父实例的候选 AppliedOperations 清空，旧数值贡献立即停止
5. 从 Base + 剩余有效候选重新构图、重算与 Clamp
6. 每轮至少新增一个局部排除父实例；同一父实例本事务内不得重复尝试
7. 全部剩余贡献有效后，原子提交候选 AppliedOperations 与最终 Current
```

该流程由注册父实例数量界定终止上限，不是固定 N 次数值迭代。失败诊断使用 Error 日志，不持久保存独立失败记录。

#### 失败归属

| 失败位置 | 临时跳过粒度 |
|---|---|
| Evaluator | 当前父 `ModifierInstId`（多 Operation 整体） |
| Operator（单一来源可追踪） | 对应父 `ModifierInstId`（多 Operation 整体） |
| Operator（多来源合并或来源不明确） | 同一 `ModifierDefId` 的完整 Merger 组 |
| Merger | 同一 `ModifierDefId` 的完整 Merger 组 |
| 依赖循环 | 最小循环 SCC 中的全部父实例 |
| Current Clamp / Range 传播 | 本轮受影响依赖闭包 |
| Base Clamp / Range 配置 | 所有构建使用 Fatal 终止；不得通过跳过 Modifier 掩盖 |

Merger 后的 Operation MUST 携带足以区分单一来源与多来源结果的 provenance。Custom Merger 无法提供可信单一来源时，系统 MUST 将其结果视为完整 `ModifierDefId` 组共同产生。

#### State Step 5 子流程

```text
5a. 将待清理 SourceHandle 的父实例从候选注册表删除
5b. 清理这些父实例的依赖反向索引与 Dirty 记录
5c. 使用 Base + 剩余注册父实例重新执行有效贡献筛选
5d. 失败父实例或不可分割失败组仅在本轮清空候选 AppliedOperations
5e. 原子提交注册表、最终 AppliedOperations 与不含失效来源的 Current
5f. State removal 继续执行 Step 6-8
```

#### 后续事务恢复

- 任意后续 Attribute 变更事务 MUST 重新尝试所有 `AppliedOperations` 为空的已注册父实例，不设置每帧轮询。
- 依赖变化与 `RequestOngoingModifierRecalculation(SourceHandle)` 仍可触发下一次 Flush；Flush 作为 Attribute 事务同样重试全部空贡献父实例。
- 重算成功时直接写回新的 AppliedOperations，并在同一事务重算下游闭包后广播最终事件。
- 重算再次失败时保持 AppliedOperations 为空，仅记录 Error，不广播 Attribute 中间事件。
- Initial Apply 从未成功提交的父实例失败时仍为整次 Application 零提交，MUST NOT 保存空贡献注册项。
- 重复 Apply 检查与 SourceHandle 清理只查询唯一注册表，空贡献父实例仍视为已注册。

## 风险 / 取舍

| 风险 | 缓解 |
|---|---|
| 帧末 Flush 与 GameThread 其他系统顺序 | 明确在 AttributeComponent 可控点注册；文档化相对 Buff Period / StateTree 的顺序开放项若冲突再收紧 |
| 闭包膨胀 | 仅 Dirty 种子 + 写出 Attribute 的下游；限制在单 Component |
| Custom Evaluator 偷读 | Context API 收口；Code review / 规范禁止旁路 |
| 双调度复杂度 | 共享同一 `FlushDirtyOngoingModifiers` 实现 |
| Custom ClampStrategy 未声明完整 Attribute 依赖 | 无法证明依赖闭包与拓扑正确时，原子拒绝本轮 Ongoing Application / Flush；不伪造依赖边或提交可能过期的结果 |
| 持续失败实例在每次 Attribute 事务重复记录 Error | 接受该可观察代价以换取单一注册表；不做每帧轮询，也不自动移除 OwningState |

## 开放问题（实现阶段可细化，不阻塞提案）

- 帧末具体挂点：`FTSTicker` / World Subsystem / Component 延迟任务。
- Revision 是否需要复制/网络同步（首版可仅本地）。
- Blueprint 是否暴露 `RequestOngoingModifierRecalculation`（默认 C++ 优先，BP 可选）。

## 迁移计划

1. 数据与索引结构 + Context 收集。
2. MarkDirty / Flush 内核 + 策略 C 调度。
3. Attribute Current 与 StateParam effective 生产者挂钩。
4. 显式 Request API。
5. 循环检测与诊断。
6. 单一注册表临时跳过、Source 清理与事务重试。
7. 更新 specs、编译验证、人工场景（无自动化测试任务）。
