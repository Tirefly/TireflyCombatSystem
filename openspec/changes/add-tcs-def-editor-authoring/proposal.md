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
  - `UStateTree` preset for `UTcsStateComponent` usage through `UTcsStateSchema_StateComponent`
  - `UStateTree` preset for `UTcsBuffInstance` usage through `UTcsStateSchema_Buff`
  - Blueprint subclasses of `UTcsSkillEntry`
- 协调当前 `UTcsStateDefinition` 编辑器入口与其抽象运行时状态之间的关系：
  - 如果 `UTcsStateDefinition` 仍保持抽象，就不要把它暴露成一个损坏的直接创建目标
  - 取而代之，应暴露那些真正可 authoring 的具体 state 侧 DefinitionAsset 类型
  - 如果未来 `UTcsStateDefinition` 再次变成可直接 authoring 的类型，那么对应入口必须指向一个可实例化类
- 为每个受支持的 DefinitionAsset 补上专用 `UFactory` 与 `UAssetDefinitionDefault`，使其在 UE 5.7 中具备明确的显示名、分类、颜色和创建行为。
- 对 Gameplay Runtime authoring 入口使用预设工厂配置，避免开发者在受支持的 TCS runtime 目标上再次经历 schema picker 或 parent-class picker。
- 保持所有 TCS Def 资产继续是 `UPrimaryDataAsset` 的子类。本次变更只处理编辑器 authoring 入口，不改变运行时资产基类。
- 保持 Def subclassing 在技术上兼容，但不把它视为主要扩展模型。官方扩展方向仍然是 composition-first，并可在未来演进到 Def fragments。
- 新增编辑器阶段的 `AssetManagerSettings` 勘误校验：在编辑器内检查 TCS DefinitionAsset 是否被 `PrimaryAssetTypesToScan` 正确覆盖，并在漏配时提供可读错误提示。
- 勘误校验只用于编辑器阶段的配置完整性检查，不修改运行时加载时机与加载策略。
- 在 `UTcsDeveloperSettings` 中新增 TCS DefinitionAsset 勘误忽略列表；忽略列表中的 DefAsset 类型不参与漏配报错。
- 勘误检查对“类型漏配”和“扫描路径漏配”都必须单独报错，不允许仅检查类型是否存在。
- 只要存在未忽略且未修复的漏配项，开发者每次通过常用保存入口执行 Save 操作都应再次收到一次明显的勘误提示，直到配置被修复或被加入忽略列表。
- 这些保存入口至少包括：普通单资产/单包保存、主窗口菜单或快捷键触发的 `Save All`，以及 Content Browser 顶部工具栏的 `Save All`。
- 每次重复提示都应同时输出 `UE_LOG(LogTcs, Error, ...)` 与编辑器右下角 toast，避免提示只停留在某一条保存路径上。

## 后续参考口径

- 若后续需要评估勘误提示是否已经过度打扰开发流程，可先观察 5 个工作日窗口内的 Save 提示频率、同一漏配项的重复触发密度、提示后的修复转化率、忽略列表膨胀率与开发者主观干扰反馈。
- 这组观察口径仅作为后续调整策略的参考，不属于本次实现范围。

## 延后跟进与归档门槛

- 当前 `State Component StateTree` 入口已经切换到 `UTcsStateSchema_StateComponent`；后续如需扩展组件树 authoring 能力，应继续沿用同一条 `Gameplay Runtime` 菜单路径，而不是拆出另一条无关入口。
- 当前 gameplay runtime 的运行时树入口已经不再暴露 generic `StateInstance StateTree`，而是收敛为 concrete runtime owner 入口（当前为 `Buff StateTree`）；后续新增其他 concrete runtime 入口时，也应继续沿用同一 capability。
- 当前 learned-skill data Blueprint 入口已经切换到 `UTcsSkillEntry`，避免 editor authoring 面继续固化旧名。
- 在 Skill 侧编辑器 authoring 面成熟到足以并入同一 capability 之前，这条 change 不应归档；至少要等到 `SkillDef` 能作为稳定的资产化 authoring 入口暴露出来。

## 影响范围

- 受影响规范：
  - `def-editor-authoring`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/TireflyCombatSystem.uplugin`
  - new `Plugins/TireflyCombatSystem/Source/TireflyCombatSystemEditor/` module
  - editor-only factories and asset definitions for all supported TCS DefinitionAsset types
  - editor-only runtime asset factories for TCS-owned gameplay authoring entries
  - editor-only validation path for `AssetManagerSettings` coverage of TCS DefinitionAsset types/directories
  - editor-only repeated-report path for package save、主窗口/快捷键 `Save All` 与 Content Browser 顶部 `Save All`
  - `UTcsDeveloperSettings` 中新增用于勘误过滤的忽略列表配置项
- 受影响文档：
  - TCS authoring/setup 文档应把用户引导到插件自有的资产创建路径，而不是通用 `Data Asset` 流程
  - TCS authoring/setup 文档应说明 `Definition Asset` 与 `Gameplay Runtime` 子菜单的划分
  - TCS authoring/setup 文档应新增“AssetManagerSettings 漏配勘误提示”的解释与处理步骤
