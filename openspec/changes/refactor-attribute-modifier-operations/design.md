## 背景

AttributeModifier 需要同时支持：

1. Instant 一次性结算（伤害、治疗、周期跳字等业务入口的底层原子写入）。
2. Ongoing 可撤销贡献（Buff / 装备 / 天赋等持续效果）。
3. 一次 Definition 内多个稳定 Operation，各自可指向不同 Attribute。

旧模型用 `ModifierMode + single AttributeId + Operands + Execution` 混写这些语义，无法形成可独立验收的新运行时。本 change 在 SourceHandle 工厂与 Attribute 数值底盘已归档后，纵向替换 Definition、Apply、Snapshot、Merger、编辑器验证与调用方。

## 目标

- 唯一 Application 入口与原子提交语义。
- Operation Map 创作与 DataTable 1:1 同步。
- Instant BaseValue / Ongoing CurrentValue 清晰分层。
- Merger / Operator 兼容规则可配置且运行时可防御。
- Ongoing 生命周期统一挂在 StateInstance 上。

## 非目标

- 依赖链自动重算。
- 伤害业务模块。
- 旧资产兼容迁移。
- SkillModifier 架构重做。

## 决策

### 1. Operation 模型

```text
OperandPayload -- OperandEvaluator --> EvaluatedOperand -- Operator --> Attribute write/contribution
```

- AttributeModifier 固定 Numeric / `float`。
- `FInstancedStruct` 只承载 OperandPayload。
- Definition 使用 `TMap<FName, FTcsAttributeOperationSpec>`，Key 为稳定 `OperationId`。
- Operation 保存 `TargetAttributeId`、内建 Operator 或 Custom Operator、Evaluator、OperandPayload。
- Application 只允许覆写 Evaluator 与 Payload，不得覆写 Operator、目标、Priority 或 Merger。
- 所有影响结果 / 调试 / 确定性的 Operation 遍历按 `OperationId` 稳定排序。

### 2. 唯一 Application 入口

```text
ApplyAttributeModifier(Request, OutResult) -> bool
Request:
  ModifierDefId
  ApplicationMode = Instant | Ongoing
  SourceHandle (must be valid)
  OperationOverrides (Evaluator/Payload only)
Result:
  per-Operation success, TargetAttributeId, OldValue, NewValue, SourceHandle, OperationId, failure reason
```

| Mode | 写入层 | 存储 | 事件 |
| --- | --- | --- | --- |
| Instant | BaseValue 原子写入 | 不进入 Ongoing 存储 | 无 Modifier Added/Updated/Removed；成功时发 BaseValue Change |
| Ongoing | CurrentValue 聚合 | 父实例 + Operation 记录 | 生命周期事件按父实例；Attribute Change 只报告最终稳定态 |

- 同一次调用的全部 Operation 共用同一 ApplicationMode。
- 任一 Evaluator / Operator / 目标 / 上下文失败 → 整次零提交。
- Instant 不经 Merger。

### 3. Snapshot 与自排除

- `AttributeEvaluationSnapshot` 只读 TargetAttributeComponent 内任意 Attribute 的 BaseValue / CurrentValue。
- 不隐式读取 Instigator、其他 Actor 或其他 Component。
- Ongoing 初次 Apply：当前父实例尚未入库，天然不在已应用记录中。
- Ongoing 重算：虚拟排除当前父实例全部旧结果，重建 Snapshot；该父实例全部 Operation 共用这份 Snapshot。
- Snapshot 不持久化到 ModifierInstance。
- 本 change 保持受控入口拉取式惰性重算，不实现自动标脏。

### 4. EvaluatorContext

每轮动态构建只读 Context：

- TargetAttributeComponent / Target
- Instigator（由 SourceHandle 派生）
- SourceHandle
- 可空 SourceStateInstance
- 可空 SourceSkillEntry
- `const AttributeEvaluationSnapshot&`

StateTree Task 不向编辑器暴露来源对象；从 Buff/Skill schema 根 Context 读取 SourceStateInstance，Skill 场景再 `GetSkillEntry()`。缺失所需来源时 Application 原子失败，禁止通过 SourceHandle 反查 UObject。

StateParam Operand 使用 `FTcsStateParamOperandPayload` + Numeric Evaluator，读取无参 `GetModifiedValue()`。

### 5. Ongoing 宿主与重复语义

