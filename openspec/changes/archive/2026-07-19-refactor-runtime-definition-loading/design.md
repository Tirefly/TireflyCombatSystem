## 背景
当前 TCS 对运行时 Definition 的管理存在两个明显问题：
1. `UTcsStateManagerSubsystem` 承担了不只属于 state runtime orchestration 的 definition cache/load 责任。
2. `UTcsSkillComponent` 仍以已加载的 `UTcsSkillDefinition*` 为学习入口，无法自然支持按 `SkillDefId` 持久化、同步和配置驱动授予技能。

随着范围扩大，现在还存在三个必须正面修正的结构性问题：
3. 所有 DefAsset 的核心加载契约仍未固定为异步优先，预加载与按需加载都缺少统一的异步主路径要求。
4. `UTcsDeveloperSettings` 与编辑器期同步路径仍被错误地用作 Def 缓存基站，这会把 editor 数据、runtime authoritative cache 与会话期加载状态混在一起。
5. `UTcsStateDefinition` 现在已经是抽象基类，但当前 AssetManager、加载配置与缓存心智仍把它当成可独立承载 `BuffDef` / `SkillDef` 的加载家族中心；这会在抽象语义层与具体可加载资产层之间制造重复配置与重复缓存。

## 目标
- 引入统一的运行时 Definition 加载归口 `UTcsDefinitionManagerSubsystem`。
- 让 `UTcsDefinitionManagerSubsystem` 成为唯一运行时 authoritative cache base。
- 让所有 DefAsset 统一遵循 `UTcsDeveloperSettings` 中的三种加载策略，并要求预加载固定走异步主路径。
- 让 Skill / Buff / StateSlot / Attribute / Modifier 等 DefinitionAsset 共享一致的运行时加载模型。
- 让运行时加载配置与 AssetManager 建模都转向**具体非抽象 DefAsset 类型**，而不是继续围绕抽象 `UTcsStateDefinition`。
- 让 `UTcsDeveloperSettings` 退出 Definition 缓存基站角色，只保留设置读取职责。
- 把现有 `UTcsDefAssetDataTableSyncSubsystem` 升级为编辑器期 `UTcsDefinitionEditorManagerSubsystem`，承接编辑器期桥接、缓存与调度职责。
- 让 `UTcsSkillEntry` 对外以 `SkillDefId` 参与主路径身份流转，但在实例内部以一个已校验的 `UTcsSkillDefinition*` 作为运行时唯一权威 SkillDef 缓存。
- 让 `ApplyBuff` / `ApplySkillModifier` 的主执行路径也按 DefId 驱动。
- 让 `UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` 退出 Definition cache/load 归口角色；若评估后仅剩空壳职责，则允许直接删除子系统本体。
- 让 `Attribute`、`Buff` 两个模块同步完成相关加载/查询/应用路径收敛，而不是只修 Skill 链路。

## 非目标
- 本提案不改变 Def editor authoring 菜单结构。
- 本提案不在同一 change 中重做所有 gameplay runtime API 的 Blueprint 表面，仅要求主执行路径收敛。
- 本提案不在同一 change 中完成所有 Definition 类型的完整统一加载策略配置面设计。

## 已确认设计决策
1. 新的统一运行时 Definition 归口命名固定为 `UTcsDefinitionManagerSubsystem`
2. 编辑器期中枢固定为 `UTcsDefinitionEditorManagerSubsystem`，并沿用插件编辑器模块根目录路径：
	- `Source/TireflyCombatSystemEditor/Public/TcsDefinitionEditorManagerSubsystem.h`
	- `Source/TireflyCombatSystemEditor/Private/TcsDefinitionEditorManagerSubsystem.cpp`
