# Change: 冷却与消耗的类型层（策略族 + 契约键 + Def 字段回补）

> 对应计划 `PLN-R6.5-1`（`Documents/combat-system-design/plans/plan-r6.5-cooldown-and-cost.md`）**Task 0**。
> **本变更零行为变更**——只落类型、字段与契约键，**无任何调用点**（门禁 / 到期堆 / 事件全部归 Task 1~5）。

## Why

R6 收束后，"技能为什么放不出来"的第一层理由（未学 / 已在飞 / 定义无效 / 实体不可操作）已经可查，
但第二层**完全没有载体**：`SAR_OnCooldown` 与 `SAR_CannotAfford` 两道门禁在 `TcsCastOps.cpp:163` / `:237`
**只是带 `R6.5 占位` 注释的空分支**——判据恒过（空轨道恒可放 / 不消耗档恒可付），宿主拿不到"冷却中"这个答案。

实测（`PLN-R6.5-1` §0.3 十七行证据表）：`ECostPolicy` / `FCooldownTiming` / `FCooldownTrack` /
`AdjustCooldown` / `ResetCooldown` / `OnCostApplied` 在 `Source/` **全部 0 命中**；全仓亦**无任何
`enum class *Timing`**（两个新枚举名无占用）。而 R6 Task 2 的条目结构体注释**原文即写明**
"MUST NOT 声明冷却轨道状态字段（归 `R6.5-a`）"——理由是"其元素类型此刻全仓不存在，
而 `TArray<T>` 成员要求 `T` 至少可见 ⇒ 写进来就是未声明标识符"。

⇒ 本变更把**类型与消费者同时到位**的那一半先落地：没有这一层，Task 1~5 一行都编不过。

## What Changes

### 1. ADDED `skill-cooldown`（新能力，本变更只落**数据形状**）

- `ETcsCooldownTiming`（**四档**：`OnCastStarted` 默认 / `OnCastCompleted` / `OnCastInterrupted` / `Manual`）——
  **刻意无 `OnEvent`**（冷却的起因永远在本模块手里）与**无 `Custom` 档**；
- `FTcsCooldownTrack{ double Duration; FGameplayTag GroupTag; }`——**无效 `GroupTag` = 自身轨、有效 = 单位级组槽**，
  这就是**真公共冷却（GCD）的纯数据表达**；批次表 `:198` 的 `GroupTag?` 问号**就此关闭**（字段恒存在，
  用"tag 是否有效"承担两个语义，不再加一个"有没有组"的开关）；
- `FTcsCooldownPolicy{ TArray<FTcsCooldownTrack> Tracks; ETcsCooldownTiming Timing; }`——
  **空 `Tracks` = 永远可放**（`D5-15` "默认无冷却"的数据表达；Strategy 三档"溶解进轨道"）；
  `Timing` 是**策略级**（`D5-15` 明文）；
- **`Custom` 时机档整体取消**（2026-10-11 用户裁定）：`ITcsCooldownTimingFragment` **不建**、
  `FCooldownPolicy.CustomFragment` **不声明**。判据：自定义时机 = "**不自动执行，让宿主在自己的确定
  逻辑处手动调用**" ⇒ 那正是 `ECT_Manual` 档，**不需要任何 Payload / 策略片段载体**（时机是"何时做"，
  "何时"只有两种表达——能被事件名说出来，或只能由代码在具体位置说出来，**中间没有第三种形态**）；
- **`CooldownPct` 契约键**：原生 tag `TcsStateParam.CooldownPct`（`Public/Param/TcsSkillParamKeys.h`，
  带模块导出宏），**由 `TcsSkill` 声明**——声明三条判据见下节需求的"为什么住 `TcsSkill`"。

### 2. ADDED `skill-cost`（新能力，本变更只落**数据形状**）

- `ETcsCostPolicy`（**三档**：`ECP_None` 默认 / `ECP_ResourceAttr` / `ECP_CustomFragment`）；
- `ETcsCostTiming`（**四档**：`OnCastStarted` 默认 / `OnCastCompleted` / **`OnEvent`** / `Manual`）——
  **刻意无 `OnCastInterrupted`**，判据两条硬约束（见下节需求）；`OnEvent` 档**采纳**（批次表 `c` 漏列，
  `D5-10` 原本就有）；
