# plugin-descriptor Specification

## Purpose
TBD - created by archiving change update-uplugin-for-r3-modules. Update Purpose after archive.
## Requirements
### Requirement: 引擎版本声明

插件描述文件 MUST 声明 `"EngineVersion": "5.8"`，与宿主工程 LegendAutoChess.uproject 解析出的引擎（关联 GUID → `E:/UnrealEngine/UE_5.8`）保持一致。

#### Scenario: 引擎版本与宿主一致

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `EngineVersion` 字段
- **THEN** 其值为 `"5.8"`，与宿主工程解析出的引擎版本一致

### Requirement: R3 模块物化声明

插件描述文件 MUST 仅声明三个 Runtime 模块——`TcsCore`、`TcsNotation`、`TcsAttribute`——按依赖序排列（TcsCore → TcsNotation → TcsAttribute），LoadingPhase 均为 `Default`；不得引用任何已删除的旧模块。

#### Scenario: 模块列表与 R0 §9 R3 子集一致

- **WHEN** 读取 `.uplugin` 的 `Modules` 数组
- **THEN** 恰好包含 TcsCore、TcsNotation、TcsAttribute 三项，Type 均为 `Runtime`、LoadingPhase 均为 `Default`，且不含 TireflyCombatSystem / TireflyCombatSystemEditor 等旧模块条目

### Requirement: 无插件级依赖

R3 三模块基线的 `.uplugin` MUST NOT 声明 `Plugins` 数组——事件总线与对象池内置 TcsCore（R0 §9 基础设施内置规定），引擎插件依赖（StateTree / GameplayStateTree）由 TcsIntegration 落地时回补。

#### Scenario: 幽灵依赖清除

- **WHEN** 检查 `.uplugin` 全文
- **THEN** 不存在 `Plugins` 数组，GameplayMessageRouter 等旧外部插件不再被引用

