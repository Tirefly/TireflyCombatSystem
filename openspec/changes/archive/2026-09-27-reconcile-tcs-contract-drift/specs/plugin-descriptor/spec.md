## MODIFIED Requirements

### Requirement: R3 模块物化声明

插件描述文件 MUST 声明当前 R3 竖切实际物化的七个 Runtime 模块——`TcsCore`、`TcsNotation`、`TcsAttribute`、`TcsEffect`、`TcsTargeting`、`TcsDamage`、`TcsIntegration`——并按依赖序排列，LoadingPhase 均为 `Default`。R0 的 `TcsState`、`TcsSkill`、`TcsCue`、`TcsEditor` 属于目标架构模块，不得被描述成当前已经物化的模块；不得引用已删除的旧模块。

#### Scenario: 模块列表与当前 R3 实现一致

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `Modules` 数组
- **THEN** 恰好包含 `TcsCore`、`TcsNotation`、`TcsAttribute`、`TcsEffect`、`TcsTargeting`、`TcsDamage`、`TcsIntegration` 七项 Runtime 模块，且按依赖序排列

#### Scenario: 目标模块不被误报为当前实现

- **WHEN** 对照 `openspec/project.md` 的目标架构与 `Source/` 目录
- **THEN** `TcsState`、`TcsSkill`、`TcsCue`、`TcsEditor` 被标为目标模块或后续轮次，不出现在当前物化模块清单中

#### Scenario: 旧模块不复活

- **WHEN** 检查插件描述文件全文
- **THEN** 不存在 `TireflyCombatSystem`、`TireflyCombatSystemEditor` 等已删除旧模块条目
