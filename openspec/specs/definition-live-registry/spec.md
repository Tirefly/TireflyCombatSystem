# definition-live-registry 规范

## Purpose
定义 `UTcsDefinitionRegistrySubsystem` 作为编辑器期 Definition 权威快照持有者，按具体非抽象 DefAsset 类型维护 Attribute、AttributeModifier、Buff、Skill、StateSlot、SkillModifier 的实时 registry。编辑器桥接与调度由 `UTcsDefinitionEditorManagerSubsystem` 承担；`UTcsDeveloperSettings` 只保存配置，不镜像 Definition 快照。runtime Definition cache/load 由 `UTcsDefinitionManagerSubsystem` 从 AssetManager 建立，不因 editor registry 刷新而自动重建。
## Requirements
### Requirement: 编辑器期实时 Definition 感知
TCS SHALL 在编辑器会话期间维护一份权威的实时 Def 快照，使 Def 变化无需重启编辑器即可被观察到。

#### Scenario: 新建 AttributeDef 在同一编辑器会话内可见
- **WHEN** 开发者在编辑器中新建一个 AttributeDef 资产
- **THEN** 权威 TCS Def 快照会在同一编辑器会话内刷新
- **AND** 新的 AttributeDef 会在无需重启编辑器的前提下，对编辑器期 TCS 加载路径可见

#### Scenario: 具体 state-side Definition 的更新在同一编辑器会话内可见
- **WHEN** 开发者在编辑器中修改并保存一个具体的 state-side DefinitionAsset（例如 `UTcsBuffDefinition`，或任意派生自 `UTcsStateDefinition` 的资产）
- **THEN** 权威 TCS Def 快照会在同一编辑器会话内刷新
- **AND** 更新后的 state-side DefinitionAsset 会在无需重启编辑器的前提下，对编辑器期 TCS 加载路径可见

### Requirement: Subsystem 实时同步

TCS 的编辑器期 Definition 快照消费方在 registry 刷新时 SHALL 从最新的权威 Def 快照中重建，但这种重建职责 MUST 仅落在编辑器期消费方上，不得再要求 `UTcsStateManagerSubsystem` 或 runtime `UTcsDefinitionManagerSubsystem` 直接承担编辑器快照同步职责。编辑器期权威快照、编辑器期管理中枢与运行时 Definition 加载归口 SHALL 强解耦。

#### Scenario: 运行时 Definition 归口不再绑死在 StateManager 上
- **WHEN** 权威 TCS Def 快照在编辑器中刷新时
- **THEN** 编辑器期消费方 MAY 从刷新后的快照中重建其查询状态
- **AND** `UTcsStateManagerSubsystem` MUST NOT 因此继续保留运行时 Definition cache/load 归口职责

#### Scenario: Editor manager 是编辑器期快照消费中枢
- **WHEN** 权威 TCS Def 快照在编辑器中刷新时
- **THEN** `UTcsDefinitionEditorManagerSubsystem` MAY 作为编辑器期快照消费与桥接调度中枢，重建其编辑器态查询视图
- **AND** 这种刷新 MUST NOT 被定义为 runtime definition manager 的通用 cache 生命周期契约

#### Scenario: 刷新后旧 cache 不得继续冒充最新视图
- **WHEN** 权威 TCS Def 快照已刷新，但某些旧 runtime cache 仍然存在于内存中
- **THEN** 编辑器期消费方的旧快照视图 MUST NOT 继续冒充最新编辑器态查询结果
- **AND** 该约束 MUST NOT 被扩大解释为 runtime contract 需要支持通用 refresh/rebuild 生命周期

#### Scenario: 刷新期间失败诊断保持一致
- **WHEN** 某次 runtime Definition 查询恰好发生在 registry 刷新后的重建窗口内
- **THEN** 该刷新流程 SHOULD 只约束编辑器期快照消费方的一致性
- **AND** 系统 MUST NOT 因此暗示 runtime definition manager 存在通用 refresh 窗口或 runtime rebuild 契约

#### Scenario: DeveloperSettings 不再充当 registry 快照缓存基站
- **WHEN** 编辑器期 registry 刷新并触发相关消费方重建
- **THEN** 系统 MUST NOT 要求把最新 Def 快照落回 `UTcsDeveloperSettings` 作为缓存基站
- **AND** 该快照消费与调度责任 SHOULD 由 `UTcsDefinitionEditorManagerSubsystem` 承担

#### Scenario: AssetManager 覆盖检查必须按具体非抽象 DefAsset 类型拆分
- **WHEN** 编辑器期 registry 或相关校验逻辑检查 `AssetManagerSettings` 对 TCS DefinitionAsset 的覆盖情况
- **THEN** 它 MUST 按 `BuffDef`、`SkillDef`、`StateSlotDef`、`AttributeDef`、`AttributeModifierDef`、`SkillModifierDef` 等具体非抽象 DefAsset 类型分别检查
- **AND** 它 MUST NOT 继续默认接受把 `BuffDef` / `SkillDef` 共同挂在抽象 `TcsStateDef` 扫描路径下的旧建模

### Requirement: 派生 Def 兼容性

TCS 编辑器期 Definition registry SHALL 按各受管具体非抽象 DefAsset 类型及其可派生类建立快照，而不是将抽象 `UTcsStateDefinition` 建模为独立 AssetManager 扫描、运行时 cache 或加载策略中心。registry 刷新只更新编辑器期快照消费方，MUST NOT 驱动 runtime Definition cache 重建。

#### Scenario: 派生具体 Buff 或 Skill Definition 能建立编辑器索引
- **WHEN** 项目定义了派生自 `UTcsBuffDefinition` 或 `UTcsSkillDefinition` 的具体资产类，且资产位于对应扫描目录
- **THEN** 编辑器期 registry MUST 将该资产索引到对应具体 Definition 类型快照
- **AND** MUST NOT 因此为抽象 `UTcsStateDefinition` 新增独立 PrimaryAssetType、扫描目录或 runtime cache

#### Scenario: 派生 Attribute Definition 能建立编辑器索引
- **WHEN** 项目定义了派生自 `UTcsAttributeDefinition` 或 `UTcsAttributeModifierDefinition` 的具体资产类，且资产位于对应扫描目录
- **THEN** 编辑器期 registry MUST 将该资产索引到对应具体 Definition 类型快照
- **AND** runtime `UTcsDefinitionManagerSubsystem` MUST 继续仅从 AssetManager 建立自身 source cache

