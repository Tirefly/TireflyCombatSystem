# 借鉴 01：WHO/WHEN/WHAT/HOW 四维设计语言

## 概述

AbilityKit 在 `Docs/游戏技能系统本质抽象.md` 中提出用"四个问题"作为技能系统的元语言：**WHO（对谁）、WHEN（什么时候）、WHAT（做什么）、HOW（怎么做）**。这不是一个框架或运行时机制，而是一种"设计任意战斗能力时先回答四个问题"的思考纪律与文档语言。

本模块价值极高且风险极低：TCS 可直接吸收为设计习惯与文档语言，零代码改动。

## AbilityKit 设计思想与架构实践

### 核心四问

AbilityKit 把所有游戏技能系统抽象为四个维度（`Docs/游戏技能系统本质抽象.md:69-98`）：

```
WHO ────→ 对谁？目标是谁？  施法者/目标/区域内友军/敌人/自己
WHEN ───→ 什么时候？触发条件？  立即/延迟/周期/条件/事件
WHAT ───→ 做什么？产生什么效果？  伤害/治疗/Buff/位移/状态
HOW ────→ 怎么做？执行方式？  立即/分段/引导/条件触发/叠加
```

### 元语素分解

每个维度进一步分解为"元语素"，形成可组合的设计词汇（`游戏技能系统本质抽象.md:102-266`）：

- **WHO（目标选择）**：Selector（self/target/caster/enemies/allies/in_range/has_tag/filter）+ Range（point/cone/line/circle/rectangle）。
- **WHEN（触发时机）**：主动触发 / 被动触发（Condition/Event/Periodic）/ 延迟触发 after(X)。
- **WHAT（效果/行为）**：即时（damage/heal/shield）/ 持续（apply_buff/remove_buff/DoT/HoT/drain）/ 移动 / 状态 / 特殊；每个效果带 Magnitude/Tags/Duration 元属性。
- **HOW（执行方式）**：Immediate / Delayed / Channel / Phased / Periodic / Conditional。

### 设计检查清单

AbilityKit 给出一个技能设计检查清单（`游戏技能系统本质抽象.md:475-499`）：

```
□ WHO - 目标是什么？单一/多目标？需要过滤？有范围？
□ WHEN - 什么时候触发？主动/被动？触发条件？触发冷却？
□ WHAT - 产生什么效果？即时/持续？可叠加？有副作用？
□ HOW - 怎么执行？立即/延迟？可打断？多阶段？
```

### 常见技能模板对照

`游戏技能系统本质抽象.md:506-516` 用四维快速描述常见模板：

| 技能类型 | WHO | WHEN | WHAT | HOW |
|---------|-----|------|------|-----|
| 普通攻击 | target | 施放 | damage(base_attack) | immediate |
| 治疗术 | ally | 施放 | heal(X) | immediate |
| 持续伤害 | target | 施放 | DoT(X, Ys) | periodic(every=1s, Ys) |
| 眩晕 | target | 施放 | stun(Xs) | immediate |
| 引导AOE | enemies_in_range | 施放 | damage(X) | channel(duration=3s, tick=1s) |
| 被动光环 | allies | always | buff(X) | continuous |
| 反击 | self | receives_damage | damage_back(X) | immediate, passive |
| 复活 | self | death | resurrect | delayed(after=3s) |

### 抽象与实现映射

AbilityKit 明确把这四问映射到技术实现（`游戏技能系统本质抽象.md:454-471`）：

| 本质抽象 | 技术实现 |
|---------|---------|
| WHO - 目标选择 | TargetSystem / 目标系统 |
| WHEN - 触发时机 | TriggerSystem / 触发系统 |
| WHAT - 效果/行为 | EffectSystem / 效果系统 |
| HOW - 执行方式 | ExecutionSystem / 执行系统 |

## 与 TCS 现状的对比

### TCS 当前的设计语言

TCS 的设计文档与 spec 更偏面向"状态生命周期与 StateTree 执行细节"。例如 `state-management` spec 描述的是"八步移除时序"、"StateSlot/StateInstance"、"生命周期状态机"，这属于 HOW 的执行细节，但对 WHO/WHEN/WHAT 缺少统一显式表达。

