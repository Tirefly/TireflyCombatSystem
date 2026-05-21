# 变更：新增 TCS 编辑器 Authoring 入口

## 背景

TCS 已经定义了专用的 DefinitionAsset 类，但当前在编辑器中创建这些资产仍然要经过通用的 `Data Asset` 流程，并额外选择一次父类。这会拖慢 authoring 速度，让插件显得未完成，也使 TCS 的核心定义资产没有自己的插件内分类入口。

最近的 State/Buff 重构也暴露了当前编辑器层的缺口：`UTcsStateDefinition` 现在已经是抽象类，但编辑器 authoring 入口仍直接指向它；与此同时，`UTcsBuffDefinition` 也还没有作为一等创建入口暴露出来。本提案必须把这条修复路径明确写进去，否则插件自带的 authoring 流程仍然会留下损坏或不完整的 DefinitionAsset 面。

本次变更只改善编辑器 authoring 体验，不改变 Def 资产的运行时契约。

同一套编辑器入口也应覆盖那一小部分 TCS 开发者在玩法搭建时经常直接创建的 runtime 资产。如果这些资产没有插件自带入口，那么分类仍然是割裂的，插件自有的 authoring 路径也仍然不完整。

## 变更内容

- 新增 `TireflyCombatSystemEditor` 模块。
- 在 Content Browser 中注册插件自有的 `Tirefly Combat System` 资产分类。
- 将规范的 TCS DefinitionAsset 入口归类到 `Tirefly Combat System -> Definition Asset` 下。
- 为所有应直接在编辑器中创建的 TCS DefinitionAsset 类型提供专用 authoring 入口。
- 确保内建 authoring 面覆盖当前规范 DefinitionAsset 集合：
  - `UTcsAttributeDefinition`
  - `UTcsAttributeModifierDefinition`
  - `UTcsStateSlotDefinition`
  - `UTcsBuffDefinition`
- 为 TCS 开发者应直接创建的 runtime 资产新增 `Tirefly Combat System -> Gameplay Runtime` 子菜单：
  - `UStateTree` preset for `UTcsStateComponent` usage through `UStateTreeComponentSchema`
  - `UStateTree` preset for `UTcsStateInstance` usage through `UTcsStateTreeSchema_StateInstance`
  - Blueprint subclasses of `UTcsSkillInstance`
- 协调当前 `UTcsStateDefinition` 编辑器入口与其抽象运行时状态之间的关系：
  - 如果 `UTcsStateDefinition` 仍保持抽象，就不要把它暴露成一个损坏的直接创建目标
  - 取而代之，应暴露那些真正可 authoring 的具体 state 侧 DefinitionAsset 类型
  - 如果未来 `UTcsStateDefinition` 再次变成可直接 authoring 的类型，那么对应入口必须指向一个可实例化类
- 为每个受支持的 DefinitionAsset 补上专用 `UFactory` 与 `UAssetDefinitionDefault`，使其在 UE 5.6 中具备明确的显示名、分类、颜色和创建行为。
- 对 Gameplay Runtime authoring 入口使用预设工厂配置，避免开发者在受支持的 TCS runtime 目标上再次经历 schema picker 或 parent-class picker。
- 保持所有 TCS Def 资产继续是 `UPrimaryDataAsset` 的子类。本次变更只处理编辑器 authoring 入口，不改变运行时资产基类。
- 保持 Def subclassing 在技术上兼容，但不把它视为主要扩展模型。官方扩展方向仍然是 composition-first，并可在未来演进到 Def fragments。

## 延后跟进与归档门槛

- 当前 `State Component StateTree` 入口有意指向 `UStateTreeComponentSchema`，因为 TCS 目前还没有专用的 `UTcsStateTreeSchema_StateComponent`。
- 如果未来引入了专用的 `UTcsStateTreeSchema_StateComponent`，应在这条 change 中直接把现有 `Gameplay Runtime` 入口改指向它，而不是再额外提出一个单独只做菜单布线的 proposal。
- 在 Skill 侧编辑器 authoring 面成熟到足以并入同一 capability 之前，这条 change 不应归档；至少要等到 `SkillDef` 能作为稳定的资产化 authoring 入口暴露出来。

## 影响范围

- 受影响规范：
  - `def-editor-authoring`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/TireflyCombatSystem.uplugin`
  - new `Plugins/TireflyCombatSystem/Source/TireflyCombatSystemEditor/` module
  - editor-only factories and asset definitions for all supported TCS DefinitionAsset types
  - editor-only runtime asset factories for TCS-owned gameplay authoring entries
- 受影响文档：
  - TCS authoring/setup 文档应把用户引导到插件自有的资产创建路径，而不是通用 `Data Asset` 流程
  - TCS authoring/setup 文档应说明 `Definition Asset` 与 `Gameplay Runtime` 子菜单的划分
