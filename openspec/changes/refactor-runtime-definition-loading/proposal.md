# 变更：重构运行时 Definition 加载归口

## 背景
当前 TCS 将运行时 Definition 的缓存、预加载策略、按需加载策略与部分查询职责集中在 `UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` 中。这一结构在 `StateDef` 侧尚可工作，但已经无法自然承载 `SkillDef`、`SkillModifierDef`、`AttributeDef`、`AttributeModifierDef`、`StateSlotDef` 等多类 DefinitionAsset 的统一运行时加载策略。

与此同时，`UTcsSkillComponent` 当前仍要求 `LearnSkill(UTcsSkillDefinition* Def)` 直接接收已加载对象，导致已学会技能的拥有态、运行时 Definition 生命周期、存档/同步/配置驱动授予技能这三类关注点耦合在一起。

更严重的是，当前运行时路径仍带有三类错误边界：
- 运行时 Definition 主路径仍偏向同步加载，既没有把异步加载作为核心契约，也没有把单个 DefAsset 的按需异步解析作为硬要求。
- `UTcsDeveloperSettings` 仍承担了超出“设置读取”范围的 Def 缓存基站角色，这会把 editor 构建产物、runtime authoritative cache、会话期加载状态错误混在一起。
- `UTcsStateDefinition` 现在已经是抽象类，但当前 AssetManager 与加载配置仍把 `BuffDef` / `SkillDef` 混在 `TcsStateDef` 这类抽象家族语义下管理；这会把抽象语义层错误地延续成具体资产加载层，制造重叠缓存、重叠配置和重叠加载逻辑。

此外，TCS 已存在编辑器期 `UTcsDefAssetDataTableSyncSubsystem`。如果不先把它升级为明确的编辑器期 Definition 管理中枢，而是继续叠加新的缓存/桥接组件，就会出现多个 EditorSubsystem 并行监听资产事件、并行维护缓存、并行调度更新的职责重叠问题。

现有规范还要求 `UTcsStateManagerSubsystem` 继续承担 state-side definition cache/load；该约束与“统一的 Definition 运行时加载层”方向冲突，因此本提案将显式修订这些既有规范。

