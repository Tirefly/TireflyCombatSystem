## RENAMED Requirements

- FROM: `### Requirement: R3 模块物化声明`
- TO: `### Requirement: 模块物化声明`

## MODIFIED Requirements

### Requirement: 模块物化声明

插件描述文件 MUST 声明当前实际物化的**八个** Runtime 模块——`TcsCore`、`TcsNotation`、`TcsAttribute`、`TcsEffect`、`TcsState`、`TcsTargeting`、`TcsDamage`、`TcsIntegration`——并按依赖序排列，LoadingPhase 均为 `Default`。

模块清单是**随轮次推进的现况声明**（R3 七模块 → R5 加入 `TcsState`，2026-10-04 R5 Task 1）：它 MUST 如实反映"今天物化了哪些模块"，MUST NOT 被读成"目标架构的固定清单"，也 MUST NOT 为了凑目标数把未物化的模块提前写进去。

R0 的 `TcsSkill`、`TcsCue`、`TcsEditor` 仍属目标架构模块，不得被描述成当前已经物化的模块；不得引用已删除的旧模块。

#### Scenario: 模块列表与当前 R3 实现一致

> 场景标题里的 "R3" 是建立时的轮次标签（开源规格的 `#### Scenario:` 标题在 MODIFIED 语义下不可改名）——本场景的判据随模块清单一起现况化。

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `Modules` 数组
- **THEN** 恰好包含上列八个 Runtime 模块，且按依赖序排列（`TcsState` 位于 `TcsEffect` 之后、`TcsTargeting` 之前），LoadingPhase 均为 `Default`

#### Scenario: 目标模块不被误报为当前实现

- **WHEN** 对照 `openspec/project.md` 的目标架构与 `Source/` 目录
- **THEN** `TcsSkill`、`TcsCue`、`TcsEditor` 被标为目标模块或后续轮次，不出现在当前物化模块清单中；`TcsState` 自 2026-10-04（R5 Task 1）起**已物化**，不再属于该清单

#### Scenario: 旧模块不复活

- **WHEN** 检查插件描述文件全文
- **THEN** 不存在 `TireflyCombatSystem`、`TireflyCombatSystemEditor` 等已删除旧模块条目
