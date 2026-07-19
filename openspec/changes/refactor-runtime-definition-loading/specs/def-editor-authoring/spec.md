## MODIFIED Requirements
### Requirement: 编辑器阶段 AssetManagerSettings 覆盖勘误
TCS 编辑器 authoring 集成 SHALL 在编辑器阶段检测 `AssetManagerSettings` 对 TCS DefinitionAsset 的覆盖完整性，并在漏配时通过错误日志与编辑器通知提供明确勘误提示。

#### Scenario: 检测到 Definition 类型或扫描路径漏配
- **WHEN** `PrimaryAssetTypesToScan` 未正确覆盖 `UTcsAttributeDefinition`、`UTcsAttributeModifierDefinition`、`UTcsBuffDefinition`、`UTcsSkillDefinition`、`UTcsStateSlotDefinition`、`UTcsSkillModifierDefinition` 对应的类型或扫描目录
- **THEN** 编辑器应输出可读勘误信息，明确缺失的 PrimaryAssetType 与扫描路径
- **AND** 该勘误信息至少应同时出现在错误日志与编辑器通知中
- **AND** 勘误信息应可用于直接指导开发者修正工程配置
- **AND** 系统 MUST NOT 把抽象 `UTcsStateDefinition` 作为独立 PrimaryAssetType、扫描目录或覆盖勘误对象

#### Scenario: 类型与路径漏配必须分别可见
- **WHEN** 某个 TCS DefAsset 类型已存在于 `PrimaryAssetTypesToScan`，但其扫描目录配置错误或缺失
- **THEN** 编辑器仍应报错，且该错误不得被“类型已存在”判定掩盖
- **AND** 报错信息应明确这是路径覆盖问题

#### Scenario: DevSettings 忽略列表可以抑制指定类型报错
- **WHEN** 某个 TCS DefAsset 类型被加入 `UTcsDeveloperSettings` 的勘误忽略列表
- **THEN** 该类型的类型漏配或路径漏配不应再产生勘误报错
- **AND** 其他未被忽略类型的漏配检测继续生效

#### Scenario: 勘误校验不改写工程配置
- **WHEN** 编辑器执行 `AssetManagerSettings` 覆盖勘误检查
- **THEN** 该检查应只报告配置问题，不自动改写项目 `AssetManagerSettings`

#### Scenario: 修复漏配后勘误消失
- **WHEN** 开发者按提示补齐 `AssetManagerSettings` 的缺失类型与扫描目录
- **THEN** 后续勘误检查不应再报告同一漏配项
- **AND** 既有 Definition 资产同步与加载行为保持有效

#### Scenario: 未修复漏配在常用 Save 入口重复提示
- **WHEN** 编辑器中仍存在未忽略且未修复的 DefAsset 漏配项
- **AND** 开发者执行普通单资产/单包保存、主窗口/快捷键 `Save All` 或 Content Browser 顶部工具栏 `Save All`
- **THEN** 编辑器应再次输出对应勘误提示
- **AND** 该重复提示应继续同时使用错误日志与编辑器通知两条通道
- **AND** 该重复提示行为持续到漏配被修复或该类型被加入忽略列表
