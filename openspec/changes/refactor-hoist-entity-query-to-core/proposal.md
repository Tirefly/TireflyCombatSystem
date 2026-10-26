# Change: 把 `ITcsEntityQuery` 契约提升到 `TcsCore`，并把注入槽位从两处减到一处

## Why

`ARCH-11` 的 `WAIT-12`（台账条目）登记了一个**静默错配**缺陷：实体查询的注入点分散在两处门面且互不感知——

| 门面 | 谁在读 | 角色 |
|---|---|---|
| `UTcsEffectSubsystem::SetEntityQuery` | 组件注册（`TcsCombatEntityComponent.cpp:33`）、距离打分（`TcsStepSelectTargets.cpp:86`） | **真持有者、全仓唯一"句柄 ↔ Actor"映射点** |
| `UTcsSkillSubsystem::SetEntityQuery` | 只有技能门禁第一道（`TcsSkillSubsystem_EntityReady.cpp:24`） | **影子槽位**，从不外传、与 Effect 侧无同步 |

宿主只注入其一、或分别注入两个不同实例时，技能侧门禁将**恒为假**，症状是"实体明明好好的，却报实体未就绪"——本仓反复拦的那一类静默失效。

同时，契约 `ITcsEntityQuery` 声明在 `TcsEffect` 下，但它的**四个消费者**（`TcsEffect` 持槽位 / `TcsSkill` 用 `IsEntityReady` / `TcsTargeting` 用 `GetLocation` / `TcsIntegration` 用实现）的**最近公共祖先是 `TcsCore`**，不是 `TcsEffect`。契约住在域模块里，使 `TcsEffect` 从"恰好持有槽位的门面"错位成"契约的主人"。

## What Changes

- **契约搬家**：`ITcsEntityQuery` / `UTcsEntityQuery` 从 `TcsEffect/Public/Host/TcsEntityQuery.h` 移到 `TcsCore/Public/Host/TcsEntityQuery.h`。`TcsCore` 已持有 `Handle/TcsCombatEntityHandle.h`（契约的唯一类型依赖），故**四个消费者的 `Build.cs` 零改动、零新增依赖边**。
- **删除技能侧槽位**：`UTcsSkillSubsystem` 不再持有 `EntityQuery` 成员、不再提供 `SetEntityQuery` / `GetEntityQuery`；门禁第一道改为经 `UTcsEffectSubsystem::GetEntityQuery()` 取。
- **降级语义改判**：门禁降级的条件从「**技能侧未注入**」变成「**宿主整个未注入**」。语义上更正确——"技能门禁降级"的理由不该是"技能侧忘了接"，而是"宿主根本没提供实体查询"。
- **规格同步**：`entity-query-contract` 的主语与路径订正 + 新增「注入面唯一」需求；`skill-registry` 的门禁需求整条重写；`integration-entity` 的注入方式加强为"MUST 只注入这一处"。
- **装置同步**：`TcsDev` 的注入点全部收敛到 `UTcsEffectSubsystem`；G1/G2/G3/C12 四条证据**可证伪性保持不变**（发散探针仍能让 `IsAlive` 与 `IsEntityReady` 给出相反答案）。

## Impact

- Affected specs: `entity-query-contract`（主语 + 路径 + 新增需求）、`skill-registry`（门禁需求重写）、`integration-entity`（注入方式）
- Affected code:
  - `Source/TcsCore/Public/Host/TcsEntityQuery.h`（新增，内容来自 TcsEffect）
  - `Source/TcsEffect/Public/Host/TcsEntityQuery.h`（删除）
  - `Source/TcsSkill/Public/TcsSkillSubsystem.h`、`Source/TcsSkill/Private/TcsSkillSubsystem_EntityReady.cpp`
  - `Source/TcsDev/Private/Dev/`（1 个探针逻辑改动 + 7 处重复注入点删除）
- **不涉及**：`ARCH-11` U-1 本体（实体状态机存不存在）是独立一轮，本变更只动契约归属与槽位数量。

## 附：本变更顺带闭合的账

| 账 | 状态 |
|---|---|
| `WAIT-12`（两处注入点静默错配） | 两个症状**结构上不可能**（宿主只接一处；不存在两个不同实例） |
| `ARCH-11` `WAIT-12` 候选解法 ①「只文档化装配要求」 | 不采用（靠人记住）；采用候选 ②的强化版——**删槽位**而非"回落" |