3. 第一阶段范围固定为“归口重构 + SkillDefId 主路径 + 异步优先加载契约 + editor/runtime 强断开 + 具体非抽象 DefAsset 粒度加载配置”，而不是一次性统一所有 Definition 类型的完整配置面
4. 所有 DefAsset 都固定遵循 `UTcsDeveloperSettings` 中的三种加载策略：全部预加载、只预加载特定资产、完全不预加载；预加载必须走异步主路径，运行时加载以异步为推荐主方案，同时提供显式同步补充方案
5. 运行时加载配置、AssetManager `PrimaryAssetType` 与扫描路径，都必须以**具体非抽象 DefAsset 类型**为基本单位；至少覆盖 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef`
6. `UTcsStateDefinition` 继续作为抽象语义基类存在，但不再作为独立加载配置族、缓存中心或 AssetManager 扫描中心，也不再暴露直接查询它本身的 public runtime 接口
7. 如果继续依赖 `AssetManager` 实现加载，则 `BuffDef` 与 `SkillDef` 必须拆分为各自独立的 `PrimaryAssetType` 与扫描路径配置，不能继续共同挂在抽象 `TcsStateDef` 家族下面
8. `UTcsDeveloperSettings` 只保留设置读取职责，不再持有 runtime cached defs，不再承担 Def 缓存基站角色；现有 `StateLoadingStrategy` 这类抽象 `StateDef` 中心化设置语义必须收敛为面向具体非抽象 DefAsset 的统一加载配置模型
9. `UTcsDefinitionEditorManagerSubsystem` 只承担编辑器期桥接协调、防递归回写、缓存/索引/脏标记/更新队列、编辑器事件监听与统一调度；它不参与 runtime authoritative cache，不参与 gameplay runtime 生命周期，也不为 runtime `UTcsDefinitionManagerSubsystem` 提供桥接快照来源
10. `UTcsSkillEntry` 的外部身份流转以 `SkillDefId` 为准，但运行时实例内部应持有一个由合法 `SkillDefId` 解析并校验后的权威 `UTcsSkillDefinition*` 缓存，用于减轻重复读取负载
11. `LearnSkill` / `ActivateSkill` / `ApplyBuff` / `ApplySkillModifier` 的主执行路径固定为按 DefId 驱动；对象指针版本不再作为归档后保留的 public API 主路径，优先直接删除，必要时也只能作为内部辅助逻辑短暂存在
12. `Attribute` 与 `Buff` 模块必须与 Skill / State 一起完成 Definition 加载与应用路径收敛，不能留在旧模型里
13. `StateDefId` 相关查询与标识语义只允许收紧到 `State` 模块内部，用于单个 state definition 在 `StateComponent` 上的运行生命周期；Buff 相关 public API 若涉及 Definition 标识，必须统一使用 `BuffDefId`
14. 面向 Buff 语义的跨 Actor facade 若继续保留，命名与参数也必须同步收敛到 `BuffDefId`，不得继续以 `TryApplyStateToTarget(..., StateDefId, ...)` 形式对外泄露旧抽象语义；若无外部调用方，则优先直接删除而不是保留空壳 facade
15. `UTcsDefinitionManagerSubsystem` 的最终查询接口不强制区分 `Find` / `Get` / `TryResolve` 三种命名层级，但保留的类型化查询面必须足够全面
16. 失败诊断必须带上查询 key / `DefId`、入口名、失败类别（未注册 / 类型不匹配 / 加载失败）
17. editor registry、`UTcsDefinitionEditorManagerSubsystem` 与 runtime definition manager 强解耦；runtime 不定义通用 refresh 生命周期，也不引入 `RuntimeRefresh` 契约
18. 第五阶段若为先移除空壳子系统而把 `StateInstanceId`、`AttributeInstId`、`ModifierInstId`、`ModifierChangeBatchId`、`SourceHandle.Id` 的分配逻辑临时下沉到 `Component static`，该方案只视为过渡实现，不视为最终架构；后续运行时实例身份与网络同步设计由独立的 `runtime-network-identity` capability 收敛

## 本轮已写硬到 spec 的设计细节
1. `UTcsDefinitionManagerSubsystem` 的 public 查询面已明确要求提供按 `DefId` 的类型化入口
2. 按 tag 查询的边界已收紧到当前已有 runtime 语义支撑的 `BuffDef` / `StateSlotDef`
3. 所有 DefAsset 都已被锁定到统一三种加载策略；预加载固定异步，运行时加载推荐异步并提供同步补充能力
4. `UTcsDeveloperSettings` 退出 Def 缓存基站、`UTcsDefinitionEditorManagerSubsystem` 接管编辑器期桥接/缓存/调度，以及 editor/runtime 强断开边界，已进入本 change 的 delta spec
5. 具体非抽象 DefAsset 类型成为加载配置与 AssetManager 建模的基本单位；抽象 `StateDef` 不再提供直接 public 查询接口，这一点已进入本 change 的 delta spec
6. 归档门槛已明确要求清零 deprecated wrapper、对象型 public 主入口与遗留 Definition 查询包装器
7. 统一 Definition 查询失败语义、日志责任层级，以及 editor registry / runtime manager 的强解耦边界，已进入本 change 的 delta spec

## 具体 DefAsset 作为加载基本单位的建议
- 当前可实例化、应进入统一加载配置面的 DefinitionAsset 至少包括：
	- `UTcsBuffDefinition`
	- `UTcsSkillDefinition`
	- `UTcsStateSlotDefinition`
	- `UTcsAttributeDefinition`
	- `UTcsAttributeModifierDefinition`
	- `UTcsSkillModifierDefinition`
- 这些类型应成为：
	- 加载策略配置的基本单位
	- source cache 的基本单位
	- 预加载 / 按需加载策略的基本单位
	- AssetManager `PrimaryAssetType` 与扫描路径的基本单位
- 抽象 `UTcsStateDefinition` 不应再作为上述任何一类“中心配置族”存在。

## `StateDefinition` 语义的收敛建议
- `UTcsStateDefinition` 仍可保留为语义基类，因为 `BuffDef` / `SkillDef` 本质上仍然是 state-like definitions。
- 但这层保留只能体现在：
	- 继承关系
	- 少量与 state-like 行为确实相关的共享契约
- `StateDefId` 如果最终仍有存在价值，也只应收紧在 `State` 模块内部，用于单个 state definition 在 `StateComponent` 上的运行生命周期。
- 不能继续体现在：
	- 独立 `PrimaryAssetType`
	- 独立扫描目录中心
	- 独立加载配置族
	- 对 Blueprint 或其他非 State 模块调用方暴露的直接 public runtime 查询入口
	- Buff 相关 public API 的 DefId 命名
	- `UTcsDefinitionManagerSubsystem` 可以维护从具体 Buff / Skill source cache 与 loaded cache 派生出的 `StateDefinitionSources` / `StateDefinitions` 聚合索引，供 State 模块内部 `StateDefId` 高频查询。它们不独立扫描 AssetManager、不定义独立加载策略，也不替代具体类型缓存。

## AssetManager 建模建议
- 当前实现已经明显依赖 `AssetManager` 进行 Definition 发现与 runtime 加载，因此配置层与实现层必须对齐。
- 如果采用“非抽象 DefAsset 类型 -> LoadingConfig”映射，那么 `AssetManagerSettings` 也必须跟着拆分为同样粒度：
	- `TcsBuffDef`
	- `TcsSkillDef`
	- `TcsStateSlotDef`
	- `TcsAttributeDef`
	- `TcsAttributeModifierDef`
	- `TcsSkillModifierDef`
- `BuffDef` / `SkillDef` 不应继续共同挂在抽象 `TcsStateDef` 的扫描路径下。
- 如果后续还需要 state-like 聚合语义，应在 runtime `UTcsDefinitionManagerSubsystem` 内部做聚合，而不是回到 AssetManager 级别继续使用抽象家族中心化建模。

## Editor / Runtime 边界建议

### `UTcsDefinitionEditorManagerSubsystem` 应负责
- 编辑器期 DefAsset / DataTable 的桥接协调
- 避免互相回写造成递归循环
- 编辑器期受管 Def 的缓存、索引、脏标记、更新队列
- 编辑器期事件监听与统一调度

### `UTcsDefinitionEditorManagerSubsystem` 不应负责
- runtime authoritative cache
- gameplay runtime Def 生命周期
- 把 editor refresh 直接定义成 runtime refresh 契约
- 代替各具体 DefAsset 的 `IsDataValid()`
- 把所有 authoring 规则集中成万能校验器
- 为 runtime `UTcsDefinitionManagerSubsystem` 提供桥接快照来源

## 运行时加载模型建议
- `UTcsDefinitionManagerSubsystem` 必须是唯一运行时 authoritative cache base。
- 所有 DefAsset 都必须遵循统一三种加载策略：
	- 全部预加载
	- 只预加载特定资产
	- 完全不预加载
- 预加载必须走异步主路径。
- 运行时加载应以异步加载作为推荐主方案，并允许显式同步补充方案存在。
- 单个 DefAsset 的按需解析仍必须可独立触发；异步路径是推荐主路径，同步路径是阻塞式补充能力。
- 同步加载函数（`Load...DefinitionSync`）作为纯内部实现细节放在 `protected` 域下，外部统一通过 `Get...Definition` 隐式触发；这保证调用方默认走 loaded cache 或异步路径，不会意外引入阻塞加载。

## `SkillEntry` 权威对象建议
- `SkillEntry` 作为运行时实例，不应只把 `SkillDefId` 当作唯一运行时真相，而把真实 `SkillDef` 每次都重新查回。
- 更合理的模型是：
	- 对外身份、存档、同步、授予入口仍使用 `SkillDefId`
	- 但当 `SkillEntry` 已经完成合法解析后，实例内部缓存一个已校验的权威 `UTcsSkillDefinition*`
- 这个缓存不是身份来源，而是运行时实例已完成解析后的权威对象句柄，用来减轻重复读取负载。
- 如果缓存失效，恢复路径仍应回到 `SkillDefId -> DefinitionManager -> 合法 SkillDef` 的解析链，而不是允许裸指针脱离 `DefId` 约束自行漂移。

## `DeveloperSettings` 角色收窄建议
- `UTcsDeveloperSettings` 应仅保留：
	- 运行时/编辑器共享的配置读取
	- 策略开关
	- authoring / tool 流程所需的设置项
- `UTcsDeveloperSettings` 不应再保留：
	- runtime cached defs
	- editor 桥接缓存快照
	- Def 会话态索引
	- runtime bootstrap authoritative source

## 统一失败语义建议
- 所有按 `DefId` 驱动的主执行路径，都必须共享同一条底线：**Def 解析失败就是主流程失败**。
- 不允许的行为：
	- 静默返回成功
	- 构造残缺占位 `Entry` / `State` / `Modifier`
	- 在没有合法 Definition 的情况下继续进入后续参数求值或 apply 逻辑
	- deprecated wrapper 私自改写失败语义
- 允许的行为：
	- 查询层返回显式失败结果（例如空指针 / 未命中 / 明确失败状态）
	- façade 或 gameplay API 把该失败翻译成它本身 public surface 合法的失败返回（例如 `false`、空句柄、无效结果）
- 但无论外层 API 如何编码失败，**都不得把 Definition 解析失败伪装成成功执行过**。

## 日志与包装器策略建议
- deprecated wrapper 不应保留旧时代的“对象型主路径独立行为”；它只能转发到新的 DefId 主路径。
- 如果 wrapper 收到空对象、非法对象或无法提取合法 `DefId` 的输入，它必须遵循统一失败语义并发出确定性的失败信号。
- 不建议让每层都各打一次错误日志，否则迁移期会制造噪音；更合理的是由主执行路径或统一 Definition 查询层负责发出一次权威失败诊断，其余层只透传失败。
- 权威失败诊断至少应包含三项固定信息：目标 `DefId`（或等价查询 key）、发起入口名、失败类别（未注册 / 类型不匹配 / 加载失败）。

## editor registry 与 runtime manager 解耦建议
- editor registry 是编辑器期权威快照能力，不应直接定义 runtime definition manager 的生命周期。
- runtime 不应假定存在通用 refresh 过程；在正常 runtime 语义下，也不应把 DefAsset 的增删改查建模为运行时契约。
- runtime 不得依赖 `UTcsDefinitionEditorManagerSubsystem` 或 `UTcsDeveloperSettings` cached defs 作为任何形式的引导快照来源。
- 不允许 `StateManagerSubsystem`、`AttributeManagerSubsystem` 或其他 gameplay manager 继续以“消费 editor registry 刷新”为由保留隐式 runtime cache 同步责任。
- 如果未来 editor-hosted 调试工具需要显式重建某个 runtime 测试视图，那也应被视为独立工具链/编辑器流程，而不是 `UTcsDefinitionManagerSubsystem` 的通用 runtime 契约。

## DefinitionManager public 查询面建议
- 所有进入统一归口的 Definition 类型都必须至少有按 `DefId` 的类型化查询入口。
- 第一阶段建议显式要求以下查询面：
	- `GetBuffDefinition(FName BuffDefId)`
	- `GetBuffDefinitionByTag(FGameplayTag BuffTag)`
	- `GetSkillDefinition(FName SkillDefId)`
	- `GetStateSlotDefinition(FName StateSlotDefId)`
	- `GetStateSlotDefinitionByTag(FGameplayTag StateSlotTag)`
	- `GetAttributeDefinition(FName AttributeDefId)`
	- `GetAttributeModifierDefinition(FName AttributeModifierDefId)`
	- `GetSkillModifierDefinition(FName SkillModifierDefId)`
- 这里的函数名是规格层期望的 public surface 表达，不要求实现阶段逐字照抄同名，也不强制拆成 `Find` / `Get` / `TryResolve` 三套命名，但必须体现出足够全面的一一对应类型化能力。
- 第一阶段不建议为 `SkillDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 发明新的 tag 查询义务，因为当前讨论里没有证据表明它们已经存在稳定的 tag 驱动主路径；硬加只会扩大范围。

