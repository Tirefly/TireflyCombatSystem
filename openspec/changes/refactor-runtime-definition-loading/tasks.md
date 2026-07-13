> 执行标记说明
> - `[AI]`：我可以直接完成的任务（文档、代码、配置、命令行校验）
> - `[用户]`：需要你在 Unreal Editor 或真实资产环境里手动完成 / 观察 / 确认的任务
> - `[协作]`：我先改代码或给步骤，你再在编辑器里验证并反馈结果

## 1. 规格与设计收敛
- [x] [AI] 1.1 明确统一运行时 Definition 归口 `UTcsDefinitionManagerSubsystem` 的职责边界、命名与作用域。
- [x] [AI] 1.2 锁死所有 DefAsset 都统一遵循 `UTcsDeveloperSettings` 的三种加载策略：全部预加载、只预加载特定资产、完全不预加载；并明确预加载固定异步、运行时加载推荐异步并提供同步补充。
- [x] [AI] 1.3 明确运行时加载配置、source cache 与 AssetManager 建模都以具体非抽象 DefAsset 类型为基本单位，而不是以抽象 `UTcsStateDefinition` 为基本单位。
- [x] [AI] 1.4 明确 `UTcsStateDefinition` 在新架构下只保留抽象语义基类与共享行为契约，不再承担独立加载配置族、缓存中心或 AssetManager 扫描中心职责，也不再提供直接 public 查询接口。
- [x] [AI] 1.5 明确哪些 Definition 类型进入统一运行时归口；本次 change 虽不扩展到所有 authoring 表面，但必须覆盖当前全部非抽象 DefAsset 的按配置加载，并同步覆盖 `Attribute`、`Buff` 模块相关路径。
- [x] [AI] 1.6 明确 `UTcsDefinitionEditorManagerSubsystem` 的命名、文件路径、职责边界与非职责边界，并确认它由现有 `UTcsDefAssetDataTableSyncSubsystem` 演进而来，而不是新增并列 EditorSubsystem。
- [x] [AI] 1.7 明确 `UTcsDeveloperSettings` 仅保留设置读取职责，不再承担 runtime cached defs、editor 缓存快照或 Def 缓存基站角色；现有 `StateLoadingStrategy` 语义要收敛为统一 Def 加载配置模型。
- [x] [AI] 1.8 明确 `UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` 在新架构下的最终职责。
- [x] [AI] 1.9 明确 Skill 侧“对外由 `SkillDefId` 驱动、实例内部由已校验 `UTcsSkillDefinition*` 作为权威运行时缓存”的双层约束，以及 Entry 生命周期、Definition 解析时机与失败语义。
- [x] [AI] 1.10 明确 `ApplyBuff` / `ApplySkillModifier` 的 DefId 主路径、包装入口与失败语义。
- [x] [AI] 1.11 明确统一 Definition 查询失败语义、日志责任层级，以及 editor registry / editor manager / runtime manager 的强解耦边界。

## 2. 旧逻辑清理前置
- [x] [AI] 2.1 清理 `UTcsDeveloperSettings` 中把 cached defs 当作 runtime/bootstrap 基站的旧逻辑与旧注释，不允许继续通过 `DeveloperSettings` 承载 Definition 会话态缓存。
- [x] [AI] 2.2 清理 `UTcsDefinitionManagerSubsystem` 读取 `DeveloperSettings` cached defs 作为 runtime authoritative source 的旧路径。
- [x] [AI] 2.3 清理“editor subsystem / editor registry 可以给 runtime manager 提供桥接快照”的残留假设，明确 runtime 不依赖 editor 侧缓存结果启动。
- [x] [AI] 2.4 清理现有 `UTcsDefAssetDataTableSyncSubsystem` 的狭义命名与接口假设，为升级为 `UTcsDefinitionEditorManagerSubsystem` 腾位。
- [x] [AI] 2.5 清理遗留在 `UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` / `Buff` 相关路径上的 Definition cache/load/query 归口职责与相关旧假设。
- [x] [AI] 2.6 清理“抽象 `StateDef` 是独立加载配置族 / 独立缓存中心 / 独立 AssetManager 扫描中心 / 可直接 public 查询对象”的旧假设。
- [x] [AI] 2.7 清理 `AssetManagerSettings` 中把 `BuffDef` / `SkillDef` 共同挂在抽象 `TcsStateDef` 扫描路径下的旧配置与旧校验假设。

