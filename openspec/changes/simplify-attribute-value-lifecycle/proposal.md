# 变更：收敛 Attribute 数值生命周期

## Why

当前 `FTcsAttributeInstance` 同时保存 `InitialValue`、`BaseValue` 与 `CurrentValue`，`AddAttribute` 接受初始数值，`ResetAttribute` 会同时恢复数值并删除相关 Modifier，`SetAttributeCurrentValue` 又允许直接覆写聚合结果。这些 API 把角色出生、读档、重生、资源恢复、伤害和 Modifier 生命周期混合在 Attribute Core 中，且没有一致的业务语义。

Attribute Core 只应维护可推导的 BaseValue / CurrentValue 和统一 Range Clamp。业务层必须明确决定数值来源与生命周期，后续伤害模块则应使用 Instant AttributeModifier，而不是继续直接写 CurrentValue。

## What Changes

- **BREAKING**：从 `FTcsAttributeInstance` 删除 `InitialValue`、创建时数值构造参数及等价初始基线。
- **BREAKING**：`AddAttribute` 和 `AddAttributeByTag` 不再接收数值；它们只创建、注册 AttributeInstance 并应用 Range Clamp。
- **BREAKING**：删除 `ResetAttribute` 与 `SetAttributeCurrentValue` 的 C++、Blueprint、UnrealSharp 和测试调用入口，不保留兼容包装器。
- 规定业务初始化、读档、等级变化和资源恢复通过 `SetAttributeBaseValue` 写入明确数值；该 API 仍经过统一 CurrentValue 重算与 Range Clamp。
- 固化 BaseValue 与 CurrentValue 使用同一个 `AttributeRange` 和 `UTcsAttributeClampStrategy`；动态 Min / Max 只读取同一 AttributeComponent 的依赖 Attribute CurrentValue。
- 规定临时有效上限降低时，BaseValue 与 CurrentValue 都被截断，超出部分永久丢失，不保存隐藏溢出或在上限恢复时返还。

## Impact

- 受影响规范：`attribute-management`
- 受影响代码：
- `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeInstance.h`
- `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h`
- `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent_AttributeInstance.cpp`
- `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent_RangeConstraints.cpp`
- `Script/ManagedTireflyGameplayUtils/TestTCS/Directors/TestTcsDirectors.cs`
- Attribute 生命周期、范围传播和自动化测试

## Non-Goals

- 不引入多 Operation AttributeModifier、OperandEvaluator、Snapshot 或 Merger 兼容规则。
- 不实现伤害、治疗、复活、读档或资源恢复等业务流程。
- 不引入 `RemoveAttribute` 对多 Operation Ongoing 引用的检查；该规则属于 Change 3。
- 不为现有 Blueprint 资产、DefinitionAsset 或 DataTable 保留旧 API / 初始值 schema 兼容层。
- 不改变后续 AttributeModifier Operation 重构前的旧 Modifier 执行模型。