## 跨 Actor facade 命名建议
- 如果 `UTcsStateManagerSubsystem` 继续保留跨 Actor apply facade，需要先区分它究竟是在表达：
	- 抽象 state-like 运行时统一语义
	- 还是实际上的 Buff apply 门面
- 按当前已确认方向，Buff 相关 public API 既然已经固定使用 `BuffDefId`，那么这类 facade 若主要服务于 Buff apply，也应同步改为类似 `TryApplyBuffToTarget(TargetActor, BuffDefId, ...)` 的 public surface。
- `TryApplyStateToTarget(..., StateDefId, ...)` 这种命名会继续向外暴露旧的抽象 `StateDef` 语义，应避免保留在最终 public API 面上。

## 迁移与归档验收建议
- `UTcsStateManagerSubsystem` 归档时不应再保留任何 Definition 查询 public API，包括按 `DefId` / 按 tag 的 state/state-slot 查询包装器。
- `LearnSkill(UTcsSkillDefinition*)`、对象型 `ActivateSkill` 包装、对象型 `ApplyBuff` 包装、对象型 `ApplySkillModifier` 包装，不应作为归档后 public API 保留；优先直接移除。
- 如果迁移阶段短暂保留这些逻辑，它们也只能退化为内部辅助转发实现，而不能继续对外承担 public 主入口职责。
- 如果运行时仍存在必须依赖对象型 public 主入口的调用点，说明迁移尚未完成，该 change 不应归档。