## 3. 编辑器期 Definition 管理中枢
- [x] [AI] 3.1 将现有 `UTcsDefAssetDataTableSyncSubsystem` 升级并重命名为 `UTcsDefinitionEditorManagerSubsystem`。
- [x] [AI] 3.2 让 `UTcsDefinitionEditorManagerSubsystem` 成为编辑器期 DefAsset / DataTable 的唯一桥接协调中心。
- [x] [AI] 3.3 为 `UTcsDefinitionEditorManagerSubsystem` 补齐受管 Def 的缓存、索引、脏标记、更新队列。
- [x] [AI] 3.4 让 `UTcsDefinitionEditorManagerSubsystem` 统一处理编辑器期资产事件监听与调度。
- [x] [AI] 3.5 明确并实现防递归回写策略，避免 DefAsset ↔ DataTable 互相影响时出现循环同步。
- [x] [AI] 3.6 将 `UTcsDefinitionRegistrySubsystem` 的源文件从 Runtime 模块（`Source/TireflyCombatSystem/`）迁移到 Editor 模块（`Source/TireflyCombatSystemEditor/`），因为第二阶段重构后运行时代码已不再引用该子系统，其实现几乎全部包裹在 `#if WITH_EDITOR` 中。
- [x] [协作] 3.7 验证 `UTcsDefinitionEditorManagerSubsystem` 不承担 runtime authoritative cache、runtime lifecycle 或通用 authoring 校验中枢职责。

## 4. 运行时 Definition 加载层
- [x] [AI] 4.1 新增统一的运行时 Definition 管理子系统 `UTcsDefinitionManagerSubsystem`。
- [x] [AI] 4.2 将多类 DefinitionAsset 的 source cache、预加载策略、按需加载策略迁移到新子系统。
- [x] [AI] 4.2.1 在 `UTcsDefinitionManagerSubsystem` 中新增 source cache（`TMap<FName, TSoftObjectPtr<DefType>>`），与现有 loaded cache 分离。
- [x] [AI] 4.2.2 `Initialize` 时先重建 source cache（不加载资产），再按配置执行预加载。
- [x] [AI] 4.2.3 查询路径改为"先查 loaded cache → 未命中则从 source cache 按需同步加载 → 写入 loaded cache → 返回"。
- [x] [AI] 4.2.4 `GetAll...DefIds()` 改为返回 source cache 的 key 集合，而非 loaded cache。
- [x] [AI] 4.2.5 tag 查询在索引未命中时，从 source cache 逐条同步加载并匹配，命中后写入 loaded cache 与 tag 索引。
- [x] [AI] 4.3 为所有进入统一归口的 DefAsset 建立统一三种加载策略下的异步预加载主路径。
- [x] [AI] 4.3.1 新增 `RequestAsyncPreload()` 入口，使用 `UAssetManager::LoadPrimaryAssets` 异步加载。
- [x] [AI] 4.3.2 `Initialize` 中根据 `DeveloperSettings` 配置触发异步预加载；`PreloadAll` 策略下异步加载全部，`PreloadSelected` 下只加载白名单。
- [x] [AI] 4.3.3 异步预加载完成后将资产写入 loaded cache 与 tag 索引，并设置 `bIsRuntimeReady`。
- [x] [AI] 4.4 为所有进入统一归口的 DefAsset 建立单资产粒度的按需异步加载主路径。
- [x] [AI] 4.4.1 新增 `LoadDefinitionAsync()` 入口，使用 `UAssetManager::LoadPrimaryAsset` 异步加载单个资产。
- [x] [AI] 4.4.2 异步加载完成后写入 loaded cache，并通过回调通知调用方。
- [x] [AI] 4.4.3 定义异步加载完成回调委托类型，回调参数至少包含：`DefId`、加载结果（成功/失败）、加载完成的 Definition 指针（失败时为 nullptr）。
- [x] [AI] 4.4.4 `LoadDefinitionAsync()` 在资产已在 loaded cache 中时立即同步回调，不走异步路径。
- [x] [AI] 4.4.5 同一 `DefId` 的并发异步请求只发起一次实际加载，完成后统一广播给所有回调方。
- [x] [AI] 4.5 若保留同步加载接口，明确它们只是运行时显式补充能力，不得反过来成为预加载主模型。
- [x] [AI] 4.5.1 确认现有 `LoadSynchronous` 路径仅用于按需同步补充，不在 `Initialize` 中作为预加载主路径。
- [x] [AI] 4.6 为具体非抽象 DefAsset 类型建立统一加载配置面，并把现有 `DeveloperSettings` 明确收敛为统一三种加载策略配置，覆盖全部当前非抽象 DefAsset。
- [x] [AI] 4.7 调整 `AssetManagerSettings`：为 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 建立与加载配置粒度一致的 `PrimaryAssetType` 与扫描路径。
- [x] [AI] 4.8 从 `AssetManagerSettings` 中移除 `BuffDef` / `SkillDef` 共挂抽象 `TcsStateDef` 扫描路径的建模方式。
- [x] [AI] 4.9 为 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 提供类型化查询入口。
- [x] [AI] 4.9.1 明确并实现按 `DefId` 的类型化查询面；不得只提供弱类型通用入口。
- [x] [AI] 4.9.1.1 查询接口命名不强制区分 `Find` / `Get` / `TryResolve`，但最终保留面必须足够全面覆盖主执行路径。
- [x] [AI] 4.9.2 保留 `BuffDef` / `StateSlotDef` 的按 tag 查询语义；本次 change 不为其他 Definition 类型强行扩展新的 tag 查询契约。
- [x] [AI] 4.9.3 取消所有直接查询抽象 `StateDef` 的 public runtime 接口，并清理依赖这些接口的调用路径。
- [x] [AI] 4.9.4 将 `StateDefId` 相关查询与标识语义收紧到 `State` 模块内部；若实际检查后无必要，则直接清零。
- [x] [AI] 4.9.5 确保所有 Buff 相关 public API 若涉及 DefId，都统一使用 `BuffDefId`，不得继续对外使用 `StateDefId`。
- [x] [AI] 4.9.6 为按 `DefId` / 按 tag 的查询入口实现统一失败结果，不得伪造占位 Definition 或静默成功。
- [x] [AI] 4.9.7 为权威失败诊断补齐固定字段：查询 key / `DefId`、入口名、失败类别。
- [x] [AI] 4.9.8 收窄 `UTcsDefinitionManagerSubsystem` 的 runtime-ready 契约；不得用跨 Definition 域的全局聚合就绪条件阻塞仅消费 `State` / `Attribute` 的运行时组件，必要时拆分为更贴近消费面的域内就绪判定。
- [x] [AI] 4.10 为新子系统补齐独立 capability 规格，避免与 `StateManagerSubsystem` 或编辑器期 registry / editor manager 职责混淆。

