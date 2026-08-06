## 1. Definition / Operation / DataTable 模型
- [x] 1.1 新增 `FTcsAttributeOperationSpec`、OperandPayload 基类型、Numeric Evaluator 与 Operator（内建枚举 + Custom Operator）类型。
- [x] 1.2 将 `UTcsAttributeModifierDefinition` 改为 `Operations` Map + Merger/Priority/Tags 等定义字段；删除 `AttributeId`、`ModifierMode`、`Operands`、`ModifierType`。
- [x] 1.3 重建 `FTcsAttributeModifierDefRow` 与 Def 非标识字段 1:1，并更新双向 Sync 直接赋值。
- [x] 1.4 删除 `UTcsAttributeModifierExecution` 与内建 Execution 实现，迁移内建运算到新 Operator 路径。
- [x] 1.5 更新 DefAsset / Operation 默认值：`MergerType` 仍默认 `NoMerge`；Operation 默认 Constant Evaluator / Payload；不默认化 Operator / Custom Operator。

## 2. Application 入口、Snapshot 与原子结算
- [x] 2.1 新增 `FTcsAttributeModifierApplicationRequest` / `FTcsAttributeModifierApplicationResult` 与 `ApplyAttributeModifier(Request, OutResult)` 唯一入口。
- [x] 2.2 实现 Instant 路径：校验 SourceHandle、按 OperationId 稳定排序、构建 Snapshot、原子写 BaseValue、失败零提交。
- [x] 2.3 实现 Ongoing 路径：校验 StateInstance 宿主与同 DefId 唯一性、创建父实例、参与 CurrentValue 聚合。
- [x] 2.4 实现 `AttributeEvaluationSnapshot` 与 Ongoing 自排除语义；Snapshot 不持久化到实例。
- [x] 2.5 实现 EvaluatorContext（Target、Instigator、SourceHandle、可选 SourceStateInstance/SourceSkillEntry、Snapshot）。
- [x] 2.6 实现 StateParam OperandPayload Evaluator，读取无参 `GetModifiedValue()`；缺失来源/参数时原子失败。

## 3. Ongoing 存储、Merger 与 ID 模型
- [x] 3.1 重写 Ongoing 父实例存储与索引：保留 `ModifierInstId`、`SourceHandleIdToModifierInstIds`、`ModifierInstIdToIndex`。
- [x] 3.2 删除 `ModifierChangeBatchId` / `NextModifierChangeBatchId` / `LastTouchedBatchId` 及相关事件批次语义。
- [ ] 3.3 重写 Merger 输入为 EvaluatedOperand；Instant 不经 Merger。
- [ ] 3.4 实现 `TcsDeveloperSettings` Operator/Merger 兼容规则（Allowed/Forbidden）与 Custom Merger 默认兼容收紧语义。
- [ ] 3.5 Apply / Recalculate 运行时执行兼容检查；多 Operation + 内建选择/聚合 Merger 默认 Error，设置可降 Warning。
- [x] 3.6 实现 `RemoveOngoingModifiersBySourceHandle` 与来源结束后的清理路径。
- [x] 3.7 `RemoveAttribute` 在被任意 Ongoing Operation 引用时硬拒绝且零修改。

## 4. 事件、生命周期与调用方迁移
- [x] 4.1 Instant 不广播 Modifier Added/Updated/Removed；成功时广播 BaseValue Change，并用 ApplicationResult 提供逐 Operation 审计。
- [x] 4.2 Ongoing Attribute Change Event 只报告 Clamp/范围传播后的最终稳定态。
- [x] 4.3 约束所有 Ongoing 必须由 StateInstance 持有与施加；SkillInstance 禁止直接 Ongoing。
- [x] 4.4 迁移 Buff/StateInstance 施加与清理路径到新 Apply/Remove 入口。
- [x] 4.5 删除 `TcsSTTask_ApplyAttributeModifierToTarget`；迁移 Owner 侧 Apply Task 到新 Request 模型。
- [x] 4.6 删除旧 Create/Apply/Bindings API，并清理 C++ / Blueprint / UnrealSharp / 现有调用方。

## 5. 编辑器验证与创作体验
- [ ] 5.1 更新 AttributeModifierDef `IsDataValid` / `PostEditChangeProperty`：Operator/Merger Forbidden、多 Operation Merger、缺失 Evaluator/Payload 等。
- [ ] 5.2 在 `TcsDeveloperSettings` 落地兼容规则配置结构，并让 Def validation 读取同一权威来源。
- [ ] 5.3 若实现成本可控，为先设 Operator 后设 Merger 的下拉过滤补 Detail Customization；否则先保证 Data Validation + 运行时防御。
- [ ] 5.4 验证 Operation Map 与嵌套 `FInstancedStruct` 在 DataTable ↔ DefAsset 同步中可编辑并完整深拷贝。

## 6. 构建与规范验证
- [x] 6.6 执行 `openspec validate refactor-attribute-modifier-operations --strict --no-interactive`。
- [x] 6.7 编译 `TireflyGameplayUtilsEditor Win64 Development`，并编译受影响 Glue / Managed 脚本工程。
