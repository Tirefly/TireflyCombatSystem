# Attribute 模块运行时 Modifier 动态操作数与应用链改造分析

## 1. 文档定位

这份文档只聚焦 `Attribute` 模块当前已经存在的运行时 `Modifier` 能力、现阶段的真实边界，以及后续可落地的优化方向。

本文不包含编辑器测试搭建步骤，也不讨论 `State / Buff / Skill` 的完整测试方案。

## 2. 当前代码真相

### 2.1 DefAsset 不是唯一操作数来源

当前 `UTcsAttributeModifierDefinition` 仍然把 `Operands` 作为默认模板配置保存在 DefAsset 中，且构造与校验都要求至少存在 `Magnitude` 键。

但真正参与执行与合并的，不是 DefAsset 本身，而是运行时实例 `FTcsAttributeModifierInstance` 上的 `Operands`。

这意味着：

1. DefAsset 中的 `Operands` 只是默认值模板。
2. 运行时实例可以持有与 DefAsset 不同的操作数。
3. 执行器与合并器都已经能够消费运行时覆写后的值。

### 2.2 运行时动态操作数能力已经存在

当前 `UTcsAttributeComponent` 已经提供两条创建路径：

1. `CreateAttributeModifier(...)`
   - 从 DefAsset 复制默认 `Operands` 到实例。
2. `CreateAttributeModifierWithOperands(...)`
   - 直接用调用方提供的 `Operands` 生成实例。

同时，组件还提供了：

1. `ApplyModifier(...)`
2. `ApplyModifierWithSourceHandle(...)`
3. `GetModifiersBySourceHandle(...)`
4. `HandleModifierUpdated(...)`

因此，“运行时给某个 AttributeModifier 动态设置操作值”这件事，在 `Attribute` 底层并不是不存在，而是已经具备基础能力。

### 2.3 执行器与合并器确实读取实例 Operands

当前内置执行器：

1. `UTcsAttrModExec_Addition`
2. `UTcsAttrModExec_MultiplyAdditive`
3. `UTcsAttrModExec_MultiplyContinued`

读取的都是 `ModInst.Operands["Magnitude"]`。

当前合并器中，`UTcsAttrModMerger_UseAdditiveSum` 也会基于实例上的 `Magnitude` 做合并求和，而不是回头读取 DefAsset 默认值。

因此，只要实例的 `Operands` 在应用前被正确写入，运行时结果就会按覆写值生效。

## 3. 当前主应用链的问题

### 3.1 常用便捷入口仍然偏向 DefAsset 固定值

虽然 `CreateAttributeModifierWithOperands(...)` 已经存在，但当前最显眼、最顺手的便捷入口仍然是：

`ApplyModifierWithSourceHandle(const FTcsSourceHandle& SourceHandle, const TArray<FName>& ModifierIds, TArray<FTcsAttributeModifierInstance>& OutModifiers)`

这条 API 只接收 `ModifierIds`，不会让调用方传入运行时 `Operands`。其内部走的仍然是 `CreateAttributeModifier(...)`，即“从 DefAsset 复制默认值”。

这会带来两个后果：

1. 底层虽然支持动态操作数，但主调用面没有把它暴露成默认使用路径。
2. 上层业务很容易继续沿着“一个数值一个 DefAsset”的方式扩张资产数量。

### 3.2 当前动态能力更像底层 API，不像稳定工作流

从当前代码使用痕迹看：

1. `CreateAttributeModifierWithOperands(...)` 有声明和实现。
2. `HandleModifierUpdated(...)` 有声明和实现。
3. 但当前仓库中没有明显的上层业务调用点把这两条能力接成稳定 authoring/workflow。

这说明系统现状更接近：

“底层已经支持运行时 operand 覆写，但上层主链和文档仍默认 DefAsset 固定值模型。”

### 3.3 当前值修改器与基础值修改器的持久化语义并不对称

这是当前最容易被忽略、但影响改造方向的关键边界。

`ApplyModifier(...)` 内部会先把输入实例拆成两类：

1. `AMM_BaseValue` -> `ModifiersToExecute`
2. `AMM_CurrentValue` -> `ModifiersToApply`

其中：

1. `BaseValue` 修改器会立即参与一次 `RecalculateAttributeBaseValues(...)`。
2. `CurrentValue` 修改器会进入持久化数组 `AttributeModifiers`，随后参与 `RecalculateAttributeCurrentValues(...)`。

这意味着当前系统的真实语义是：

