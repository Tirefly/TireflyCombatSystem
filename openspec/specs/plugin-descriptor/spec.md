# plugin-descriptor Specification

## Purpose
定义插件描述符必须声明的三条内容：引擎版本（UE 5.8）、**当前物化**的编译模块集合（现况声明，随轮次推进——R3 七模块 → R5 八模块），以及「插件级零依赖」的边界，避免模块依赖被误升为插件依赖。

## Requirements

### Requirement: 引擎版本声明

插件描述文件 MUST 声明 `"EngineVersion": "5.8"`，与宿主工程 LegendAutoChess.uproject 解析出的引擎（关联 GUID → `E:/UnrealEngine/UE_5.8`）保持一致。

#### Scenario: 引擎版本与宿主一致

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `EngineVersion` 字段
- **THEN** 其值为 `"5.8"`，与宿主工程解析出的引擎版本一致

### Requirement: 模块物化声明

插件描述文件 MUST 声明当前实际物化的**九个** Runtime 模块——`TcsCore`、`TcsNotation`、`TcsAttribute`、`TcsEffect`、`TcsState`、`TcsTargeting`、`TcsDamage`、`TcsIntegration`、`TcsSkill`——并按依赖序排列，LoadingPhase 均为 `Default`。

模块清单是**随轮次推进的现况声明**（R3 七模块 → R5 加入 `TcsState`，2026-10-04 → R6 加入 `TcsSkill`，2026-10-06）：它 MUST 如实反映"今天物化了哪些模块"，MUST NOT 被读成"目标架构的固定清单"，也 MUST NOT 为了凑目标数把未物化的模块提前写进去。

R0 的 `TcsCue`、`TcsEditor` 仍属目标架构模块，不得被描述成当前已经物化的模块；不得引用已删除的旧模块。

**`TcsSkill` 的落位口径（2026-10-06 R6 Task 1）**：它 MUST 追加在 `Modules` **数组末尾**（`TcsIntegration` 之后），MUST NOT 插在 `TcsState` 与 `TcsTargeting` 之间。理由：`TcsSkill` 是依赖链**第五层终点**，而既有八条已按依赖序排列——**按依赖序排在前/后都可加载**（无模块在加载期读它），取末尾可保持既有八条顺序**一字不动**，把改动面压到最小。**这是"可加载性"判据下的取舍，不是对依赖序的否认**：`TcsSkill` 依赖 `TcsState`，该依赖由 `TcsSkill.Build.cs` 的 `PublicDependencyModuleNames` 声明，不靠 `Modules` 数组顺序表达。

#### Scenario: 模块列表与当前 R3 实现一致

> 场景标题里的 "R3" 是建立时的轮次标签（开源规格的 `#### Scenario:` 标题在 MODIFIED 语义下不可改名）——本场景的判据随模块清单一起现况化。

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `Modules` 数组
- **THEN** 恰好包含上列九个 Runtime 模块，LoadingPhase 均为 `Default`；既有八条（`TcsCore` … `TcsIntegration`）的顺序与 R5 时**逐字相同**，`TcsSkill` 位于数组**末尾**

#### Scenario: 目标模块不被误报为当前实现

- **WHEN** 对照 `openspec/project.md` 的目标架构与 `Source/` 目录
- **THEN** `TcsCue`、`TcsEditor` 被标为目标模块或后续轮次，不出现在当前物化模块清单中；`TcsState` 自 2026-10-04（R5 Task 1）起、`TcsSkill` 自 2026-10-06（R6 Task 1）起**已物化**，不再属于该清单

#### Scenario: 旧模块不复活

- **WHEN** 检查插件描述文件全文
- **THEN** 不存在 `TireflyCombatSystem`、`TireflyCombatSystemEditor` 等已删除旧模块条目

#### Scenario: 技能模块不反向依赖领域模块

- **WHEN** 检查 `Source/TcsSkill/TcsSkill.Build.cs` 的 `PublicDependencyModuleNames` / `PrivateDependencyModuleNames`
- **THEN** 其中**不含** `TcsDamage` 与 `TcsTargeting`（`SPEC-04-skill` §1 的依赖铁律）；含 `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect` / `TcsState`

### Requirement: 无插件级依赖

R3 三模块基线的 `.uplugin` MUST NOT 声明 `Plugins` 数组——事件总线与对象池内置 TcsCore（R0 §9 基础设施内置规定），引擎插件依赖（StateTree / GameplayStateTree）由 TcsIntegration 落地时回补。

#### Scenario: 幽灵依赖清除

- **WHEN** 检查 `.uplugin` 全文
- **THEN** 不存在 `Plugins` 数组，GameplayMessageRouter 等旧外部插件不再被引用
