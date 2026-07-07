## MODIFIED Requirements
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