1. `CurrentValue` 修改器是持久运行时实例，支持查询、更新、按 `SourceHandle` 移除。
2. `BaseValue` 修改器更像一次性输入，不天然具备同等级别的持久生命周期语义。

因此，如果目标只是“动态速度加成、攻击力加成、护盾变化”等典型当前值效果，现有底层能力已经够用。

但如果目标是“可撤销的动态 MaxHealth Buff”这类基础值 modifier，当前实现并不完整，不能直接当作稳定能力使用。

## 4. SourceHandle 相关语义现状

当前 `State` 生命周期已经与 `Attribute Modifier` 的清理挂钩。

当 `StateInstance` 结束时，`UTcsStateComponent::FinalizeStateRemoval(...)` 会调用：

`RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle())`

这说明：

1. 当前系统设计上已经认可“Modifier 生命周期绑定到 SourceHandle”。
2. 如果未来要让运行时动态 operand 成为 `Buff / State / Skill` 的常规用法，应该继续沿用这条 SourceHandle 管理链，而不是另起一套独立清理逻辑。

## 5. 当前架构的核心判断

### 5.1 已经存在的能力

当前 Attribute 模块已经具备以下能力：

1. Modifier 实例持有独立 `Operands`。
2. 运行时可以创建带自定义 `Operands` 的 Modifier 实例。
3. 当前值 Modifier 支持按 `SourceHandle` 查询、更新、移除。
4. 执行器和合并器都按实例值工作。

### 5.2 当前缺失的不是底层执行，而是上层收口

当前缺失的主要不是“底层是否能动态算值”，而是：

1. 主调用入口没有把 operand override 暴露成默认工作流。
2. 上层 `Buff / State / Skill` 配置面没有把“模板 Def + 运行时覆写”明确成标准模式。
3. `BaseValue` modifier 的长期生命周期语义还没有补完整。

## 6. 优化目标

Attribute 模块的后续优化应以以下目标为准：

1. 保留 DefAsset 作为模板，而不是继续把所有数值变化硬编码成独立资产。
2. 让运行时 operand override 成为一等能力，而不是隐藏在底层 API 中。
3. 统一 `Buff / State / Skill` 对 AttributeModifier 的应用入口，避免上层各自拼装实例。
4. 明确 `CurrentValue` 与 `BaseValue` modifier 的生命周期差异，不要在语义未完成前把两者混成同一承诺。

## 7. 推荐改造方向

### 7.1 第一阶段：把 DefAsset 固定值模型升级为“模板 + 覆写”模型

推荐新增一个显式的应用规格结构，例如：

`FTcsAttributeModifierApplySpec`

建议至少包含：

1. `FName ModifierId`
2. `TMap<FName, float> OperandOverrides`

其中语义应明确为：

1. `ModifierId` 负责选中模板 DefAsset。
2. 运行时先复制 DefAsset 的默认 `Operands`。
3. 然后仅对 `OperandOverrides` 中出现的键执行覆盖。

这样做比直接要求上层传完整 `Operands` 更合理，因为：

1. 调用方只需要关心要改的那几个值。
2. DefAsset 仍然可以提供默认键集合和默认值。
3. 未来执行器新增更多 operand key 时，上层调用不至于全量改签名。

### 7.2 第一阶段的主 API 建议

建议在 `UTcsAttributeComponent` 上新增一条新的统一入口，例如：

`ApplyModifierSpecsWithSourceHandle(...)`

职责是：

1. 接收 `SourceHandle`
2. 接收一组 `FTcsAttributeModifierApplySpec`
3. 内部完成“Def 默认值复制 + override 覆盖 + 实例创建 + ApplyModifier”

同时保留现有 API，但重新定位：

1. `CreateAttributeModifier(...)`：兼容旧逻辑 / 简单场景。
2. `CreateAttributeModifierWithOperands(...)`：底层原子能力。
3. `ApplyModifierWithSourceHandle(...)`：兼容旧接口，但不再作为推荐主路径。

### 7.3 第一阶段的收益

完成这一阶段后，可以直接复用同一个 DefAsset 去表达：

1. 不同等级技能带来的不同攻击力加成。
2. 不同 Buff stack 数量折算出的不同移动速度修正。
3. 同一种技能在不同施法者面板属性下计算出的不同最终 Magnitude。

这样可以显著减少“`Pct10 / Pct20 / Pct30 / Pct40` 各配一个 ModifierDef”这类资产膨胀。

## 8. 第二阶段：显式化运行时更新语义

当前系统虽然已经能通过以下方式更新已存在的 CurrentValue Modifier：