## 5. Manager / Registry 解耦
- [x] [AI] 5.1 从 `UTcsStateManagerSubsystem` 中移除 Definition cache/load 与 registry 刷新同步职责。
- [x] [AI] 5.2 评估并收敛 `UTcsAttributeManagerSubsystem` 与新 Definition 管理层之间的依赖方式，并同步检查 `Buff` 相关应用路径的归口调整。
- [x] [AI] 5.3 检查 `UTcsStateManagerSubsystem` 保留的跨 Actor facade 是否仍在对外暴露 `StateDefId`；若主要服务于 Buff apply，则同步改名/改参到 `BuffDefId` 语义。
- [x] [AI] 5.4 调整 `UTcsDefinitionRegistrySubsystem` 的编辑器期快照消费方，使编辑器期同步不再要求 `StateManagerSubsystem` 直连 registry。
- [x] [AI] 5.5 调整 `UTcsDefinitionRegistrySubsystem` 的 AssetManager 覆盖检查逻辑，使其按具体非抽象 DefAsset 类型分别校验，不再默认把 `BuffDef` / `SkillDef` 视为抽象 `StateDef` 家族的一条扫描规则。
- [x] [AI] 5.6 明确 `UTcsDefinitionEditorManagerSubsystem` 与 `UTcsDefinitionRegistrySubsystem` 的边界：editor manager 可以消费编辑器快照，但不得把这种消费扩展为 runtime 契约。
- [x] [AI] 5.7 允许迁移期保留 deprecated wrapper，但必须在 change 归档前清零。
- [x] [AI] 5.8 明确 editor registry / editor manager 都不定义 runtime source cache 生命周期，也不引入通用 `RuntimeRefresh` 契约。
- [x] [AI] 5.9 调研并清理名义管理器空壳职责：若 `UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` 已不具备独立存在必要，则将残余职责下沉到更贴近使用点的组件或 DefinitionManager，并删除子系统本体。
- [x] [AI] 5.9.1 删除 `UTcsStateManagerSubsystem`：将 `StateInstanceId` 工厂下沉到 `UTcsStateComponent` static，清理 RuntimeBootstrap / StateComponent 对其依赖与死 include。
- [x] [AI] 5.9.2 删除 `UTcsAttributeManagerSubsystem`：将 AttributeTag 解析归口到 `UTcsDefinitionManagerSubsystem`，将 Attribute / Modifier ID 工厂下沉到 `UTcsAttributeComponent` static，将 `SourceHandle` 工厂下沉到 `UTcsStateComponent` static，并清理所有调用方。
- [x] [AI] 5.9.3 同步更新 OpenSpec 规格与任务清单，确保删除两类名义管理器后代码事实与 change 文档一致。

