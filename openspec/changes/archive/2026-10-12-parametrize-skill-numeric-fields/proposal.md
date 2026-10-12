# 技能数值字段参数化：Cost 的 `Amount` 与 Cooldown 的 `Duration` 换载体

## Why

**用户裁定（2026-10-12）**：`FTcsCostConfig::Amount` 与 `FTcsCooldownTrack::Duration` 这两处
"技能自己的数值"应当能**用 `TcsParam` 配置**——具体要支持三件事：

1. **按等级配数值**（"1 级 10 蓝 / 2 级 15 蓝"、"1 级 8 秒 / 2 级 6 秒"）；
2. **百分比减免**（"某状态下技能半价"、"装备减 CD 20%"）；
3. **固定值减免**（"某 buff 让这个技能少花 10 点"）。

**现状为什么做不到 —— 一个字段同时承担了三件事，而三者都需要"可配置"**：

- 两个字段都是**裸 `double`** ⇒ 数值只能**在资产上写死**，等级/来源的差异无从表达；
- Cost 侧**连减免的通道都没有**：`FTcsCostOps` 全文**零 `EvaluateParamKey` 调用**
  （`CanAffordCost` 直接比 `Config.Amount`、`CommitCost` 直接 `Base - Config.Amount`）；
- 冷却侧虽有系数通道（`CooldownPct`），但**它只能缩放** ⇒ 表达不了"减固定值"。

**⇒ 这三条正是台账 `COST-1` / `COOLDOWN-1` / `STAT-11` 的同一条技术根因**
——"**技能侧数值字段缺参数源载体**"。三条的台账行都写着 **MUST 同批裁**（任一落地时
另一条的形状随之定，MUST NOT 分两次各造一套参数化通道），本次**同批处理**。

## What Changes

- **MODIFIED**（`skill-cooldown`）：`FTcsCooldownTrack::Duration` 由 `double` → **`FTcsParamValue`**；
- **MODIFIED**（`skill-cost`）：`FTcsCostConfig::Amount` 由 `double` → **`FTcsParamValue`**；
- **ADDED**（`skill-cost`）：新契约键 **`TcsStateParam.CostAmount`**（消耗修正键）与
  `FTcsCostOps::ResolveEffectiveCost`（**基数求值 ⊕ 减免折叠**的唯一实现）；
- **ADDED**（`skill-cooldown`）：时长载体的**求值时点**与上下文口径（起冷那一刻、全场一次）。

**形状选择（用户 2026-10-12 从三案中点定 B 案）**：`Amount` **换载体**，
而**不是**"照 `CooldownPct` 加一个 `CostPct` 系数键"——判据 = **只有换载体能表达固定值减免**
（纯系数减不到"−10"），而用户明确要求"有可能是百分比、**有可能是固定值**"。

**保留不动**：`ECP_ResourceAttr` 单资源（多资源归**新台账条目**"成本组"，见 Impact）；
`SCRIPT-9`（属性写入面的脚本可达性）**归属不变**；`ECP_CustomFragment` 具名未实现维持现状。

## Impact

- **Affected specs**：`skill-cost`（MODIFIED 1 + ADDED 1）、`skill-cooldown`（MODIFIED 1 + ADDED 1）
- **Affected code**：
  - `TcsSkill/Public/Def/TcsCostTypes.h` / `TcsCooldownTypes.h`（两个字段换型 + include）
  - `TcsSkill/Public/Param/TcsSkillParamKeys.h` / `Private/Param/TcsSkillParamKeys.cpp`（新契约键）
  - `TcsSkill/Public/Skill/TcsParamChain.h` / `Private/Skill/TcsParamChain_Evaluate.cpp`
    （`FoldParamKeyWithBase`——"以显式初值折叠"，与 `EvaluateParamKey` 并存）
  - `TcsSkill/Public/Skill/TcsCostOps.h` / `Private/Skill/TcsCostOps.cpp`
    （`ResolveEffectiveCost` + 三个入口接生效值）
  - `TcsSkill/Private/Skill/TcsCooldownOps.cpp`（时长载体求值）
  - `TcsSkill/Private/TcsSkillSubsystem.cpp`（`CanAfford` 公开口传条目）
- **Affected host rig**：`TcsDevSliceRig_CooldownCost.{h,cpp}`（新增 `J0`~`J5` 六条判据）
- **台账**：`COST-1` / `COOLDOWN-1` **本轮闭合**；**新增一条**"成本组"（多资源，触发条件型）
- **无破坏性资产影响**：两个字段的**默认值语义不变**（`FTcsParamValue` 默认装 `Literal(0)`），
  且全仓 `.uasset` 扫这两个字段**零命中** ⇒ **无存量资产需迁移**

## ★ 实施期抓到的一处**真实设计缺陷**（装置拒绝面检查 `d`，MUST 记）

初版把"折叠后钳 `[0, +∞)`"当成**唯一**收口 ⇒ 后果：**"负基数"被钳成 0**
⇒ `CanAffordCost` 的"负值 ⇒ 恒不可付"守卫**永远不触发**
⇒ 检查 `d`（`Amount = -50` 应拒）**从 `false` 翻成 `true`，当场报 FAIL**。

**两件事语义完全不同，MUST 分开**：

| 情形 | 语义 | 处置 |
|---|---|---|
| **负基数**（作者把 `Amount` 填成 -50） | **配置错误**、"倒给资源"的**漏洞形态** | **拦住**（`CanAffordCost` / `CommitCost`） |
| **折叠后转负**（基数 30、减免 -100） | **减免过度** = 合法的"免费" | **钳 0**、**不报错** |

**修法** = `ResolveEffectiveCost` 增 `OutBaseAmount` 出参（输出**未钳的基数**），
让两个判据各有其数。**这条只有行为级取证才抓得到**（编译与代码阅读都过）
——正是本仓"装置要能证伪"纪律的又一次兑现。