- `FTcsCostConfig{ Policy; Timing; FGameplayTag ResourceAttr; double Amount; FGameplayTag TriggerEventTag; }`；
- **`ECP_CustomFragment` 档本轮具名未实现**（记 `Warning` + 回退 `ECP_None`）：其消费者是"宿主脚本写属性"，
  而属性写入面今天**物理不可达**（`FTcsAttrModInstance` 是纯 C++ struct、非反射；`SCRIPT-9` / `R5.5-h` 均未开工）。

### 3. MODIFIED `skill-def-asset`（`FTcsSkillDefData` 补两个字段）

R6 §11.2 曾把"冷却策略 / Cost 配置"记为**明确不落的字段**（判据 = 零消费者不预建）；
本轮的消费者（`TcsCooldownOps` / `TcsCostOps`）到位 ⇒ 字段随之落地。**MUST NOT 读成"R6 漏写了"**。

### 4. MODIFIED `skill-registry`（条目回补 `CooldownTracks`）

`FTcsLearnedSkillEntry` 字段由**七项**回补为**八项**，新增 `TArray<FTcsCooldownTrackState> CooldownTracks`
（**冷却的真值住账本条目**——`D5-15`："真值在 Entry，UI 轮询"）。同 `NumericParamModInstances` 的回补先例：
**不是"当初漏了"，是"当初不能写"**（元素类型不存在 + 零消费者）。

## 明确不做（非目标）

- **不做任何行为**：门禁③/⑤ 真实现、到期堆驱动、CDR 生效、事件发布 —— 分别归 `PLN-R6.5-1` Task 1 / 2 / 3 / 4。
- **不做 `Duration` 的 `Param` 形态**：本轮只落字面量 `double`；`D5-15` 原话的 `Duration(字面量|Param)`
  后半要求它变成参数源载体，而**参数源族在技能侧尚无消费者**（同族 `STAT-11` 的 `属性换算` 至今零实现者）
  ⇒ 已入台账（触发条件型），**与 Cost 侧的折扣系数缺口是同一个技术问题，MUST 同批裁**。
- **不做 Cost 折扣系数（`CostPct`）**：`CooldownPct` 有对称的系数槽而 `Amount` 侧没有——那是**一处真实的
  设计不对称**（已入台账 `COST-1`）。**本轮不做是边界判据不是"没时间"**：加它**不越界**（只引用框架自身
  契约"属性值是个数"），但"**要不要有消耗折扣**"是**玩法结论**——`D5-15` 给冷却定 CDR 是**当时就拍了的**，
  Cost 侧**从未拍过**，今天没有真实内容需求（现在加就是本计划反复拦的"零消费者预建"）。
- **不做多资源消耗**（`SPEC-04-skill` §7 非目标明文："v1 单资源"）。
- **不做网络复制姿态**（`SPEC-04-skill` §6）。

## Impact

- **Affected specs**：
  - `skill-cooldown`（**ADDED**，2 需求：策略数据形状 + `CooldownPct` 契约键）
  - `skill-cost`（**ADDED**，1 需求：消耗配置数据形状）
  - `skill-def-asset`（**MODIFIED** ×1：「技能 Def 数据形状」——补 `Cooldown` / `Cost` 两字段）
  - `skill-registry`（**MODIFIED** ×1：「已学技能账本」——字段七项 → 八项，补 `CooldownTracks`）
- **Affected code**（插件仓 TCS）：
  - 新：`Source/TcsSkill/Public/Def/TcsCooldownTypes.h`、`Public/Def/TcsCostTypes.h`、
    `Public/Skill/TcsCooldownState.h`、`Public/Param/TcsSkillParamKeys.h` + `Private/Param/TcsSkillParamKeys.cpp`
  - 改：`Source/TcsSkill/Public/Def/TcsSkillDefData.h`（+2 字段 +2 include）、
    `Public/Skill/TcsSkillRegistry.h`（+1 字段 +1 include）
  - **零新模块依赖边**（`TcsSkill.Build.cs` 不改——已含 `TcsCore` / `TcsAttribute` / `TcsEffect` / `TcsState`）
- **两配置编译**：Development Editor + Game Shipping 均 `Result: Succeeded`、**0 error / 0 warning**；
  新增字段经 `UHT` 产物 `NewProp_*_MetaData[]` 逐项核实为**真反射字段**（非"假字段"）
- **台账**：本变更**不消费也不新增**任何台账条目（`STAT-11` / `WAIT-14` / `STAT-10` 均归批次 `e` 或触发条件型，不受影响）
