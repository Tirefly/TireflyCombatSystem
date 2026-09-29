# plugin-descriptor Specification

## Purpose
定义插件描述符必须声明的三条内容：引擎版本（UE 5.8）、R3 物化的编译模块集合，以及「插件级零依赖」的边界，避免模块依赖被误升为插件依赖。
## Requirements
### Requirement: 引擎版本声明

插件描述文件 MUST 声明 `"EngineVersion": "5.8"`，与宿主工程 LegendAutoChess.uproject 解析出的引擎（关联 GUID → `E:/UnrealEngine/UE_5.8`）保持一致。

#### Scenario: 引擎版本与宿主一致

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `EngineVersion` 字段
- **THEN** 其值为 `"5.8"`，与宿主工程解析出的引擎版本一致

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

### Requirement: 无插件级依赖

R3 三模块基线的 `.uplugin` MUST NOT 声明 `Plugins` 数组——事件总线与对象池内置 TcsCore（R0 §9 基础设施内置规定），引擎插件依赖（StateTree / GameplayStateTree）由 TcsIntegration 落地时回补。

#### Scenario: 幽灵依赖清除

- **WHEN** 检查 `.uplugin` 全文
- **THEN** 不存在 `Plugins` 数组，GameplayMessageRouter 等旧外部插件不再被引用
