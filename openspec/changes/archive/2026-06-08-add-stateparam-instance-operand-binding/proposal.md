# 变更：新增 StateParamInstance 与 AttributeModifier Operand 动态绑定

## 背景

`bIsSnapshot` 字段已声明但未实现，StateParam 值变化后无法自动更新到 AttributeModifier 的 Operand。当前 StateInstance 使用六容器（Numeric/FName、Numeric/Tag、Bool/FName 等）只存纯值，缺少求值器、CDO 缓存和 Snapshot 策略信息。

## 变更内容

- 新增 `FTcsStateParamInstance` 统一运行时结构，替代 StateInstance 上的六个纯值容器
- 新增 `FTcsStateParamBinding`，在 AttributeModifierInstance 上声明 Operand → StateParam 的绑定
- `TcsStateDefinition::Parameters` Key 从 FName 改为 FGameplayTag，删除 TagParameters
- `RecalculateAttributeCurrentValues` 在执行每个 Modifier 前刷新绑定 Operand
- Operand 刷新采用"拉取"模式：StateParam 变化只更新自身，由 `RecalculateAttributeCurrentValues` 统一拉取

## 影响范围

- 受影响规范：`state-parameter-management`（新增）、`attribute-modifier-runtime`（新增）、`definition-live-registry`（修改）
- 受影响代码：`TcsStateInstance.h/.cpp`、`TcsStateDefinition.h`、`TcsAttributeModifier.h`、`TcsAttributeComponent_Calculation.cpp`、`TcsStateComponent_StateCreation.cpp`、`TcsStateInstance_Parameters.cpp`
- **BREAKING**: `TcsStateDefinition::Parameters` Key 类型变更，删除 `TagParameters`
- **BREAKING**: 删除 StateInstance 上的六个 Param 容器及其存取 API
