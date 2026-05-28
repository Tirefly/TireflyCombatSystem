# 变更：为编辑器期感知新增实时 TCS Definition Registry

> 归档说明：本目录是历史快照，不是当前事实的最高优先级来源。
> 若本文与 `openspec/specs/` 或活动 `openspec/changes/` 冲突，以当前 spec / 活动 change 为准。

## Why

TCS 当前依赖 `UTcsDeveloperSettings` 作为临时编辑器缓存，并依赖 subsystem 的一次性初始化读取。这个模型无法可靠处理同一编辑器会话中的新建、重命名、删除或修改后的 Def 资产，而且它目前还依赖精确类匹配，这会悄悄漏掉派生 Def 资产。

本次变更引入一个真正的 definition registry，使编辑器期的 Def 变化可被观察到，并让两个 TCS manager subsystem 都能从最新快照中重建，而不需要重启编辑器。

## What Changes

- 引入专用的 `UTcsDefinitionRegistrySubsystem`，作为实时 Def 快照的权威持有者。
- 将编辑器期的变更检测与刷新编排从 `UTcsDeveloperSettings` 中移出。
- 让 `UTcsDeveloperSettings` 保持为配置对象与兼容门面，但不再作为刷新逻辑的归属者。
- 在需要兼容的地方，将最新 registry 快照镜像回 `UTcsDeveloperSettings`。
- 让 `UTcsAttributeManagerSubsystem` 与 `UTcsStateManagerSubsystem` 消费 registry 快照，并对 registry 刷新事件作出反应。
- 在所有与编辑器相关的变化上触发刷新，包括新建、保存/更新、重命名、删除、内存中资产的创建/删除，以及相关 `AssetManager` 设置变更。
- 按基类契约发现 TCS Def 资产，从而保持 Def 子类兼容性；State 侧通过抽象 `UTcsStateDefinition` 基类纳入具体定义资产，例如 `UTcsBuffDefinition`。
- 保持未来官方扩展方向仍然是 composition-first（`Def + Fragment[]`），但 registry 本身对项目使用 fragments、基础 Def 还是狭义 Def 子类保持中立。

## Impact

- 受影响规范：
  - `definition-live-registry`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/TcsDeveloperSettings.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/TcsDefinitionRegistrySubsystem.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/TcsDefinitionRegistrySubsystem.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeManagerSubsystem.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Attribute/TcsAttributeManagerSubsystem.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
- 受影响验证/文档：
  - 编辑器期 Def authoring 与刷新的手动编辑器测试流程
  - 与 registry 刷新、subsystem 重建相关的实现说明
