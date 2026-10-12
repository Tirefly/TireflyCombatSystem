# 移除技能的 `ECT_OnEvent` 消耗档

## Why

**用户裁定（2026-10-12）**：TCS 插件**不应提供**"由某个特定事件触发的 Cost/Cooldown"这类档位——
这类"给予特定事件或特定情景"的时机**属于宿主项目自己的玩法逻辑**。

**判据（这是本次移除的核心理由，MUST 写清以防后人"补回"）**：

- 框架只能认识"**施法这个事务自身的边界**"——即 `OnCastStarted` / `OnCastCompleted`。
  这两个时点在**每一个项目里含义都相同**，故框架内置它们是对的。
- 而"**某个特定事件**"（挨打时付费 / 连击到第 3 段付费 / 血量低于 30% 付费 / 蓄力满时付费……）
  是**项目词汇**。框架内置它，等于**让框架去猜项目的玩法**——它只能提供一个"等某个 tag"的
  空槽位，而"这个 tag 是什么、什么时候发"全在宿主手里。
- ⇒ **框架提供了一个自己解释不了、也无法校验的档位**。这与本仓「**框架零默认 / 凡是需要宿主
  词汇的内置策略一律不提供**」的主判据直接冲突。

**宿主实现该玩法的成本极低，且不需要任何新框架面**：
监听自己关心的事件 → 调 `PayCost`（公开口）。这正是 `ECT_Manual` 档 + `PayCost` 的组合，
**已经存在**。

**同批删除的伴随面（MUST 一起删——否则留下悬空契约）**：

| # | 删除项 | 判据 |
|---|---|---|
| ① | `ETcsCostTiming::ECT_OnEvent` 枚举值 | 档位本体 |
| ② | `FTcsCostConfig::TriggerEventTag` 字段 | **只**为该档存在 |
| ③ | `UTcsCostEventRelay` 整个类（`.h` / `.cpp`） | **只**为该档服务 ⇒ 删档后消费者数 = **0** |
| ④ | 门面 `GetCostEventRelay()` + `CostEventRelay` 成员 + `Deinitialize` 的 `ResetAll` | 同 ③ |
| ⑤ | 三处撤销路径的中继退订（`Revoke` / `RevokeBySource` / `UnregisterUnit`） | 同 ③ |
| ⑥ | 宿主装置的两个检查 `H1` / `H2` + `GR65OnEventDef` / `GR65TriggerEvent` | 装置不该为已删档位保留判据 |
| ⑦ | 两条检查用 tag（`SkillDef.Check.R65OnEvent` / `TcsEvent.Check.R65Trigger`） | 临时验证词，随功能退役 |

**判据 = 本仓「零消费者不预建」的**反向**使用**：那条纪律管"没有消费者就不建"；
本条是"**消费者归零 ⇒ MUST 删掉**"——留着中继就是留一个**永远不被订阅**的 `UTcsEventHandler`
加上三条为它服务的撤销路径代码。

## What Changes

- **BREAKING**（对宿主面）：`ETcsCostTiming` 从 **4 档降为 3 档**——
  `ECT_OnCastStarted` / `ECT_OnCastCompleted` / `ECT_Manual`。
  **枚举数值变化**：`ECT_Manual` 由 `3` 变 `2`。
  **影响面实测 = 0**：全仓扫描 `.uasset` **零命中** `TriggerEventTag` / `ECT_OnEvent`
  ⇒ 没有任何已存资产会被这次数值变化影响。
- **REMOVED**：`FTcsCostConfig::TriggerEventTag`、`UTcsCostEventRelay` 及其全部调用点。
- **MODIFIED**：`skill-cost` 规格的"数据形状"与"运行态"两条需求——去掉 `OnEvent` 档，
  并把"特定事件触发消耗"的**归属**明文写成宿主侧。
- **保留不动**：`CostApplied` 事件本身（它与档位无关，任何提交路径都发它）；
  `ECT_Manual` 档 + `PayCost` 公开口（这正是宿主实现"特定事件触发"的既有组合）。
- **`ETcsCooldownTiming` 不受影响**——**冷却侧本来就没有 `OnEvent` 档**
  （其四档是 `OnCastStarted` / `OnCastCompleted` / `OnCastInterrupted` / `Manual`），
  故用户"Cooldown/Cost 都删"的意图在冷却侧**已经是现状**。

## Impact

- **Affected specs**：`skill-cost`（MODIFIED 2 条需求）
- **Affected code**：
  - `TcsSkill/Public/Def/TcsCostTypes.h`（枚举 + 字段）
  - `TcsSkill/Public/Skill/TcsCostEventRelay.h` + `Private/Skill/TcsCostEventRelay.cpp`（**删除**）
  - `TcsSkill/Public/TcsSkillSubsystem.h` + `Private/TcsSkillSubsystem.cpp`（读口 + 成员 + 拆解）
  - `TcsSkill/Private/Skill/TcsCastOps.cpp`（激活路径的登记块）
  - `TcsSkill/Private/Skill/TcsSkillOps.cpp`（三处撤销路径的退订）
  - `TcsSkill/Public/Skill/TcsCastOps.h`（注释面）
- **Affected host rig**：`Source/TcsDev/.../TcsDevSliceRig_CooldownCost.{h,cpp}`（删 H1/H2 + 两条 tag）
- **Affected config**：`Config/DefaultGameplayTags.ini`（退役两条检查用 tag）
- **无台账新增**：本次移除属 `PLN-R6.5-1` 范围内的实施收窄，不产生"指不出归属"的待办。

## 与 `D5-10` 的关系（MUST 记，防后人当作"违反设计"）

`D5-10` 原文**确实列了 `OnEvent` 档**。本次移除是**对该设计决策的修订**，不是实现偏差。
处置 = 在 `SPEC-04-skill` 的实施注记里**保留 `D5-10` 原文 + 追加修订说明**（就地注记，
MUST NOT 直接删原文）——同 `D5-4`（砍返还）/ `D5-15`（`Custom` 档取消）的既有处置口径。