## 6. 运行时 InstanceId / Identity 方案收敛
- [x] [AI] 6.1 明确第五阶段下沉到 `Component static` 的 `StateInstanceId` / `AttributeInstId` / `ModifierInstId` / `ModifierChangeBatchId` / `SourceHandle.Id` 工厂只作为过渡实现，不视为最终架构。
- [x] [AI] 6.2 明确当前版本不为了 InstanceId 单独新增子系统；后续若需要统一身份账本，再单独评估是否引入集中管理实现。
- [x] [AI] 6.3 明确运行时实例身份在设计层至少区分：条目级稳定身份（如 `DefId`）、未来预测阶段身份（`PredictionKey`，仅作为预测/同步设计约束）与实例级 authority 最终身份（字段命名继续沿用 `StateInstId` / `AttrModInstId` / `SkillModInstId` 等现有语义名）；当前阶段不得因此新增 `PredictionKey` 代码。
- [x] [AI] 6.4 明确哪些运行时对象不需要实例级身份（如 `SkillEntry`、`AttributeInstance`），哪些运行时对象必须具备实例级 authority 身份（如 `StateInstance`、`AttributeModifierInstance`、`SkillModifierInstance`）。
- [x] [AI] 6.5 明确未来本地预测与 authority 确认的 reconcile 设计约束：若后续明确进入预测/同步实现，根请求携带 `PredictionKey`，客户端通过既有确认/复制链路完成 `PredictionKey -> 实例级 authority 身份` 映射，不额外为实例 reconcile 引入专门 RPC；当前阶段只写约束，不实现该链路。
- [x] [AI] 6.6 明确 `FTcsSourceHandle::Id` 是 **AuthorityOnly** 的因果链句柄字段；客户端预测阶段不得生成最终 authority `SourceHandle`。

## 7. 未来保留：运行时 InstanceId / Identity 方案实现（按需进入）

> 进入门槛：本阶段不是当前执行范围，也不是当前归档前必须完成的代码任务。只有用户明确批准进入本地预测 / 网络同步实现后，才允许把 7.x 转为可执行编码任务；否则后续实现默认从第 8 阶段继续。

- 7.1 未来若正式纳入本地预测开发，才允许在需要预测的主请求路径上接入 `PredictionKey`，并按 GAS 风格的根请求预测模型管理预测实例生命周期。
- 7.2 未来若正式纳入网络同步 / authority 验证，才允许为 `StateInstance` / `BuffInstance` / `SkillInstance` 接入最终确认的实例级 authority 身份确认链路；字段命名继续沿用 `StateInstId` 等现有语义名。
- 7.3 未来若对应路径进入跨端实例重关联范围，才评估 `AttributeModifierInstance` / `SkillModifierInstance` 的最终确认链路；当前不为其引入预测主路径。
- 7.4 未来若正式纳入预测/同步实现，才根据最终方案收敛 `FTcsSourceHandle` 的 authority-only 代码实现，并通过既有确认/复制链路完成预测对象绑定。

