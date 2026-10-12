## MODIFIED Requirements

### Requirement: 消耗配置的数据形状（策略与三档时机）

`TcsSkill` MUST 提供消耗配置的**纯数据**形状（零行为）：

- `UENUM(BlueprintType) ETcsCostPolicy`（**三档**）：`ECP_None`（**默认**：恒可付、零日志）/
  `ECP_ResourceAttr`（走 M2 属性账本）/ `ECP_CustomFragment`（**本轮具名未实现**）；
- `UENUM(BlueprintType) ETcsCostTiming`（**三档**）：`ECT_OnCastStarted`（**默认**）/
  `ECT_OnCastCompleted` / `ECT_Manual`；
- `USTRUCT(BlueprintType) FTcsCostConfig{ ETcsCostPolicy Policy; ETcsCostTiming Timing;
  FGameplayTag ResourceAttr; FTcsParamValue Amount; }`。

**`Amount` MUST 是 `FTcsParamValue`（参数源载体）**（2026-10-12 用户裁定，台账 `COST-1` 闭合）：

- **判据 = 一个 `double` 同时承担了三件事，而三者都需要"可配置"**：
  ① **按等级配数值**（"1 级 10 蓝 / 2 级 15 蓝"）；② **按来源配数值**（`HostDelegate` / `ParamRef`）；
  ③ **减免**——由契约键 `TcsStateParam.CostAmount` 上的修正器折叠给出。
  ⇒ 裸 `double` 三者都做不到；
- **`FTcsParamValue` 的默认构造装 `Literal(0)`** ⇒ **与换型前的 `double Amount = 0.0` 语义逐字相同**
  （既有资产零迁移；全仓 `.uasset` 扫该字段零命中）；
- **求值 MUST 在门禁⑤ 与每个提交点各做一次**，**MUST NOT** 求一次后跨"门禁 → 提交"复用
  （那会把 TOCTOU 窗口重新打开）；
- **求值结果 MUST 钳下界到 0**，但**调用方 MUST 能同时拿到"未钳的基数"**——
  判据见下方"负基数与减免过度是两件事"。

**`ETcsCostTiming` MUST NOT 含 `OnCastInterrupted` 档**——两条硬判据（非偏好）：

1. `D5-4` 已把 `bRefundCostOnInterrupt` 砍除，理由原文是"**返还是政策不是引擎语义**"
   ⇒ 框架层面"打断 ⇒ 退钱"这条路已被明确关闭；
