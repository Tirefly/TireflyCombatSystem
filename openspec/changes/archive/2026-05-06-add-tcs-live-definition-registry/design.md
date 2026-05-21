## 背景

当前 TCS 的编辑器期加载路径把三类关注点混进了 `UTcsDeveloperSettings`：

- 静态配置（`StateLoadingStrategy`、通用路径）
- 临时 Def 缓存
- 编辑器事件绑定与刷新时机

这是一种错误的归属模型。`UTcsDeveloperSettings` 是一个设置对象，而不是实时定义注册表。与此同时，两个管理子系统当前只在 settings 中做一次初始化读取，然后各自持有自己的副本，因此它们无法自然感知后续编辑器期的 Def 变化。

## 目标 / 非目标

- 目标：
  - 让编辑器中的 Def 变化在同一会话内即可被观察到。
  - 确保 `Attribute`、`State`、`StateSlot` 与 `AttributeModifier` Def 的变化都收敛到一条权威刷新路径上。
  - 确保两个管理子系统都能从最新快照重建，而不需要重启编辑器。
  - 通过基类契约而非精确类相等来发现资产，从而保持 Def 子类兼容性。
  - 让系统对未来的 `Def + Fragment[]` 组合模型保持中立。
- 非目标：
  - 本次不实现 fragment。
  - 本次不强制移除 Def 子类兼容性。
  - 本次不处理资产创建体验；这应归属到 `add-tcs-def-editor-authoring` change。

## 决策

- 决策：在运行时模块中引入 `UTcsDefinitionRegistrySubsystem`，作为权威快照持有者。
  - 原因：即使运行在编辑器内部，定义注册表也必须对运行时模块中的管理子系统可见。把定义注册表放到仅编辑器模块会产生错误的依赖方向。

- 决策：`UTcsDeveloperSettings` 变为配置对象 + 镜像门面，而不是刷新编排器。
  - 原因：settings 仍然需要暴露配置和面向兼容性的缓存视图，但实时刷新逻辑不应继续放在那里。

- 决策：定义注册表刷新要做合并收敛，并通过单一变更事件发布快照。
  - 原因：编辑器里的资产操作经常成批出现（`in-memory create`、save/update、rename、settings change）。一条单一的权威刷新路径，比许多零散的增量修改更安全。

- 决策：管理子系统订阅定义注册表刷新，并从发布的快照中重建内部 map。
  - 原因：这满足了两边 subsystem 在同一编辑器会话里始终“知道”最新 Def 状态的要求。

- 决策：资产发现基于 `IsChildOf` / 基类契约，而不是精确类路径相等。
  - 原因：TCS 不应把 Def subclassing 推荐为主要模型，但它必须继续兼容那些用于项目级校验、默认值或狭义扩展的 Def 子类。

## 提议架构

### 定义注册表归属

- 在 TCS 运行时模块中新增 `UTcsDefinitionRegistrySubsystem`。
- 该定义注册表持有以下类型的权威编辑器期快照：
  - Attribute Defs
  - Attribute Modifier Defs
  - State Defs
  - StateSlot Defs
- 快照存储在按规范 Def ID 作为键的基类类型 map 中。

### 编辑器刷新触发器

在编辑器构建中，定义注册表监听以下事件：

- `OnAssetAdded`
- `OnAssetUpdated`
- `OnAssetRemoved`
- `OnAssetRenamed`
- `OnInMemoryAssetCreated`
- `OnInMemoryAssetDeleted`
- relevant `UAssetManagerSettings::OnSettingChanged()` updates
- initial `AssetManager` scan completion or equivalent initialization point

定义注册表不会在每个回调里临时直接修改 map。相反，各回调只请求一次带去抖的刷新，而定义注册表会在下一个安全 tick 上重建一份权威快照。

### 变更后的 DeveloperSettings 角色

- `UTcsDeveloperSettings` 仍然持有静态配置。
- `UTcsDeveloperSettings` 不再决定刷新何时发生。
- 出于兼容目的，每次定义注册表刷新后，最新快照都会镜像到 `UTcsDeveloperSettings` 的缓存视图中。
- 在迁移窗口内，仍然读取 `GetCached*Definitions()` 的既有代码可以继续工作。

### Subsystem 同步

- `UTcsAttributeManagerSubsystem` 订阅定义注册表刷新事件，并重建：
  - `AttributeDefinitions`
  - `AttributeModifierDefinitions`
  - tag/name lookup maps
- `UTcsStateManagerSubsystem` 订阅定义注册表刷新事件，并重建：
  - `StateSlotDefinitions`
  - eager `StateDefinitions` if strategy requires it
  - tag/DefId lookup maps
- 重建在游戏线程上同步执行，并由已发布的定义注册表快照驱动。

## 风险 / 取舍

- 风险：在每个相关编辑器事件上都重建，可能会过于频繁。
  - 缓解：合并刷新请求，并在每一波事件中只发布一次权威刷新。

- 风险：在 `UTcsDeveloperSettings` 中保留镜像缓存看起来可能显得冗余。
  - 缓解：明确把它们视为兼容镜像，而不是归属状态。

- 风险：派生 Def 资产可能定义与基础资产冲突的重复 ID。
  - 缓解：无论具体资产子类是什么，定义注册表校验都应把重复的规范 Def ID 视为错误。

## 迁移计划

1. 引入新的定义注册表子系统与快照模型。
2. 将编辑器期监听器绑定迁入定义注册表。
3. 将快照镜像回 `UTcsDeveloperSettings`。
4. 更新两个管理子系统，让它们从定义注册表重建并订阅刷新通知。
5. 清除任何残留的“只支持精确类”的发现假设。

## 开放问题

- 定义注册表是否应立即暴露 revision/generation 计数器，以帮助 gameplay 系统识别过期的本地视图。
- 如果编辑器抖动非常频繁，延后刷新究竟该使用下一次 tick 调度，还是专门的去抖 ticker。