## 变更内容
- 新增统一的运行时 Definition 加载归口 `UTcsDefinitionManagerSubsystem`，作为唯一运行时 authoritative cache base，用于承载多类 TCS DefinitionAsset 的 source cache、预加载策略、按需加载策略与运行时查询。
- 所有 TCS DefAsset 都必须统一遵循 `UTcsDeveloperSettings` 中的三种加载策略：**全部预加载**、**只预加载特定资产**、**完全不预加载**。其中预加载必须走异步主路径；运行时加载应把异步加载作为推荐主方案，同时提供显式同步加载补充方案。
- 运行时加载配置、source cache、预加载策略与按需加载策略，必须以**非抽象 DefAsset 类型**为基本单位，而不是以抽象基类为单位；至少要覆盖 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 这些当前可实例化的 DefinitionAsset 类型。
- `UTcsStateDefinition` 作为抽象基类可以继续保留 state-like 语义与共享行为契约，但不得再继续充当独立的加载配置族、缓存中心、AssetManager 扫描中心，也不得继续暴露直接查询它本身的 public runtime 接口。
- 如果加载实现继续依赖 `AssetManager`，则 `AssetManagerSettings` 的 `PrimaryAssetType` 与扫描路径也必须与上述具体非抽象 DefAsset 粒度保持一致；`BuffDef` 与 `SkillDef` 不得继续共同挂在抽象 `TcsStateDef` 的扫描路径下，而应拆成各自独立的 `PrimaryAssetType` 与扫描目录配置。
- 新子系统的 public 查询面将显式区分类型化入口：至少覆盖 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 的按 `DefId` 查询；其中当前已存在 tag 驱动运行时语义的 `BuffDef` / `StateSlotDef` 继续保留按 tag 查询。
- `StateDefId` 相关查询与标识语义必须收紧到 `State` 模块内部，仅用于单个 state definition 在 `StateComponent` 上的运行生命周期；Buff 相关 public API 若涉及 Definition 标识，必须统一使用 `BuffDefId`，不得继续对外暴露 `StateDefId`。
- 为所有 DefId 驱动主路径补齐统一失败语义：Definition 解析失败时，运行时主流程必须显式失败，不得静默降级、伪造占位对象或部分成功。
- `UTcsDefinitionManagerSubsystem` 的最终查询接口不强制区分 `Find` / `Get` / `TryResolve` 三套命名分层，但最终保留的类型化查询面必须足够全面，能够覆盖各主执行路径所需的按 `DefId` / 按既有 tag 语义查询。
- `UTcsDeveloperSettings` 必须退出 Def 缓存基站角色，仅保留设置读取职责；运行时 Definition authoritative cache、编辑器期 Def 缓存与桥接状态都不得继续挂在 `DeveloperSettings` 上。当前 `StateLoadingStrategy` 这类只面向抽象 `StateDef` 的设置语义必须收敛到面向具体非抽象 DefAsset 类型的统一加载配置模型。
- 现有 `UTcsDefAssetDataTableSyncSubsystem` 将升级并重命名为 `UTcsDefinitionEditorManagerSubsystem`，作为唯一编辑器期 Definition 管理中枢，负责编辑器期 DefAsset / DataTable 桥接协调、防递归回写、受管 Def 缓存/索引/脏标记/更新队列，以及编辑器事件监听与统一调度。
- 调整 `UTcsStateManagerSubsystem` 最终职责：移除 Definition cache/load 与 registry 同步职责，仅保留全局 state instance ID 工厂和跨 Actor apply 门面。
- 若 `UTcsStateManagerSubsystem` 仍保留跨 Actor facade，则其面向 Buff 的 public 语义必须同步改名/改参到 `BuffDefId`，避免继续通过 `TryApplyStateToTarget(..., StateDefId, ...)` 泄露旧的抽象 `StateDef` 语义。
- 调整 `UTcsSkillComponent` / `UTcsSkillEntry` / 相关 Skill 调用链，使 Skill 相关入口以 `SkillDefId` 为对外主路径身份；同时 `UTcsSkillEntry` 作为运行时实例，内部应缓存一个已由合法 `SkillDefId` 解析并校验过的 `UTcsSkillDefinition*`，作为运行时唯一权威 SkillDef 对象，避免重复读取负载。
- 统一要求 `LearnSkill`、`ActivateSkill`、`ApplyBuff`、`ApplySkillModifier` 支持按 DefId 执行；现有对象指针入口不再作为归档后保留的 public API 主路径，优先直接移除；若迁移阶段短暂保留，也只能退化为内部辅助转发逻辑。
- 保持编辑器期 `UTcsDefinitionRegistrySubsystem` 为权威快照持有者，但运行时 manager/subsystem 不再直接把 registry 刷新同步逻辑塞进 `StateManagerSubsystem`。
- 明确 editor registry、`UTcsDefinitionEditorManagerSubsystem` 与 runtime definition manager 强解耦：编辑器刷新、桥接重建和编辑器态缓存更新都不定义 runtime source cache 生命周期，也不引入通用 `RuntimeRefresh` 契约；runtime 也不得依赖 editor subsystem 或 `DeveloperSettings` 快照作为引导来源。
- Definition 失败诊断必须至少包含：目标 `DefId`（或查询 key）、发起查询/主路径的入口名，以及失败类别（未注册 / 类型不匹配 / 加载失败）。
- 本次 change 先聚焦“归口重构 + SkillDefId 主路径 + 异步优先加载契约 + 具体非抽象 DefAsset 粒度加载配置”，不在同一 change 内完成所有 Definition 类型的完整统一配置面设计。
- 本次 change 还将同步收敛 `Attribute`、`Buff` 模块中的相关 Definition 加载/查询/应用路径，避免只修 Skill/State 而把其余模块留在旧模型中。
- 新增 `runtime-definition-management` capability，用于定义 `UTcsDefinitionManagerSubsystem` 的运行时职责边界与类型化查询面。
- 先完成旧逻辑清理，再进入新加载机制实现：必须先清理 `DeveloperSettings` cached defs 旧路径、抽象 `StateDef` 中心化加载假设、旧的 `TcsStateDef` AssetManager 扫描绑定，以及旧的 `UTcsDefAssetDataTableSyncSubsystem` 狭义命名与职责假设，再讨论和推进后续实现细节。
- 为迁移收尾增加硬验收：在 change 归档前，`StateManagerSubsystem` 上遗留的 Definition 查询包装器、以及 `LearnSkill` / `ActivateSkill` / `ApplyBuff` / `ApplySkillModifier` 的 deprecated 对象型主入口都必须清零，不允许作为长期兼容层归档。

## 影响范围
- 受影响规范：
  - `combat-manager-subsystems`
  - `attribute-management`
  - `state-management`
  - `definition-live-registry`
  - `skill-runtime`
  - `runtime-definition-management`
