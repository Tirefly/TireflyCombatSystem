# 变更：收紧 TCS 运行时访问契约并移除泛化 StateInstance Schema

## 背景

`refactor-tcs-state-buff-skill-split` 已经把 State / Buff / Skill 的职责边界收口到一个更明确的方向：

- `UTcsStateInstance` 更像共享执行态基类，而不是长期 concrete 业务运行时类型
- Buff 和 Skill 才是当前已经明确存在的 concrete 业务运行时语义面
- `UTcsStateComponent` 对应的是宿主级编排树，而不是 generic `StateInstance` 的替身

在这个前提下，继续保留 `UTcsSTSchema_StateInstance` 作为 editor-facing concrete schema，会把抽象执行基类再次伪装成具体业务运行时类型。这和新的架构收敛方向冲突。

同时，Skill 侧命名也需要一起翻面：

- 当前 `UTcsSkillInstance` 实际表示 SkillComponent 托管的 learned skill 数据对象
- 真正代表一次技能激活执行态的运行时类，应该是新的 `UTcsSkillInstance : UTcsStateInstance`
- 当前 learned skill 数据对象应更名为 `UTcsSkillEntry`

因此，本提案不再以“保留 `StateInstance` 最小 concrete schema”为目标，而是转向：

- 把 `UTcsStateInstance` 固化为抽象执行态基类
- 删除 `UTcsSTSchema_StateInstance`
- 只保留 concrete 业务 owner 明确的三条 schema 线：
  - `UTcsSTSchema_StateComponent`
  - `UTcsSTSchema_Buff`
  - `UTcsSTSchema_Skill`

## 变更内容

- 将 `UTcsStateInstance` 收敛为抽象共享执行态基类，不再让它直接作为 concrete 业务 StateTree 容器进入长期契约。
- 删除 `UTcsSTSchema_StateInstance`，不再保留 generic `StateInstance` 的 concrete editor-facing schema 入口。
- 将组件树 schema 命名统一为 `UTcsSTSchema_StateComponent`：
  - 根上下文为 `StateComponent`
  - 继续覆盖当前 `State Component StateTree` 的临时用途
  - 保留 `LinkSubTree` / `LinkedSubTree` 兼容方向
- 将 Buff 树 schema 命名统一为 `UTcsSTSchema_Buff`：
  - 根上下文只暴露 `BuffInstance`
  - 不复用已被删除的 generic `StateInstance` schema
- 将 Skill 树 schema 命名统一为 `UTcsSTSchema_Skill`：
  - 根上下文为 `SkillInstance + SkillEntry`
  - 其中 `SkillInstance` 表示本次技能激活执行态
  - `SkillEntry` 表示 SkillComponent 托管的 learned skill 数据对象
- 将当前 `UTcsSkillInstance` 重命名为 `UTcsSkillEntry`，保留其“已学会技能数据载体”的职责。
- 新增运行时 `UTcsSkillInstance : UTcsStateInstance`，使 Skill 执行态与 Buff 执行态在命名层级上保持一致。
- 保持 `UTcsStateDefinition` 作为抽象共享定义层，不强行拆散已有共享字段与 `ResolveStateInstanceClass()` 合同。
- 为 `UTcsBuffDefinition` 新增 `TSubclassOf<UTcsBuffInstance> BuffInstanceClass`，并重写共享的运行时实例解析入口。
- 新增 `UTcsSkillDefinition`：
  - 提供 `TSubclassOf<UTcsSkillInstance> SkillInstanceClass`
  - 提供 `TSubclassOf<UTcsSkillEntry> SkillEntryClass`
  - 使 Skill 的执行态类与 learned 数据类都回到 SkillDef 这个静态配置层统一声明
- 保留并沿用 `ITcsEntityInterface` 的稳定组件访问面，确保 Buff / State / Skill 三条运行时线继续能解析宿主组件协作关系。

## 影响范围

- 受影响规范：
  - `state-runtime-access`
  - `combat-entity-surface`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Buff/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Buff/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Skill/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Skill/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/StateTree/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/StateTree/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/TcsEntityInterface.h`
- 与现有变更的关系：
  - 延续 `refactor-tcs-state-buff-skill-split` 已确认的 State / Buff / Skill 运行时边界，但把 `StateInstance` 从 concrete 业务运行时进一步收回到抽象执行基类。
  - 响应 `add-tcs-def-editor-authoring` 中的组件树 schema 债务，但不会在本 change 内顺手完成全部 editor 菜单 / `UFactory` / `UAssetDefinition` 改线。
- 明确不在本提案范围内：
  - 完整实现 Skill 模块业务逻辑、持久化、复制或激活流程
  - 自动迁移全部现有 StateTree 资产绑定
  - 一次性完成所有 `LinkedSubTree` 兼容改造