证据：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem\openspec\specs\state-management\spec.md:39-50` 聚焦移除顺序；`project.md:147-156` 聚焦 StateTree 双层与跨 Actor 效果施加约束。

### 缺口

- **WHO**：TCS 由 `StateComponent` 承载，但没有显式"目标选择器"抽象。技能目标选择散落在 SkillInstance 的业务逻辑里，缺乏成体系的 Selector/Range 语素。
- **WHEN**：TCS 状态事件是 `NotifyStateStageChanged/Removed/ApplySuccess/ApplyFailed`，但缺少"触发时机"的显式分类（Active/Passive/Aura/Event/Periodic）。
- **WHAT**：TCS 用 Modifier / State / Buff 表达效果，但缺少"效果类型与元语素"的统一描述。
- **HOW**：TCS 用 StateTree 表达执行方式，是 HOW 维度的强实现，但 Phase/Channel/Phased/Periodic 等执行模式没有跨系统统一词汇。

### 相似点

TCS 的"数据-行为分离 + 策略模式"（`project.md:132-146`）与 AbilityKit 的"配置即类型"在精神上有共通：都强调配置驱动、行为可替换。差异是 AbilityKit 把维度抽象提到设计语言层面，TCS 把抽象落在类型化分层与 CDO 策略上。

## 借鉴建议

### 1. 把四维设计语言纳入 TCS 设计文档与 spec

- 在 `project.md` 的"架构概念"一节补充"四维设计语言"说明，作为 TCS 描述任意能力的统一前置框架。
- 在新增 capability 或 change proposal 的 `design.md` 中显式回答 WHO/WHEN/WHAT/HOW 四问，建立规范。
- 对现有 spec 做一次"四维对齐审计"：哪些 capability 缺 WHO 抽象、哪些缺 WHEN 分类，作为后续优化的 backlog。

### 2. 在 TCS 内建立"元语素词汇表"

引用 AbilityKit 的元语素分解，按 TCS 实情映射：

- **WHO**：TargetSelector（self/caster/target/owner/enemies/allies/in_range/has_tag）+ RangeShape（cone/circle/line/rectangle）。TCS 近期可先在 Skill 子域建模，再推广到 Aura/区域 State。
- **WHEN**：TriggerMode（Active/Passive/Aura/Event/Periodic）+ TriggerCondition（OnHit/OnDamaged/OnKill/OnBuffApply/HealthThreshold）。TCS 的事件通知已有零件，可整合成显式枚举。
- **WHAT**：EffectType（Damage/Heal/Shield/ApplyState/RemoveState/Movement/Status/Summon）+ Magnitude/Tags/Duration 元属性。TCS 的 Buff/State 已有 Tag/Duration/Period，可统一到 `UTcsStateDefinition` 基类。
- **HOW**：ExecutionMode（Immediate/Delayed/Channel/Phased/Periodic/Conditional）。TCS 用 StateTree 表达执行，可在 Definition 层暴露 ExecutionMode 字段供编辑器 authoring。

### 3. 用四维检查清单约束新设计

任何新 Buff/Skill/State 引入评审时，必须回答四问：

```
□ WHO - 目标是谁？单一/多目标？过滤？范围？
□ WHEN - 什么时候触发？主动/被动/光环/事件？触发冷却？
□ WHAT - 产生什么效果？即时/持续？可叠加？副作用？
□ HOW - 怎么执行？立即/延迟/引导/分段？可打断？多阶段？
```

这是一项组织纪律，不引入代码改动，但能显著降低设计模糊性。

## 风险与前置条件

- **风险：极低**。仅是设计语言与文档习惯，不改动代码与 spec 契约。
- **前置条件**：无。
- **落地形式**：
  1. 更新 `project.md` 的架构概念一节（非破坏性补充）。
  2. 在团队内部把"四问检查清单"纳入评审标准。
  3. 后续 capability spec 中补充四维说明（渐进推进，不强制一次性改完）。

## 不建议的做法

- **不要**把 AbilityKit 的元语素词汇直接当 UE 反射类型塞进 C++。TCS 已有自己的类型化分层（`UTcsStateDefinition` → `UTcsBuffDefinition` / `UTcsSkillDefinition`），元语素应是设计语言而不是基类继承树。
- **不要**把"四维"硬绑到编辑器资产字段。它首先是设计语言，资产字段取舍由 capability spec 决定。

## 参考

- AbilityKit: `Docs/游戏技能系统本质抽象.md`
- AbilityKit: `Docs/游戏效果系统统一抽象.md`（"一切皆为效果"的统一思维，见借鉴 02）
- TCS: `Plugins/TireflyCombatSystem/openspec/project.md`
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/state-management/spec.md`