- 受影响代码：
  - `Config/DefaultGame.ini`
  - `Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h`
  - `Source/TireflyCombatSystem/Private/TcsDefinitionRegistrySubsystem.cpp`
  - `Source/TireflyCombatSystem/Public/TcsDefinitionManagerSubsystem.h`
  - `Source/TireflyCombatSystem/Private/TcsDefinitionManagerSubsystem.cpp`
  - `Source/TireflyCombatSystem/Public/Attribute/*`
  - `Source/TireflyCombatSystem/Private/Attribute/*`
  - `Source/TireflyCombatSystem/Public/Buff/*`
  - `Source/TireflyCombatSystem/Private/Buff/*`
  - `Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
  - `Source/TireflyCombatSystem/Public/Skill/TcsSkillComponent.h`
  - `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp`
  - `Source/TireflyCombatSystem/Public/Skill/TcsSkillEntry.h`
  - `Source/TireflyCombatSystem/Private/Skill/TcsSkillEntry.cpp`
  - `Source/TireflyCombatSystemEditor/Public/TcsDefinitionEditorManagerSubsystem.h`
  - `Source/TireflyCombatSystemEditor/Private/TcsDefinitionEditorManagerSubsystem.cpp`
  - 新的运行时 / 编辑器期 Definition 管理相关文件

## 破坏性变化
- **BREAKING**：`UTcsStateManagerSubsystem` 的最终职责定义将被收窄，不再承担 Definition cache/load 与 registry 同步。
- **BREAKING**：所有 DefAsset 都必须统一遵循 `UTcsDeveloperSettings` 中的三种加载策略（全部预加载 / 只预加载特定资产 / 完全不预加载）；预加载固定走异步主路径，运行时加载默认推荐异步，同时提供显式同步补充路径。
- **BREAKING**：`UTcsDeveloperSettings` 不再持有 runtime cached defs，也不再承担 Definition 缓存基站角色；现有只面向抽象 `StateDef` 的加载设置语义将被收敛为面向具体非抽象 DefAsset 类型的统一加载配置。
- **BREAKING**：`UTcsStateDefinition` 不再继续作为 AssetManager 与加载配置的中心类型；`BuffDef` 与 `SkillDef` 等具体非抽象 DefAsset 将拆分为各自独立的 `PrimaryAssetType` 与扫描配置。
- **BREAKING**：现有 `UTcsDefAssetDataTableSyncSubsystem` 的命名与职责将升级为 `UTcsDefinitionEditorManagerSubsystem`，并被重定义为编辑器期 Definition 管理中枢而非狭义 DataTable 同步器。
- **BREAKING**：Skill 相关运行时主入口将从“要求调用方提供已加载 `UTcsSkillDefinition*`”迁移为“按 `SkillDefId` 驱动并由系统负责解析 Definition”；`UTcsSkillEntry` 内部则改为持有一个已校验的权威 `UTcsSkillDefinition*` 运行时缓存。
- **BREAKING**：运行时 Definition 查询面将从“散落在多个 gameplay manager 上的弱约定”迁移为“由 `UTcsDefinitionManagerSubsystem` 提供的显式类型化入口集合”。
- **BREAKING**：所有直接查询抽象 `StateDef` 的 public runtime 接口都将被取消。
- 迁移期间若因实现收口需要短暂存在对象型包装逻辑，它们也只能作为内部辅助实现存在；在 change 归档前，对外 public API 面 MUST 清零这些对象型入口，否则该 change 不应归档。

## 执行责任划分（基于当前没有 UE MCP / Editor 自动化桥接）

### AI 可直接执行
- OpenSpec 文档修改、任务拆分、设计收敛与一致性校验。
- C++ / `.h` / `.cpp` / `.ini` / 编辑器模块源码的静态修改与重构。
- `AssetManagerSettings`、`DeveloperSettings`、Subsystem 接口与实现层的代码级清理。
- 命令行可完成的验证：
  - `openspec validate`
  - UnrealBuildTool 编译
  - 纯命令行测试或日志检查（前提是已有可运行入口）
- 基于代码与配置的静态审查：确认是否仍残留抽象 `StateDef` 中心化加载假设、错误缓存归属、错误依赖方向等。

### 需要用户手动执行
- 任何依赖 Unreal Editor 交互界面的验证：
  - DefAsset / DataTable 在编辑器中的增删改查联动
  - Details 面板、菜单、编辑器事件、资产监听、回写行为
  - 编辑器内 authoring 流程是否符合预期
- 任何依赖真实资产内容与编辑器状态的最终行为确认：
  - 资产移动、重命名、删除、重导入
  - 编辑器中桥接刷新、脏标记、更新队列、防递归回写是否符合预期
- 任何当前仓库中尚未具备自动化入口、而又必须通过编辑器观察结果的验证。

### AI 与用户协作执行
- AI 负责先完成代码与配置改造，再给出明确的手动验证步骤。
- 用户负责在 Unreal Editor 中实际操作并回报现象、日志、报错与行为差异。
- 如果手动验证暴露问题，AI 再基于日志、代码和用户反馈继续修正实现。

### 本提案阶段的硬边界
- 在当前没有 UE MCP / Editor 自动化桥接的前提下，本提案不得把“编辑器内可视化验证已完成”写成 AI 已独立完成的事实。
- 任何涉及编辑器交互行为的“验证通过”，都必须明确标注为“需要用户手动验证”或“等待用户回报结果”。
- 因此，本 change 的前半段应优先推进 AI 可直接完成的工作：
  - 规格收敛
  - 旧逻辑清理
  - 运行时与配置层重构
  - 命令行编译/校验
- 编辑器交互层验证则必须在对应代码落地后，由用户手动接力完成。