2. 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**（那就是
   `ECT_OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。

**`ETcsCostTiming` MUST NOT 含 `OnEvent` 档**（2026-10-12 用户裁定移除，**MUST NOT 补回**）：

- **判据**：框架只能认识"**施法这个事务自身的边界**"（开始 / 完成）——这两个时点在**每个项目里
  含义都相同**；而"**某个特定事件**"（挨打时付费 / 连击到第 3 段付费 / 血量低于 30% 付费……）
  是**项目词汇** ⇒ 框架内置它等于**让框架去猜项目的玩法**，且它提供的只是一个"等某个 tag"的
  空槽位（"这个 tag 是什么、什么时候发"全在宿主手里）⇒ **框架提供了一个自己解释不了、
  也无法校验的档位**。这与本仓主判据「**凡是需要宿主词汇的内置策略，框架一律不提供**」直接冲突；
- **宿主实现成本极低，且不需要任何新框架面**：监听自己关心的事件 → 调 `PayCost`（公开口）
  ——那正是 `ECT_Manual` 档 + `PayCost` 的既有组合；
- **伴随面 MUST 一并删除**（否则留下悬空契约）：`FTcsCostConfig::TriggerEventTag` 字段、
  `UTcsCostEventRelay` 整个类、门面的 `GetCostEventRelay()` 与 `CostEventRelay` 成员、
  以及三处撤销路径（`Revoke` / `RevokeBySource` / `UnregisterUnit`）的中继退订。
  **判据 = 「零消费者不预建」的反向使用**：消费者归零 ⇒ MUST 删掉；
- **`CostApplied` 事件本身 MUST 保留**（它与档位无关：任何提交路径都发它）；
- **`ETcsCooldownTiming` 不受本裁定影响**——冷却侧本来就没有 `OnEvent` 档
  （其四档是 `OnCastStarted` / `OnCastCompleted` / `OnCastInterrupted` / `Manual`）。

**`ResourceAttr` / `Amount` 的失效面 MUST fail-closed**（判据 = 本仓既有惯例，
`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`
三处措辞几乎逐字相同："**配置写错**"与"**正确声明不消耗**"是两件事，混成一类会让前者
**表现成一切正常**、日志还清白）：

- `ECP_None` ⇒ 恒可付、**零日志**（正确声明"不消耗"）；
- `ECP_ResourceAttr` + `ResourceAttr` **无效** ⇒ **恒不可付** + `Warning`；
- `ECP_ResourceAttr` + **`Amount` 的求值结果为负** ⇒ **恒不可付** + `Warning`
  （负值会让技能**倒给**资源）；
- `ECP_ResourceAttr` + `Amount` 的求值结果为零 ⇒ **恒可付**、零日志（"消耗 0 点"就是不消耗）。

**★ 负基数与减免过度 MUST 是两件事（装置拒绝面检查 `d` 实测抓到的设计缺陷）**：

| 情形 | 判据依据 | 语义 | 处置 |
|---|---|---|---|
| **负基数**（作者把 `Amount` 填成 -50） | **未钳的基数** | **配置错误**、"倒给资源"的**漏洞形态** | **拦住** + `Warning` |
| **折叠后转负**（基数 30、减免 -100） | **钳后的生效量** | **减免过度** = 合法的"免费" | **钳 0**、**不报错** |

**判据（MUST）**：`ResolveEffectiveCost` MUST 输出**未钳的基数**（出参），
使上述两条判据各得其数。**MUST NOT** 只给一个钳过的结果
——那会让负基数被静默抹成 0、其守卫永远不触发（**该缺陷已在实施期被装置当场报出**）。

#### Scenario: 默认档为不消耗且恒可付

- **WHEN** 某技能的 `FTcsCostConfig` 取默认值
- **THEN** `Policy == ECP_None`、`Amount` 的内层源为 `Literal` 且值为 `0`、`ResourceAttr` 无效；
  该技能恒可付且**零日志**

#### Scenario: 三档时机且无打断档、无事件档

- **WHEN** 枚举 `ETcsCostTiming` 的全部取值被检查
- **THEN** 恰为 `OnCastStarted` / `OnCastCompleted` / `Manual` **三档**；
  **不含** `OnCastInterrupted`（判据：`D5-4` 已砍返还）也**不含** `OnEvent`
  （判据：需要宿主词汇的内置策略，框架一律不提供）

#### Scenario: 配置错误一律 fail-closed

- **WHEN** 一个"想配资源消耗但 `ResourceAttr` 未填 / `Amount` 的求值结果为负"的技能被判定
- **THEN** 判定为**不可付**并留**具名 `Warning`**——**MUST NOT** 静默按"不消耗"放过

#### Scenario: 负基数被拦而减免过度被钳（两件事可区分）

- **WHEN** 分别构造 ① `Amount` 字面量 = `-50`、② `Amount` 字面量 = `30` 且
  `CostAmount` 键上挂 `FlatAdd = -100`
- **THEN** ① 判**不可付** + `Warning`（负基数是漏洞形态）；
  ② 生效量**钳到 0**、判**恒可付**、**零红字**（减免过度是合法的"免费"）；
  且两者**不得因同一个收口而互相掩盖**（判据依赖"未钳的基数"出参）

#### Scenario: 自定义片段档具名降级

- **WHEN** 某技能配 `Policy = ECP_CustomFragment`
- **THEN** 判定为可付、**零扣减**，并留**具名** `Warning`（含档名）——**MUST NOT** 静默当作 `ECP_None`

## ADDED Requirements

### Requirement: `CostAmount` 消耗修正契约键与生效量求值

`TcsSkill` MUST 提供消耗的**生效量求值**，使"基数可配 + 减免可挂"两件事合成一个数：

- **契约键** `TcsStateParam.CostAmount`（原生 tag + 模块导出，与 `TcsStateParam.CooldownPct`
  同族）：**本键上的修正经五带折叠作用于 `Amount` 的求值结果**
  （**初值 = 字段求值结果，不是 0**）；
- **它与 `CooldownPct` 的刻意不对称（MUST 记，防按对称性误改）**：
  `CooldownPct` 是**纯系数**（`Duration × (1 − CDR)`、只按 `[0,1]` 钳制、**不做折叠**）；
  而本键走**带式折叠**——**因为"减固定值"用纯系数表达不了**。
  判据 = "**减免能不能用一条系数说清**"；
- **两种减免形态各占一带**（复用 `FoldTcsAttributeBands` 的既有算术，**零新机制**）；
- **求值 MUST 是唯一实现**（`FTcsCostOps::ResolveEffectiveCost`）：门禁⑤ 与**每个提交点**
  都调它，**MUST NOT 各算一份**（"门禁判的数"与"真扣的数"一致 = TOCTOU 防护的实质）；
- **`CostApplied` 载荷的 `Amount` MUST 取实际扣减量**（求值 ⊕ 减免之后），
  **MUST NOT 取配置基数**——否则"发出扣了 30 而实际扣了 21"会直接误导订阅者的 UI。

#### Scenario: 百分比减免生效

- **WHEN** `Amount` 基数求值为 `30`，且在 `CostAmount` 键上挂 `TAO_PercentAdd = -0.2`
- **THEN** 生效量为 `24.000`（`30 × (1 − 0.2)`）

#### Scenario: 固定值减免在百分比之后叠加

- **WHEN** 在上述之上再挂 `TAO_FlatAdd = -5`
- **THEN** 生效量为 `19.000`（`24 − 5`）——**MUST NOT 是 `20.000`**
  （那说明 `FlatAdd` 被算在缩放**之前**，与本仓属性/伤害的共用算术次序不符）

#### Scenario: 减免到免费是合法结果

- **WHEN** 减免把生效量折到 `0`（或折叠后转负被钳到 0）
- **THEN** 该技能**恒可付**、零扣减、零日志、**不发** `CostApplied`
  ——与"配了 0 点消耗"走同一条路（**MUST NOT** 报成配置错误）

#### Scenario: 减免按来源级联摘除

- **WHEN** 三条减免修正器同属一个来源，随后该来源被 `RemoveParamModifiersBySource` 摘除
- **THEN** 生效量**回到基数**（一次摘净）；**MUST NOT** 留下残余减免
  （成对性：与 `ApplyParamModifiers` 严格配对）

#### Scenario: `CostApplied` 载荷是实际扣减量而非基数

- **WHEN** 一次基数 `30`、减免后 `19` 的消耗提交成功
- **THEN** `CostApplied` 载荷的 `Amount == 19`（**MUST NOT 是 30**）
