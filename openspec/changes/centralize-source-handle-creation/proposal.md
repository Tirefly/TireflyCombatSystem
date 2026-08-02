# 变更：统一 SourceHandle 创建入口

## Why

`FTcsSourceHandle` 已经成为 State、Buff、Skill、AttributeModifier 与后续伤害模块共享的因果追踪结构，但当前规格只说明它不再挂在旧 `UTcsAttributeManagerSubsystem` 上，没有明确最终权威创建入口。

现有实现还把 SourceHandle ID 分配放在 `UTcsStateComponent`，这会把共享基础设施错误归属到 State 模块，并使 `Id == 0` 的合法性、Child 因果链构造和 Blueprint 转发边界缺少统一契约。

## What Changes

- 新增共享的 `FTcsSourceHandleFactory`，作为 TCS 内创建有效 `FTcsSourceHandle` 的唯一 C++ 权威入口。
- **BREAKING**：删除或迁移 `UTcsStateComponent::CreateSourceHandle` 与 `UTcsStateComponent::NextSourceHandleId`，不保留 StateComponent 作为 SourceHandle 分配器的兼容入口。
- 固化 `FTcsSourceHandle::IsValid()` 等价于 `Id > -1`，首个生成的 `Id == 0` 合法，业务判断不得手写 `SourceHandle.Id > 0`。
- 提供 Root / Child 创建 API，由工厂统一分配进程内唯一 ID，并由 Child API 继承父链、追加直接父来源 Definition Id。
- 将 `UTcsGenericLibrary` 作为 Blueprint 转发层，不让其拥有 ID 分配状态、对象注册表或独立 authority 语义。
- 明确 SourceHandle 不提供 `HandleId -> UObject` 的全局运行时对象注册表或反查机制。

## Impact

- 受影响规范：`source-handle-runtime`、`attribute-management`、`combat-manager-subsystems`、`state-management`
- 受影响代码：
- `Source/TireflyCombatSystem/Public/TcsSourceHandle.h`
- `Source/TireflyCombatSystem/Private/TcsSourceHandle.cpp`
- `Source/TireflyCombatSystem/Public/TcsGenericLibrary.h`
- `Source/TireflyCombatSystem/Private/TcsGenericLibrary.cpp`
- `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
- `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
- SourceHandle 创建、有效性判断和生命周期清理相关调用点

## Non-Goals

- 不新建 `UTcsSourceHandleSubsystem` 或任何 SourceHandle Manager UObject。
- 不把 SourceHandle 工厂职责放入 `UTcsDefinitionManagerSubsystem`、`UTcsAttributeComponent` 或 `UTcsStateComponent`。
- 不实现网络同步、本地预测、PredictionKey 或 reconcile。
- 不让 SourceHandle 反查 `UTcsStateInstance`、`UTcsSkillEntry`、Buff、装备、伤害来源或任意业务对象。
- 不处理 AttributeModifier Operation 重构；本 change 只提供其后续需要的 SourceHandle 基础契约。
