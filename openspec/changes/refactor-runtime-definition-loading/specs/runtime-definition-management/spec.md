## ADDED Requirements
### Requirement: 统一的运行时 Definition 管理子系统

TCS SHALL 新增 `UTcsDefinitionManagerSubsystem` 作为统一的运行时 Definition 加载归口，用于承载多类 TCS DefinitionAsset 的 source cache、预加载策略、按需加载策略与类型化查询入口。

#### Scenario: 运行时 Definition 查询不再分散在多个 gameplay manager 中
- **WHEN** 运行时代码需要解析 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef` 或 `SkillModifierDef`
- **THEN** 它 SHOULD 通过 `UTcsDefinitionManagerSubsystem` 提供的类型化查询入口完成解析
- **AND** `UTcsStateManagerSubsystem` / `UTcsAttributeManagerSubsystem` MUST NOT 继续各自持有独立的运行时 Definition cache/load 归口

#### Scenario: Runtime authoritative cache base 必须唯一
- **WHEN** 运行时系统建立多类 DefAsset 的缓存与加载状态
- **THEN** `UTcsDefinitionManagerSubsystem` MUST 作为唯一运行时 authoritative cache base
- **AND** `UTcsDeveloperSettings`、`UTcsDefinitionEditorManagerSubsystem` 或其他 editor-only 组件 MUST NOT 承担该职责

### Requirement: 统一三种加载策略与异步预加载契约

`UTcsDefinitionManagerSubsystem` SHALL 让所有受管 DefAsset 统一遵循 `UTcsDeveloperSettings` 中的三种加载策略：全部预加载、只预加载特定资产、完全不预加载。预加载必须走异步主路径；运行时加载 SHOULD 推荐异步方案，同时 MAY 提供显式同步补充方案。

#### Scenario: 预加载默认走异步主路径
- **WHEN** 系统需要为某类或某批 DefAsset 执行预加载
- **THEN** `UTcsDefinitionManagerSubsystem` MUST 提供异步预加载主路径
- **AND** 调用方 MUST NOT 被要求默认使用同步阻塞加载来完成这类预加载

#### Scenario: 全部 DefAsset 都必须受统一三种策略约束
- **WHEN** 系统为 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 配置加载行为
- **THEN** 它 MUST 落在“全部预加载 / 只预加载特定资产 / 完全不预加载”三种策略之内
- **AND** 系统 MUST NOT 为部分 DefAsset 继续保留脱离这三种策略的旧加载模型

#### Scenario: 单个 DefAsset 的按需解析推荐走异步主路径
- **WHEN** 运行时第一次解析某个尚未加载的单个 DefAsset
- **THEN** `UTcsDefinitionManagerSubsystem` MUST 提供单资产粒度的按需异步加载主路径
- **AND** 该能力 MUST 适用于所有进入统一归口的 DefAsset 类型，而不只限于 `BuffDef`

#### Scenario: 同步加载只能作为显式补充能力
- **WHEN** 某些调用点因兼容性或特殊约束需要同步阻塞解析 Definition
- **THEN** 系统 MAY 提供显式同步加载接口或包装路径
- **AND** 这些同步路径 MUST NOT 被写成预加载主契约，也 MUST NOT 让推荐异步路径名存实亡

### Requirement: 加载配置必须以具体非抽象 DefAsset 类型为基本单位

运行时加载配置、source cache、预加载策略、按需加载策略与 AssetManager 建模 SHALL 以具体非抽象 DefAsset 类型为基本单位，而不是以抽象基类为基本单位。

#### Scenario: 当前非抽象 DefAsset 类型必须被单独建模
- **WHEN** 系统为当前 TCS DefinitionAsset 设计统一加载配置面
- **THEN** 它 MUST 至少覆盖 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 这些当前非抽象 DefAsset 类型
- **AND** 它 MUST NOT 只围绕抽象 `UTcsStateDefinition` 建立加载配置中心

#### Scenario: 抽象 StateDefinition 不再作为独立加载族或直接查询面
- **WHEN** `UTcsStateDefinition` 已经是抽象基类
- **THEN** 系统 MUST NOT 再把它建模为独立的加载配置族、独立的 runtime source cache 或独立的 AssetManager 扫描中心
- **AND** 系统 MUST NOT 继续暴露直接查询抽象 `UTcsStateDefinition` 的 public runtime 接口

#### Scenario: AssetManager 粒度必须与加载配置粒度一致
- **WHEN** 运行时加载实现继续依赖 `AssetManager`
- **THEN** `AssetManagerSettings` 中的 `PrimaryAssetType` 与扫描路径 MUST 与具体非抽象 DefAsset 粒度保持一致
- **AND** `BuffDef` 与 `SkillDef` MUST NOT 继续共同挂在抽象 `TcsStateDef` 的扫描路径下

### Requirement: 类型化查询入口

`UTcsDefinitionManagerSubsystem` SHALL 为进入统一归口的 Definition 类型提供清晰的类型化查询入口，而不是要求调用方通过单一弱类型接口手工分发。

#### Scenario: State / Skill / Modifier 查询面可区分
- **WHEN** 调用方分别需要解析 `UTcsStateDefinition`、`UTcsSkillDefinition` 或 `UTcsSkillModifierDefinition`
- **THEN** 子系统 MUST 提供对应的类型化查询入口
- **AND** 调用方 MUST NOT 被迫先拿到通用 `UPrimaryDataAsset*` 再手工 Cast 才能完成主执行路径

#### Scenario: 第一阶段至少提供按 DefId 的显式查询面
- **WHEN** 第一阶段完成 `UTcsDefinitionManagerSubsystem` 的 public 查询面定义
- **THEN** 它 MUST 至少覆盖 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 的按 `DefId` 类型化查询
- **AND** 这些能力 MUST 以一一对应的类型化入口暴露，而不是合并成一个要求调用方自己分流的弱类型总入口

#### Scenario: 查询接口命名层级不作硬拆分但覆盖必须全面
- **WHEN** 设计 `UTcsDefinitionManagerSubsystem` 的最终查询 API 命名
- **THEN** 系统 MAY 不强制区分 `Find` / `Get` / `TryResolve` 三套命名层级
- **AND** 最终保留的类型化查询面 MUST 仍足够全面，能够覆盖各主执行路径所需的 Definition 解析能力

#### Scenario: 仅延续已有 tag 语义而不无端扩面
- **WHEN** 第一阶段定义哪些 Definition 类型需要按 tag 查询
- **THEN** `BuffDef` / `StateSlotDef` MUST 继续保留与当前运行时语义兼容的按 tag 查询能力
- **AND** `SkillDef` / `AttributeDef` / `AttributeModifierDef` / `SkillModifierDef` MUST NOT 在没有既有 runtime 语义依据的前提下被强制要求新增 tag 查询主路径

### Requirement: `SkillEntry` 内部权威 Definition 缓存

`UTcsSkillEntry` SHALL 保持 `SkillDefId` 作为对外身份与主路径输入，但在运行时实例内部持有一个由合法 `SkillDefId` 解析并校验过的权威 `UTcsSkillDefinition*` 缓存，以降低重复读取负载。

#### Scenario: SkillEntry 实例内部缓存合法 SkillDef
- **WHEN** `UTcsSkillEntry` 已通过合法 `SkillDefId` 完成 Definition 解析
- **THEN** 它 MUST 缓存对应的已校验 `UTcsSkillDefinition*`
- **AND** 该缓存 MUST 被视为该运行时实例内部的权威 SkillDef 对象

#### Scenario: SkillEntry 外部身份仍由 DefId 驱动
- **WHEN** `UTcsSkillEntry` 参与 learn、activate、save、load、replicate 或其他主路径流转
- **THEN** 它的对外身份与输入输出契约 MUST 仍以 `SkillDefId` 为准
- **AND** 内部缓存的 `UTcsSkillDefinition*` MUST NOT 取代 `SkillDefId` 的外部身份职责

### Requirement: 统一 Definition 查询失败语义

`UTcsDefinitionManagerSubsystem` SHALL 为按 `DefId` / 按 tag 的运行时 Definition 查询提供统一失败语义。Definition 解析失败时，系统必须显式失败，不得静默返回成功、伪造占位 Definition，或允许调用方把失败误判为已成功取得合法 Definition。

#### Scenario: Definition 查询失败时返回显式失败而不是伪造对象
- **WHEN** 调用方按 `DefId` 或按 tag 查询 Definition，而目标 Definition 不存在、未注册、类型不匹配或解析失败
- **THEN** 查询层 MUST 返回显式失败结果
- **AND** 查询层 MUST NOT 伪造占位 `Definition` 对象供后续主路径继续使用

#### Scenario: Gameplay facade 只能翻译失败，不能掩盖失败
- **WHEN** 上层 gameplay API 使用 `UTcsDefinitionManagerSubsystem` 执行 DefId 主路径，而底层 Definition 查询失败
- **THEN** 上层 API MAY 将其翻译成其 public surface 合法的失败结果（例如 `false`、无效句柄或其他明确失败态）
- **AND** 上层 API MUST NOT 把该失败表现为已经成功执行 apply、learn 或 activate

#### Scenario: 失败诊断必须可确定且不应层层刷屏
- **WHEN** Definition 查询失败经过 wrapper、facade 与主执行路径逐层传播
- **THEN** 系统 SHOULD 只保留一条权威失败诊断来源
- **AND** deprecated wrapper MUST NOT 通过各自重复记录错误而制造多层噪音

#### Scenario: 权威失败诊断必须带固定字段
- **WHEN** Definition 查询或 DefId 主路径发生失败诊断
- **THEN** 该诊断 MUST 至少包含目标 `DefId` 或等价查询 key、发起入口名、失败类别
- **AND** 失败类别 MUST 至少能区分“未注册”“类型不匹配”“加载失败”

### Requirement: 编辑器期 registry 与运行时管理层解耦

编辑器期的 `UTcsDefinitionRegistrySubsystem` SHALL 继续作为权威快照持有者，但运行时 `UTcsDefinitionManagerSubsystem` 的职责 MUST 与编辑器期实时 registry 能力分离。

#### Scenario: 运行时管理层不等于编辑器期 registry
- **WHEN** 设计或实现 `UTcsDefinitionManagerSubsystem`
- **THEN** 它 MUST 被视为运行时 Definition cache/load 与查询归口
- **AND** 它 MUST NOT 被建模为 `UTcsDefinitionRegistrySubsystem` 的别名、薄重命名或编辑器专用逻辑容器

#### Scenario: runtime contract 不定义通用 refresh 生命周期
- **WHEN** 设计 `UTcsDefinitionManagerSubsystem` 的 runtime contract
- **THEN** 它 MUST NOT 假定运行时存在通用的 DefAsset refresh / reload 生命周期
- **AND** 它 MUST NOT 因 editor registry 刷新而自动引入通用 `RuntimeRefresh` 语义

#### Scenario: Runtime 不得依赖 editor subsystem 或 DeveloperSettings cached defs 引导
- **WHEN** 运行时 `UTcsDefinitionManagerSubsystem` 建立自身的 source cache、加载状态或运行时查询面
- **THEN** 它 MUST NOT 依赖 `UTcsDefinitionEditorManagerSubsystem` 提供桥接快照来源
- **AND** 它 MUST NOT 依赖 `UTcsDeveloperSettings` 中的 cached defs 作为 runtime authoritative source

#### Scenario: editor 工具链重建不等于 runtime 通用契约
- **WHEN** 未来某个 editor-hosted 调试工具需要显式重建测试视图
- **THEN** 该行为 MAY 作为独立编辑器工具链流程存在
- **AND** 它 MUST NOT 被写成 `UTcsDefinitionManagerSubsystem` 的通用 runtime contract

### Requirement: 迁移归档前必须清零兼容包装

如果某些旧 manager 或 gameplay API 在迁移期短暂保留 deprecated Definition 查询/对象型包装逻辑，这些逻辑 SHALL 只作为内部过渡层存在，并必须在 change 归档前清零对外 public API 面。

#### Scenario: 运行时 Definition 查询包装器不能带入归档态
- **WHEN** 检查该 change 是否可以归档
- **THEN** `UTcsStateManagerSubsystem` MUST NOT 仍保留任何 Definition 查询 public API 包装器
- **AND** 若仍存在这类包装器，则该 change MUST 视为未完成

#### Scenario: 对象型 gameplay 主入口不能带入归档态
- **WHEN** 检查 `LearnSkill` / `ActivateSkill` / `ApplyBuff` / `ApplySkillModifier` 的最终 public API 面
- **THEN** 对象型 public 入口 MUST 已清零
- **AND** 若迁移仍依赖这些对象型 public 入口维持核心主流程，则该 change MUST 视为未完成