1. 重新构造实例
2. 复用相同 `SourceHandle`
3. 复用相同 `ModifierId`
4. 在 `ApplyModifier(...)` 内触发已有实例的 `Operands` 覆盖

但这条语义太隐式，不适合长期当标准接口。

推荐补一条显式更新 API，例如：

1. `UpdateModifierSpecsBySourceHandle(...)`
2. 或 `RefreshModifiersBySourceHandle(...)`

它的职责应是：

1. 明确表示“这是对已存在 modifier 的刷新/改值”，不是一次新的 apply。
2. 内部统一走 `GetModifiersBySourceHandle(...)` + operand 覆写 + `HandleModifierUpdated(...)`。
3. 对事件广播、时间戳、批次号保持现有 Attribute 组件语义。

这样 Buff stack 变化、技能蓄力变化、周期性强度变化，都可以走显式更新路径，而不是依赖隐藏的“同 SourceHandle + 同 ModifierId 自动覆盖”行为。

## 9. 第三阶段：补齐 BaseValue Modifier 的生命周期模型

这一阶段不应与前两阶段混做，因为它不是简单 API 暴露，而是语义补完。

如果后续确认要支持如下场景：

1. 动态 MaxHealth Buff
2. 可撤销的基础攻击力模板修正
3. 其他需要跟随 SourceHandle 生命周期离场的 BaseValue 修饰

那么当前实现需要继续演进。

原因是：

1. `BaseValue` modifier 现在是即时参与一次 base 重算。
2. 它没有像 `CurrentValue` modifier 那样稳定进入持久化实例集合。
3. 因此它不天然具备同级别的查询、更新、移除能力。

推荐方向有两个，只能二选一，不要混合：

### 方案 A：统一持久化

把 `BaseValue` modifier 也纳入持久实例集合，只是在计算阶段区分 base/current 两种执行顺序。

优点：

1. 生命周期模型统一。
2. `SourceHandle` 查询、更新、移除全部复用同一套机制。
3. 对上层 `Buff / Skill` 来说更直观。

代价：

1. 需要重整当前 `ApplyModifier(...)` 的分流语义。
2. 需要重新审视 base/current 两阶段重算的性能与事件边界。

### 方案 B：双集合持久化

单独维护 `PersistentBaseModifiers` 与 `AttributeModifiers` 两套运行时集合。

优点：

1. 语义清晰，计算阶段更容易独立控制。

代价：

1. 索引、更新、移除、广播、调试接口都要维护两套。
2. 复杂度明显更高。

在当前代码基础上，更推荐方案 A，因为现有查询/移除/事件/SourceHandle 体系已经围绕“单一 modifier 实例模型”建立，没必要主动制造双系统。

## 10. 推荐实施顺序

建议按以下顺序推进，而不是一次性大改：

### Phase 1

1. 明确 DefAsset 是模板，不是唯一数值来源。
2. 新增 `FTcsAttributeModifierApplySpec`。
3. 新增 `ApplyModifierSpecsWithSourceHandle(...)`。
4. 保留现有旧接口作为兼容层。

### Phase 2

1. 新增显式更新入口。
2. 统一把运行时数值刷新从隐式覆盖迁移到显式更新 API。

### Phase 3

1. 只在确认业务确实需要“可撤销 BaseValue 动态 modifier”后，再补齐 BaseValue 生命周期模型。

## 11. 当前阶段的实践建议

如果现在只是要支撑不同 Buff 效果、不同 Skill 逻辑复用同一个 AttributeModifierDef，那么当前最值得做的不是推翻 Attribute 模块，而是：

1. 继续保留少量模板型 ModifierDef。
2. 不再继续增加大量仅数值不同的 DefAsset 变体。
3. 尽快把“运行时 operand override”提升为主入口能力。

如果当前需求主要是：

1. 速度百分比
2. 攻击力加值
3. 护盾值变化
4. 其它典型 CurrentValue 效果

那么这条优化链已经足够明确，可以直接进入设计与实施。

如果当前需求核心是：

1. 动态 MaxHealth
2. 可撤销基础属性模板修正

那就不应该假装只是小优化，而应单独立项处理 `BaseValue Modifier` 的持久生命周期问题。

## 12. 最终结论

结论只有三条：

1. 当前代码里已经存在运行时动态设置 AttributeModifier 操作值的底层能力。
2. 当前真正缺的是上层主应用链与明确工作流，而不是底层执行器能力。
3. 若只针对 `CurrentValue` modifier 做优化，改造成本可控且收益很高；若要进一步支持可撤销的 `BaseValue` 动态 modifier，则应作为下一阶段独立设计议题处理。