## 初步推荐
- 推荐新增统一 `UGameInstanceSubsystem` 级别的 Definition 管理子系统 `UTcsDefinitionManagerSubsystem`。
- 推荐把现有 `UTcsDefAssetDataTableSyncSubsystem` 升级并重命名为 `UTcsDefinitionEditorManagerSubsystem`，而不是再平行叠加新的 EditorSubsystem。
- 推荐 `SkillDefId` 继续承担对外身份与持久化职责，但 `SkillEntry` 实例内部缓存一个已校验的权威 `SkillDefinition*` 运行时对象。
- 推荐所有 DefAsset 统一遵循 `UTcsDeveloperSettings` 的三种加载策略，且预加载固定异步。
- 推荐运行时加载默认优先异步，同时保留显式同步补充接口。
- 推荐 `UTcsDeveloperSettings` 只保留设置读取职责，不再承担 Def 缓存基站角色。
- 推荐 AssetManager 建模与加载配置粒度一致，直接拆到具体非抽象 DefAsset 类型，而不是继续让 `BuffDef` / `SkillDef` 共同挂在抽象 `TcsStateDef` 家族中心下面。
- 推荐不要为了单纯的 ID 发号行为额外新增 runtime 子系统；当前版本先把身份语义收敛清楚，再在后续真正需要时决定是否引入统一的 identity / reconcile 账本实现。
- 推荐直接取消对象型 public 主入口；若迁移阶段内部确实需要短暂转发逻辑，也应限定为内部辅助实现，并把“归档前清零”写入任务与验收标准。
- 推荐 `ApplyBuff` / `ApplySkillModifier` 与 `LearnSkill` / `ActivateSkill` 一样，以 DefId 作为主执行路径输入。
