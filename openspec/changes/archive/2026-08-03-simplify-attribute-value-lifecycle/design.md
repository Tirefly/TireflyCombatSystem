## 背景

`FTcsAttributeInstance` 当前保存创建时 `InitialValue`，`AddAttribute` 接受 InitValue，`ResetAttribute` 把数值恢复到该值并删除相关 Modifier，`SetAttributeCurrentValue` 可直接写入最终有效数值。该组合把不属于 Attribute Core 的业务恢复语义与 Modifier 生命周期耦合，也让 CurrentValue 不再稳定表示由 BaseValue、Modifier 和 Range 推导出的结果。

本 change 在 AttributeModifier Operation 重构之前收敛 Attribute 底盘，不改变当前旧 Modifier 结构或其 Ongoing 计算模型。

## 目标

- Attribute 实例只保存 BaseValue 与 CurrentValue 两个数值状态。
- Attribute 创建不再拥有或接受业务初始值。
- 删除 Reset 与直接 CurrentValue 写入入口。
- BaseValue 和 CurrentValue 始终使用同一 Range 与 ClampStrategy。
- 有效最大值降低时，两个数值层都稳定截断且不会保存隐藏溢出。

## 非目标

- 不在 Attribute Core 内实现伤害、治疗、复活、存档或升级规则。
- 不在本 change 中引入 Instant / Ongoing AttributeModifier Application API。
- 不重构旧 AttributeModifier Definition、Execution、Operand 或 Merger。
- 不引入不同 ValueLayer 的 ClampContext 或跨 Actor Range 依赖。

## 决策

### Attribute 创建只建立结构

`AddAttribute(AttributeDefId)` 和 `AddAttributeByTag(AttributeTag)` 不再接收数值。它们从 `UTcsDefinitionManagerSubsystem` 解析定义，创建 AttributeInstance，并使用 0 作为内部占位值参与当前 Range Clamp 与依赖传播。这个占位值不是出生值、等级 1 值或可恢复基线。

调用方在创建后必须自行决定 BaseValue：

```text
AddAttribute(AttributeDefId)
SetAttributeBaseValue(AttributeDefId, BusinessCalculatedBaseValue)
```

这使 DataTable、等级、角色模板、存档和复活配置保持在业务层，而不是由 Attribute Core 猜测。

### 删除 Reset 和直接 CurrentValue 写入

`ResetAttribute` 删除，因为 TCS 无法定义“重置”应回到出生、等级、存档还是重生配置，也不能安全地按单一 Attribute 删除未来多 Operation Modifier。

`SetAttributeCurrentValue` 删除。CurrentValue 只能由 BaseValue、当前旧 Modifier 聚合和 Range Clamp 推导。初始化、读档、等级变化和资源恢复应使用 `SetAttributeBaseValue`；伤害、治疗和周期结算将等待 Change 3 的 Instant AttributeModifier 入口。现有测试 Director 的唯一调用迁移为明确的 BaseValue fixture 设置。

### 单一 Range 与 ClampStrategy

对一个 Attribute，BaseValue 和 CurrentValue 必须使用相同 `FTcsAttributeRange` 和同一个 `UTcsAttributeClampStrategy`。动态 Min / Max 只能解析当前 AttributeComponent 上被引用 Attribute 的 CurrentValue，不增加 ValueLayer 选择、跨 Component 查询或跨 Actor 依赖。

### 容量降低永久丢失

当动态最大值下降时，范围传播在同一次收敛中 Clamp BaseValue 和 CurrentValue。例如 `CurrentHealth = 100`、`MaxHealth = 100`，MaxHealth 降至 80 后两个值都为 80；MaxHealth 后续恢复为 100 时，CurrentHealth 和 BaseValue 保持 80。系统不保存 overflow、previous max 或恢复候选值。

## 风险 / 取舍

- 直接写 CurrentValue 的临时调试或测试调用必须迁移到 BaseValue 设置；这是有意消除不稳定 CurrentValue 状态的破坏性变更。
- 创建时 0 占位值在 Range 最小值大于 0 时会立即被 Clamp；业务代码仍必须随后写入实际 BaseValue。
- 旧 Modifier 仍存在时，`SetAttributeBaseValue` 可能使 CurrentValue 因聚合重算而与 BaseValue 不同；这是期望行为。

## 迁移计划

1. 删除 AttributeInstance 初始基线字段和带 InitValue 的构造路径。
2. 删除 AddAttribute / AddAttributeByTag 的数值参数，迁移调用方先创建、再写 BaseValue。
3. 删除 `ResetAttribute` 与 `SetAttributeCurrentValue`，迁移测试 Director。
4. 保证 Add、SetBase 和 Range 传播始终对 Base / Current 使用同一 Range 与 ClampStrategy。
5. 补充无初始基线、动态上限降低和上限恢复不返还溢出的回归测试。

## 开放问题

- 无。
