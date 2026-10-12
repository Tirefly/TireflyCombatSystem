## ADDED Requirements

### Requirement: 消耗配置的数据形状（策略与时机）

`TcsSkill` MUST 提供消耗配置的**纯数据**形状（零行为）：

- `UENUM(BlueprintType) ETcsCostPolicy`（**三档**）：`ECP_None`（**默认**：恒可付、零日志）/
  `ECP_ResourceAttr`（走 M2 属性账本）/ `ECP_CustomFragment`（**本轮具名未实现**）；
- `UENUM(BlueprintType) ETcsCostTiming`（**四档**）：`ECT_OnCastStarted`（**默认**）/
  `ECT_OnCastCompleted` / `ECT_OnEvent` / `ECT_Manual`；
- `USTRUCT(BlueprintType) FTcsCostConfig{ ETcsCostPolicy Policy; ETcsCostTiming Timing;
  FGameplayTag ResourceAttr; double Amount; FGameplayTag TriggerEventTag; }`。

**`ETcsCostTiming` MUST NOT 含 `OnCastInterrupted` 档**——两条硬判据（非偏好）：

1. `D5-4` 已把 `bRefundCostOnInterrupt` 砍除，理由原文是"**返还是政策不是引擎语义**"
   ⇒ 框架层面"打断 ⇒ 退钱"这条路已被明确关闭；
2. 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**（那就是
   `ECT_OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。

**`OnEvent` 档采纳**（`D5-10` 原本就有，批次表 `c` 漏列）：**它只是"等哪个事件"这个数据**
（`FGameplayTag`，与 `FTcsEffectTriggerDef::EventTag` 同形，住 `TriggerEventTag`）；
"事件到了**做什么**"是行为，走既有的 `UTcsEventHandler` 派生 + `TcsEventBusSubsystem::Subscribe`
⇒ **不需要为时机再造一个策略片段载体**。

**`ResourceAttr` / `Amount` 的失效面 MUST fail-closed**（判据 = 本仓既有惯例，
`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`
三处措辞几乎逐字相同："**配置写错**"与"**正确声明不消耗**"是两件事，混成一类会让前者
**表现成一切正常**、日志还清白）：

- `ECP_None` ⇒ 恒可付、**零日志**（正确声明"不消耗"）；
- `ECP_ResourceAttr` + `ResourceAttr` **无效** ⇒ **恒不可付** + `Warning`；
- `ECP_ResourceAttr` + `Amount < 0` ⇒ **恒不可付** + `Warning`。判据：扣减式是 `Base - Amount`，
  负 `Amount` 会让技能**倒给**资源（`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态；
- `ECP_ResourceAttr` + `Amount == 0` ⇒ 恒可付、零日志（"消耗 0 点"就是不消耗，非错误）；
- `ECP_CustomFragment` ⇒ 按 `ECP_None` 处理 + **具名 `Warning`**（含档名），**MUST NOT 静默**。
  阻塞前置：该档要求宿主脚本能写属性，而 `FTcsAttrModInstance` 是**纯 C++ struct、非反射**
  （`SCRIPT-9` / `R5.5-h` 均未开工）⇒ 今天物理不可达；

**`FTcsCostConfig` 的"无消耗"由 `Policy` 枚举表达，而冷却侧由空数组表达**——
这是一处**刻意的不对称**（冷却天然是"零到多条轨道"，消耗是"零或一种资源扣减"，后者没有可枚举的
多个元素）⇒ **MUST NOT** 按对称性把某一侧改成另一侧的形状。

**本轮只支持单资源**（`SPEC-04-skill` §7 明文："v1 单资源，多资源 = 未来新原语"）；
**MUST NOT** 声明 `CostPct` 或任何折扣系数槽——Cost 侧是否需要折扣是**玩法结论**，
`D5-15` 给冷却定 CDR 是当时就拍了的、Cost 侧**从未拍过**，今天无真实内容需求（台账 `COST-1`）。

#### Scenario: 四档时机且无打断档

- **WHEN** 检查 `ETcsCostTiming` 的档位集
- **THEN** 恰为 `OnCastStarted` / `OnCastCompleted` / `OnEvent` / `Manual` 四档；**不含** `OnCastInterrupted`（判据见上：`D5-4` 已砍返还，该档两种读法一为重复一为荒谬）

#### Scenario: 默认档为不消耗且恒可付

- **WHEN** 新建一个 `FTcsCostConfig` 且不显式设置
- **THEN** `Policy == ECP_None`、`Amount == 0`、`ResourceAttr` 无效、`TriggerEventTag` 无效；该技能恒可付且**零日志**

#### Scenario: 自定义片段档具名降级

- **WHEN** 某技能配 `Policy = ECP_CustomFragment`
- **THEN** 行为等同 `ECP_None`（恒可付、零扣减），并留**具名** `Warning`（含档名）——**MUST NOT** 静默当作 `ECP_None`

#### Scenario: 配置错误一律 fail-closed

- **WHEN** 某技能配 `Policy = ECP_ResourceAttr` 且 `ResourceAttr` 为无效 tag、或 `Amount < 0`
- **THEN** 判定为**不可付**（返回 `SAR_CannotAfford` 一类具名原因）并留 `Warning`——**MUST NOT** 静默按"恒可付"放过
