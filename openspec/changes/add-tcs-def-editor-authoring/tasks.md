## 1. 实现

- [x] 1.1 在 `TireflyCombatSystem.uplugin` 中将 `TireflyCombatSystemEditor` 作为编辑器模块加入。
- [x] 1.2 创建 `TireflyCombatSystemEditor` 模块源码树，以及启动/关闭注册流程。
- [x] 1.3 注册 `Tirefly Combat System` Content Browser 资产分类。
- [x] 1.4 为 AttributeDef、AttributeModifierDef、StateDef、StateSlotDef 实现 `UFactory`。
- [x] 1.5 为四个核心 Def 资产类型实现 `UAssetDefinitionDefault`。
- [x] 1.6 确保所有内建编辑器入口都直接指向可实例化的规范 DefinitionAsset，而不是通用 `Data Asset` 父类选择器。
- [x] 1.7 补上缺失的 `UTcsBuffDefinition` 工厂与资产定义入口。
- [x] 1.8 调整抽象 `UTcsStateDefinition` 的编辑器入口，使其不再指向不可实例化类。
- [x] 1.9 将所有规范 DefinitionAsset 创建入口归入 `Tirefly Combat System -> Definition Asset`。
- [x] 1.10 为组件 StateTree、StateInstance StateTree 与 SkillInstance Blueprint 创建补上 `Gameplay Runtime` authoring 入口。
- [x] 1.11 更新文档，说明拆分后的 TCS authoring 子菜单布局以及新的 gameplay runtime 入口。

## 2. 验证

- [x] 2.1 验证每个受支持的具体 DefinitionAsset 类型都出现在插件自有资产分类下。
- [x] 2.2 验证创建每个受支持的 DefinitionAsset 时，都不再需要通用 `Data Asset` 的父类选择步骤。
- [x] 2.3 验证创建出的资产仍返回预期的 `PrimaryAssetId`，并继续兼容既有运行时加载路径。
- [x] 2.4 验证没有任何编辑器入口会指向抽象 DefinitionAsset 类。
- [x] 2.5 运行 `openspec validate add-tcs-def-editor-authoring --strict --no-interactive`。
- [x] 2.6 验证 DefinitionAsset 创建入口都归类在 `Tirefly Combat System -> Definition Asset` 下。
- [x] 2.7 验证 `Gameplay Runtime` 暴露了组件 StateTree、StateInstance StateTree 和 SkillInstance Blueprint 的创建入口。
- [x] 2.8 验证 gameplay runtime 入口会按预期 schema 或父类创建资产，而不再弹出额外选择器。
- [x] 2.9 执行一次覆盖更新后 `TireflyCombatSystemEditor` 模块的 editor-target 编译。
- [x] 2.10 在子菜单/runtime authoring 更新后，再次运行 `openspec validate add-tcs-def-editor-authoring --strict --no-interactive`。

## 3. 延后跟进

暂时不要归档这条 change。应继续保持打开状态，直到 Skill 侧 authoring 面成熟到足以把这项能力重新视作一条完整的 editor-authoring 主线；至少要等到 `SkillDef` 能作为稳定的资产化入口暴露出来。

- [ ] 3.1 如果未来引入了专用 `UTcsStateTreeSchema_StateComponent`，就把现有 `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree` 入口改为指向该 schema，而不是继续保留临时的 `UStateTreeComponentSchema` 预设。
- [ ] 3.2 一旦 `SkillDef` 变成可稳定资产化的类型，就把同一套 TCS 编辑器 authoring capability 扩展到对应 Skill authoring 入口及验证上。