## 8. Skill DefId 主路径
- [x] [AI] 8.1 为 `UTcsSkillEntry` 明确“对外使用 `SkillDefId`，实例内部缓存已校验 `UTcsSkillDefinition*`”的权威模型。
- [x] [AI] 8.2 将 `LearnSkill` / `ActivateSkill` 主路径改为按 `SkillDefId` 驱动。
- [x] [AI] 8.3 直接移除对象指针版本的 public 入口；若迁移阶段存在残余逻辑，只允许退化为内部辅助转发实现。
- [x] [AI] 8.4 明确 SkillDef 解析失败、缺失或加载失败时的运行时错误语义。

## 9. DefId 主路径扩展到 Buff / SkillModifier
- [ ] [AI] 9.1 将 Buff apply 主路径收敛到 `StateDefId` / `BuffDefId` 驱动，不要求调用方先持有已加载 Def 对象。
- [ ] [AI] 9.2 将 SkillModifier apply 主路径收敛到 `SkillModifierDefId` 驱动，不要求调用方先持有已加载 Def 对象。
- [ ] [AI] 9.3 直接移除旧对象型 public 入口；若迁移阶段存在残余逻辑，只允许退化为内部辅助转发实现。

## 10. 验证
- [ ] [AI] 10.1 编译验证 TCS 运行时与编辑器模块。
- [ ] [AI] 10.2 验证所有受管 DefAsset 都遵循统一三种加载策略，且预加载主路径固定走异步。
- [ ] [AI] 10.3 验证所有受管 DefAsset 都支持单资产粒度的按需异步加载。
- [ ] [AI] 10.4 验证同步加载接口若存在，只作为显式补充能力，不会反客为主成为默认主路径。
- [ ] [AI] 10.5 验证统一加载配置面已覆盖当前全部非抽象 DefAsset 类型，而不是只覆盖抽象 `StateDef` 家族。
- [ ] [AI] 10.6 验证 `AssetManagerSettings` 已按 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 等具体类型分别配置扫描路径。
- [ ] [AI] 10.7 验证 `BuffDef` / `SkillDef` 不再共同挂在抽象 `TcsStateDef` 扫描路径下。
- [ ] [AI] 10.8 验证所有直接查询抽象 `StateDefinition` 的 public runtime 接口都已清零。
- [ ] [AI] 10.9 验证 State / Skill / Modifier 相关 Def 查询与按需加载路径。
- [ ] [协作] 10.10 验证编辑器期 `UTcsDefinitionRegistrySubsystem` / `UTcsDefinitionEditorManagerSubsystem` 与 runtime definition manager 已完成职责解耦。
- [ ] [AI] 10.11 验证 `UTcsStateManagerSubsystem` 已不再暴露 Definition 查询 public API。
- [ ] [AI] 10.12 验证 `LearnSkill` / `ActivateSkill` / `ApplyBuff` / `ApplySkillModifier` 的对象型 public 入口已清零，若仍有残余逻辑也仅限内部辅助实现。
- [ ] [AI] 10.13 验证 Definition 解析失败时不会产生占位运行时对象、部分 apply 或静默成功。
- [ ] [AI] 10.14 验证 runtime contract 中不存在依赖 editor registry、editor manager 或 `DeveloperSettings` cached defs 触发的 Definition cache 重建语义。
- [ ] [AI] 10.15 验证失败诊断固定带有查询 key / `DefId`、入口名、失败类别。
- [ ] [AI] 10.16 验证 `UTcsDefinitionManagerSubsystem` 的 runtime-ready 判定不会因无关 Definition 域未就绪而阻塞当前消费面；`State` / `Attribute` / `Skill` 等主路径仅受自身必需 Definition 域约束。

## 11. 用户手动验证清单
- [ ] [用户] 11.1 在 Unreal Editor 中验证 DefAsset 新增、修改、删除后，`UTcsDefinitionEditorManagerSubsystem` 的桥接与缓存更新行为符合预期。
- [ ] [用户] 11.2 在 Unreal Editor 中验证 DefAsset ↔ DataTable 回写不会出现递归循环、重复刷新或错误覆盖。
- [ ] [用户] 11.3 在 Unreal Editor 中验证资产移动、重命名、删除、重导入后，编辑器期索引、脏标记与更新队列行为符合预期。
- [ ] [用户] 11.4 在 Unreal Editor 中验证 Details 面板、菜单入口、编辑器事件监听相关行为没有因为本次重构失效。
- [ ] [用户] 11.5 对 AI 无法直接观察的编辑器期现象，记录实际步骤、日志、截图或报错文本，回传用于后续修正。
