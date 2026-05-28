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
- [x] 1.10 为组件 StateTree、Buff StateTree 与 learned-skill data Blueprint 创建补上 `Gameplay Runtime` authoring 入口，并为后续 Skill authoring 扩展预留升级点。
- [x] 1.11 更新文档，说明拆分后的 TCS authoring 子菜单布局以及新的 gameplay runtime 入口。

## 2. 验证

- [ ] 2.1 等待开发者手动执行编辑器测试：确认每个受支持的具体 DefinitionAsset 类型都出现在插件自有资产分类下。
- [ ] 2.2 等待开发者手动执行编辑器测试：确认创建每个受支持的 DefinitionAsset 时，都不再需要通用 `Data Asset` 的父类选择步骤。
- [ ] 2.3 等待开发者手动执行编辑器测试：确认创建出的资产仍返回预期的 `PrimaryAssetId`，并继续兼容既有运行时加载路径。
- [ ] 2.4 等待开发者手动执行编辑器测试：确认没有任何编辑器入口会指向抽象 DefinitionAsset 类。
- [x] 2.5 运行 `openspec validate add-tcs-def-editor-authoring --strict --no-interactive`。
- [ ] 2.6 等待开发者手动执行编辑器测试：确认 DefinitionAsset 创建入口都归类在 `Tirefly Combat System -> Definition Asset` 下。
- [ ] 2.7 等待开发者手动执行编辑器测试：确认 `Gameplay Runtime` 暴露了组件 StateTree、Buff StateTree 与 learned-skill data Blueprint 的创建入口。
- [ ] 2.8 等待开发者手动执行编辑器测试：确认 gameplay runtime 入口会按预期 schema 或父类创建资产，而不再弹出额外选择器。
- [x] 2.9 执行一次覆盖更新后 `TireflyCombatSystemEditor` 模块的 editor-target 编译。
- [x] 2.10 在子菜单/runtime authoring 更新后，再次运行 `openspec validate add-tcs-def-editor-authoring --strict --no-interactive`。

## 3. 延后跟进

暂时不要归档这条 change。应继续保持打开状态，直到 Skill 侧 authoring 面成熟到足以把这项能力重新视作一条完整的 editor-authoring 主线；至少要等到 `SkillDef` 能作为稳定的资产化入口暴露出来。

- [x] 3.1 将现有 `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree` 入口切换为指向 `UTcsStateSchema_StateComponent`，不再保留临时的 `UStateTreeComponentSchema` 预设。
- [x] 3.2 在 `refactor-state-runtime-access-contract` 删除 generic `StateInstance` schema 后，将现有过渡性的 `StateInstance StateTree` 入口迁移为 concrete runtime owner 入口，不再继续暴露抽象共享运行时。
- [x] 3.3 将 learned-skill data Blueprint 入口从当前旧名对齐到 `UTcsSkillEntry`，并同步更新等待开发者手动执行的编辑器测试清单。
- [ ] 3.4 一旦 `SkillDef` 变成可稳定资产化的类型，就把同一套 TCS 编辑器 authoring capability 扩展到对应 Skill authoring 入口及等待开发者手动执行的编辑器测试清单上。

## 4. AssetManagerSettings 勘误校验（新增）

- [x] 4.1 在编辑器侧新增 TCS DefinitionAsset 覆盖检查入口，校验 `AssetManagerSettings.PrimaryAssetTypesToScan` 是否覆盖 `UTcsAttributeDefinition` / `UTcsAttributeModifierDefinition` / `UTcsStateDefinition` / `UTcsStateSlotDefinition` 的类型与扫描目录。
- [x] 4.2 在 `UTcsDeveloperSettings` 新增“DefAsset 勘误忽略列表”配置项；忽略列表中的 DefAsset 类型不参与漏配报错。
- [x] 4.3 当检测到未忽略的类型漏配、路径漏配或规则失配时，输出可读勘误信息，明确“缺少哪个 PrimaryAssetType 与哪个扫描路径”。
- [x] 4.4 勘误校验仅做提示，不自动改写 `AssetManagerSettings`；提示中附带建议修复步骤。
- [x] 4.5 接入编辑器 Save 事件，只要仍存在未忽略漏配项，每次 Save 后都重复提示一次。
- [x] 4.6 更新 TCS authoring/setup 文档，增加“AssetManagerSettings 漏配诊断与忽略列表”章节。

## 5. 勘误能力验证（新增）

- [ ] 5.1 等待开发者手动执行编辑器测试：构造至少一条 `TcsDefinitionAsset` 漏配场景，确认编辑器能给出准确勘误提示。
- [ ] 5.2 等待开发者手动执行编辑器测试：补齐漏配后，确认勘误提示清除，且现有 Definition 资产同步与加载路径不受破坏。
- [ ] 5.3 等待开发者手动执行编辑器测试：将某 DefAsset 类型加入忽略列表后，确认该类型不再报错；移除忽略后恢复报错。
- [ ] 5.4 等待开发者手动执行编辑器测试：在漏配未修复且未忽略时，连续执行两次 Save，确认两次都收到勘误提示。
- [x] 5.5 运行 `openspec validate add-tcs-def-editor-authoring --strict --no-interactive`，确认新增任务与规范一致。