- 所有 Ongoing 必须经由 StateInstance 持有与施加；禁止直接向 TargetAttributeComponent Apply Ongoing。
- 同一 StateInstance 对同一 `ModifierDefId` 最多一次；重复硬拒绝、零修改，Development/Editor Warning。
- 不同 StateInstance 可并存同 DefId Ongoing，由 Merger 处理叠加。
- SkillInstance 只能 Instant，或通过目标本地 StateInstance 间接 Ongoing。
- Instant 允许直接跨 Actor 作用于 TargetAttributeComponent。
- 清理入口：`RemoveOngoingModifiersBySourceHandle(SourceHandle)`。
- `RemoveAttribute`：若任意 Ongoing Operation 以该 Attribute 为目标，硬拒绝且零修改。

### 6. Merger / Operator 兼容

- 内建 Merger 在 Operator 前处理 EvaluatedOperand，不比较最终贡献。
- 推荐兼容表：

| Merger | Allowed Operators |
| --- | --- |
| NoMerge | 全部内建 + Custom |
| UseNewest / UseOldest | 全部内建 + Custom |
| UseMaximum / UseMinimum | Add, MultiplyAdditive, MultiplyCompound |
| UseAdditiveSum | Add；MultiplyAdditive 仅在设置显式允许时按 delta 语义 |
| Custom | 由类默认兼容列表声明，再由设置收紧 |

- `Override + UseMaximum/UseMinimum/UseAdditiveSum` = Forbidden。
- 多 Operation + 内建选择/聚合 Merger：Data Validation 默认 Error；`TcsDeveloperSettings` 可降级为强 Warning。`NoMerge` 始终合法。
- 兼容规则权威来源：`TcsDeveloperSettings`，二元 `Allowed` / `Forbidden`。
- Custom Merger 类自带默认兼容 Operator；设置只能收紧，不能放宽到类未声明的 Operator。
- Apply / Recalculate 必须运行时再检查；编辑器过滤不是唯一防线。

### 7. ID 模型

- 保留 `ModifierInstId`：Ongoing 父实例稳定标识，服务 `ModifierInstIdToIndex` 与按来源批量移除。
- 删除 `ModifierChangeBatchId` / `NextModifierChangeBatchId`。
- 事件分组按“一次 API 调用 = 一次事件边界”处理。
- 同 Priority 稳定 tie-break 若实现需要，可引入轻量 `ApplicationOrderId`；不预先承诺。

### 8. Definition / DataTable

- `FTcsAttributeModifierDefRow` 与新 Def 非标识 UPROPERTY 1:1。
- RowName 承载 AttributeModifierDefId。
- 双向直接赋值；验证 Operation Map 与嵌套 `FInstancedStruct` 深拷贝。
- 旧单 Operation Row / 旧资产不迁移，开发阶段直接重建。

### 9. 删除面

删除或迁移：

- `UTcsAttributeModifierExecution` 与内建 Execution 类
- Def 级 `ModifierType` / `ModifierMode` / `AttributeId` / `Operands`
- `FTcsStateParamBinding` / `OperandBindings`
- `CreateAttributeModifier*` / `ApplyModifier*` 旧入口
- `TcsSTTask_ApplyAttributeModifierToTarget`
- `ModifierChangeBatchId` 相关字段与分配器
- 旧 `TMap<SourceHandle, float> ChangeSourceRecord` 作为精确归因依赖

Owner 侧 StateTree Task 迁移到 `ApplyAttributeModifier` Request 模型。

## 风险 / 取舍

- 范围大但不可横向拆分，否则产生不可用中间态或被迫加兼容层。
- 旧测试资产与 DataTable 必须重建。
- 动态 Merger 下拉过滤若实现成本过高，可后置 UI 增强，但规则与 Validation 必须先落地。
- 归档后的 `add-skill-modifier-runtime-management` 强化了 OperandBindings 旧语义；本 change 必须整体替换，而不是局部补丁。

## 迁移计划

1. 落地 Operation Definition / Payload / Evaluator / Operator 类型与 Def/Row 同步。
2. 实现 Snapshot、ApplicationRequest/Result、ApplyAttributeModifier Instant/Ongoing。
3. 重写 Ongoing 存储索引、Merger 输入语义与兼容设置。
4. 删除旧 Execution / Bindings / Apply API / Target StateTree Task。
5. 迁移 Buff/StateInstance/Skill/Owner Task 调用方。
6. 更新规格、编译、Glue 与定向自动化测试。

## 开放问题

- 无阻塞项。OperationId 命名限制、ApplicationResult 错误码细节、Owner Task 最终 InstanceData 字段布局在实现 tasks 中细化。
