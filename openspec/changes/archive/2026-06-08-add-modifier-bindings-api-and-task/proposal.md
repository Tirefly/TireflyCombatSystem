# 变更：新增 CreateAttributeModifierWithBindings + StateTree Task ApplyAttributeModifier

## 背景

`CreateAttributeModifierWithOperands` 要求调用方显式传入完整 Operands，与新的 `OperandBindings` 拉取模型冲突。StateTree Task 配置层缺少将 `ModifierId + OperandBindings → ApplyModifier` 封装为可配置工作流的节点。

`HandleModifierUpdated(...)` 仍为零调用者状态，需要与绑定模型一起收口。

## 变更内容

- 删除 `CreateAttributeModifierWithOperands(...)`（强制全量 Operands 输入，与新绑定模型冲突）
- 新增 `CreateAttributeModifierWithBindings(...)`（接收 OperandBindings，默认 Operands 来自 DefAsset，绑定的从 StateParam 拉取初值）
- 新增 StateTree Task `ApplyAttributeModifier` 节点，封装 ModifierId + OperandBindings → ApplyModifier
- `HandleModifierUpdated(...)` 收口到 StateTree Task 或 StateParamInstance 变化触发路径

## 影响范围

- 受影响规范：`attribute-modifier-runtime`（新增）、`state-parameter-management`（修改）
- 受影响代码：`TcsAttributeComponent.h/.cpp/.Modifiers.cpp`、新建 StateTree Task 节点
- **BREAKING**: 删除 `CreateAttributeModifierWithOperands`
