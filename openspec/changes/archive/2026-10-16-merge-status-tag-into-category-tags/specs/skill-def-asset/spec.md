## MODIFIED Requirements

### Requirement: 技能 Def 数据形状

`TcsSkill` MUST 提供 `FTcsSkillDefData : FTcsStateDefBase`（`USTRUCT(BlueprintType)`），承载**施法语义**。它 MUST 继承基类的全部字段并**白拿**之——`CategoryTags` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows` MUST NOT 在本类重复声明。**`CategoryTags` 在技能侧同样可用**（2026-10-15 随 `StatusTag` 并入基类而来；判据 = 技能 Def 与技能运行态都与状态侧共用同一基类/基底，"技能也有类别容器"与"技能也有 StateInstance"是同一件已成立的事实）。

本类 MUST 只补**施法语义**字段：

- **布尔开关表**：`TArray<FTcsBoolSwitchRow> BoolSwitches`——与基类的 `Params`（数值参数行）构成设计所称的"参数双表"；
- **施法时段表**：`TArray<FTcsPhaseSpan> Phases`——任意段数，"前摇/后摇"是项目命名惯例；**瞬发 MUST 表达为空表**（MUST NOT 用"单一零时长段"这一等价写法，否则查询契约的"瞬发"档无法与"一个零时长段"区分）；
- **查询契约三档**：`ECastQueryMode CastQueryMode`（`DefSwitches` 默认 / `PhaseTable` / `Custom`）+ `bool bInterruptibleDefault` / `bool bCanMoveDefault`（`DefSwitches` 档）+ `FInstancedStruct CastQueryFragment`（`Custom` 档）——三档**各有字段载体**，MUST NOT 出现"枚举声明了某一档但没有对应字段"的形状缺口；
- **主链**：`FGameplayTag CastChainId` + `EMainChainStart MainChainStart`（`OnCastStarted` 默认 / `OnPhaseEnter` / `OnCastCompleted` / `Custom`）+ `FGameplayTag MainChainStartPhaseTag`；
- **实例化**：`ECastInstancing Instancing`（默认 `InstancePerExecution`）；
- **顶替位**：`bool bRetriggerOnActive = false`——`InstancePerEntity` 在飞时的分流位：`false`（**默认**）⇒ 驳回；`true` ⇒ 顶替。默认值 `false` 是**判据不是偏好**（GAS 的对应位无初值即 `false`，且本仓"框架零默认"纪律下默认档 MUST 取**行为最保守**的一档——驳回 > 顶替）；
- **关系字段**（同 `FTcsBuffDef` 形状）：`Blocks` / `Requires` / `Priority` / `Cancels`；
- **内联触发行**：`TArray<FTcsEffectTriggerDef> Triggers`；
- **冷却策略**：`FTcsCooldownPolicy Cooldown`（**2026-10-11 本轮落地**——见下方"冷却与 Cost 字段的落地"）；
- **消耗配置**：`FTcsSkillCostConfig Cost`（**2026-10-12 类型名由 `FTcsCostConfig` 改**，
  见下方"消耗字段的策略化"）。

**属性捕获声明列表 MUST NOT 存在（2026-10-08 删除，MUST 记录理由以免后人"补回"）**：本类**先前**含 `TArray<FTcsCastAttrCapture> AttrCaptureList`，本变更**整体删除**该字段与 `FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型。删除判据三条**同时成立**：

1. **零消费者**：技能侧 `CapturedAttrs` 全仓**无任何类型读取**；伤害流程侧的同名概念是"0 填充 0 读取"（台账 `WAIT-7`）；
2. **机制重叠**：设计给捕获写的用途是"捕获命中 → 改 `CapturedAttrs`（随流程消失、零账本污染）"，而**流程属性黑板**（`FTcsFlowAttributes`："作用域 = 流程用完即弃"）与 `FlowModify` 数据步骤已提供同一件事 ⇒ 同一个"流程局部可变值空间"存在两份；
3. **技能侧已被参数快照占满**：技能**自己的参数**由施法运行态的 `ParamSnapshot` 在激活瞬间冻结（`AttributeScaled` 类源在快照构建时即取值）；**流程内工作值**由 `FTcsFlowAttributes` 承担。**剩下的空间说不出技能侧独有的业务场景**，设计语料中也**没有给出**技能侧独有的用例。

**删除的边界（MUST 记录）**：本删除**只涉技能侧**——伤害流程侧的 `FTcsDamageFlowContext::CapturedAttrs` 归台账 `WAIT-7`，其触发条件（"第一个属性修正型伤害修改器"）**不因此改变**，本删除**MUST NOT** 被读成"交付或关闭了 `WAIT-7`"。

**连带（MUST 记录）**：删除后 `TcsSkill` 对属性门面（`UTcsAttributeSubsystem` 及其白名单薄壳）**零依赖** ⇒ 本模块 MUST NOT 新建属性访问白名单薄壳（那是"为零消费者预建"）；`attribute-pipeline` 能力要求的"两模块各自白名单薄壳"**不扩为三处**。

**参数链行 `ParamChainRows` 已在本轮声明（2026-10-09，Task 4 按上条裁定自己的约定回补）**：Task 1 曾以"元素类型 `FTcsNumericParamModifier` 尚不存在、`UPROPERTY TArray<T>` 要求元素类型完整"为由把该字段**延后到 Task 4**，并明文约定"Task 4 以 MODIFIED 增补本需求"。本变更即 Task 4 ⇒ 该类型已落地（`Skill/TcsNumericParamModifier.h`），字段随之下落：`TArray<FTcsNumericParamModifier> ParamChainRows`。

**该字段的表达式边界（MUST 记录，防后人误读为"漏做模板引用"）**：本 Task **只落内联一种形态**——`ParamChainRows` 的元素是**内联的**修正器行。计划原文曾说它"可内联、也可**引用** `UTcsSkillModDef` 的模板身份（`TemplateTag`）"，但**实测 `UTcsSkillModDef` 在全仓 `Source/` 零命中**（269 个文件全扫）⇒ **该类型尚不存在，"引用目标"没有载体**。⇒ 模板引用路径**整体归 `R5.5-f`**（其批次内容原文即"`UTcsSkillModDef` 技能侧参数行分派"，模板资产本属它）；现在为"引用"造一个只有 `TemplateTag`、**无任何解析消费者**的资产类，正是"零消费者不预建"。**声明作用域**恒为**本条目自身**的修正器行，**无 `FTcsEntrySelector`**（该选择器保持专属"外部施加"场景）。**`FTcsEntrySelector` 引用位归 Task 4 的宿主命令式入口（其类型同样在 Task 4 落地），本类 MUST NOT 声明。**

**冷却与 Cost 字段的落地（2026-10-11，`R6.5-1` Task 0 回补；MUST 记录理由以免后人误读为"R6 漏写"）**：

- 形状 = `FTcsCooldownPolicy Cooldown`（`Def/TcsCooldownTypes.h`）+ `FTcsSkillCostConfig Cost`（`Def/TcsCostTypes.h`），
  两个类型均由本轮交付、均为 `USTRUCT(BlueprintType)` 纯数据（零行为）；
- **R6 §11.2 曾把"冷却策略 / Cost 配置"记为明确不落的字段**，当时的判据即本仓的"**零消费者不预建**"
  ——类型虽已可写，但**没有任何机制读它**，落一个只有字段、无门禁、无到期堆、无事件的配置面，
  得到的是"**看起来支持冷却、实际恒可放**"的静默假象。**R6.5-1 的 `TcsCooldownOps` / `TcsCostOps`
  正是它们的消费者** ⇒ 字段与消费者**同时到位**，回补成立。
  **⇒ MUST NOT 读成"R6 当初写漏了"**——那是同一条纪律的正确执行（当时落 = 有字段无机制）。
- **本轮只落字段形状，不含行为**：门禁③（`SAR_OnCooldown`）与门禁⑤（`SAR_CannotAfford`）的真实现、
  到期堆驱动、CDR 生效、四枚事件的发布，分别归 `R6.5-1` 的 Task 1 / 2 / 3 / 4。

**★ 消耗字段的策略化（2026-10-12，`PLN-R6.5-CC`；MUST 读，防据旧字面派活）**：

- **类型名 `FTcsCostConfig` → `FTcsSkillCostConfig`**：形状由
  `{Policy 枚举, Timing, ResourceAttr, Amount}` 改为 **`{Timing, CostMode}`**——
  "消耗什么 / 怎么付"从**枚举档位**改为**策略对象**（`CostMode` 是裸 `FInstancedStruct`，
  由 `meta = (BaseStruct = "...TcsSkillCostPolicy")` 收窄，装 `FTcsSkillCostPolicy` 的派生实例）；
- **默认值语义不变**：`Timing` 默认 `ECT_None` = **不消耗、恒可付、零日志**
  （与换型前 `Policy = ECP_None` **逐字同义**）；
- **本字段的"不消耗"表达 = `Timing == ECT_None`**（**MUST NOT** 用"空 `CostMode`"表达——
  那在 `Timing != None` 时是**配置错误**，fail-closed）；
- **资产迁移成本 = 0**（实测：全仓 `.uasset` 扫 `Cost` / `ResourceAttr` / `Cooldown` 零命中）。

**`TcsSkill` MUST NOT 依赖 `TcsDamage` / `TcsTargeting`**：链步骤是 `FInstancedStruct` 数据、运行时经执行器注册表分派，本模块代码 MUST NOT 具名任何领域步骤类型。

#### Scenario: 六个基类字段不在派生类重复声明

- **WHEN** 检查 `FTcsSkillDefData` 的字段列表与其基类 `FTcsStateDefBase`
- **THEN** `CategoryTags` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows` 只出现在基类；派生类**不含**这六个的同名同型字段

#### Scenario: 瞬发表达为空时段表

- **WHEN** 一个技能定义被配成瞬发（无施法过程）
- **THEN** 其 `Phases` 为**空数组**（长度 0）；查询契约据此把 `CanMoveNow` / `IsInterruptibleNow` 走 `DefSwitches` 档，而不是"有一个零时长段"的路径

#### Scenario: 查询契约三档各有字段载体

- **WHEN** 把 `CastQueryMode` 分别设为 `DefSwitches` / `PhaseTable` / `Custom`
- **THEN** `DefSwitches` 档读 `bInterruptibleDefault` / `bCanMoveDefault`、`PhaseTable` 档读 `Phases`、`Custom` 档读 `CastQueryFragment`——三档**都有对应字段**，不存在"枚举有档、字段缺失"

#### Scenario: 冷却与 Cost 字段不在本轮

> 场景标题是**建立时（2026-10-09）的轮次标签**（本仓裁定：`#### Scenario:` 标题在 MODIFIED
> 语义下**不可改名**，先例 = `archive/2026-10-04-add-tcs-state-module/specs/plugin-descriptor/spec.md:18`）
> ——**本场景的判据已随 `R6.5-1` Task 0 现况反转**，见下。

- **WHEN** 检查 `R6.5-1` Task 0 落地后的 `FTcsSkillDefData` 字段列表
- **THEN** **含** `FTcsCooldownPolicy Cooldown` 与 `FTcsSkillCostConfig Cost` 两个字段（R6.5-1 落地、消费者同批到位；消耗字段的类型名于 2026-10-12 策略化时由 `FTcsCostConfig` 改为此名）；**不含** `FTcsEntrySelector` 引用位字段（该路径归批次 `f` 的 `UTcsSkillModDef`，实测该类型全仓 `Source/` 零命中）

#### Scenario: 参数链行延后到 Task 4

> 场景标题里的「延后到 Task 4」是**建立时的轮次标签**（本仓裁定：`#### Scenario:` 标题在 MODIFIED
> 语义下**不可改名**，先例 = `archive/2026-10-04-add-tcs-state-module/specs/plugin-descriptor/spec.md:18`）
> ——本场景的**判据已随 Task 4 现况化**。

- **WHEN** 检查本轮落地后的 `FTcsSkillDefData` 字段列表
- **THEN** **含** `TArray<FTcsNumericParamModifier> ParamChainRows`（其元素类型已由 Task 4 交付 ⇒ 满足 `UPROPERTY TArray<T>` 的「元素类型完整」要求）；**不含**模板引用形态（`UTcsSkillModDef` 全仓 `Source/` 零命中 ⇒ 引用目标无载体，该路径归 `R5.5-f`）

#### Scenario: 属性捕获字段与类型已整体移除

- **WHEN** 检查本轮落地后的 `FTcsSkillDefData` 字段列表与 `Source/TcsSkill/Public/Def/` 的类型清单
- **THEN** **不含** `AttrCaptureList` 字段，也不存在 `FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型或 `TcsCastAttrCapture.h` 文件；且本模块**不存在**属性访问白名单薄壳（删除后本模块对属性门面零依赖）

#### Scenario: 顶替位默认值取最保守档

- **WHEN** 新建一个 `UTcsSkillDef` 资产且不显式设置顶替位
- **THEN** `bRetriggerOnActive` 为 `false`（`InstancePerEntity` 的默认行为是**驳回**而非顶替）

#### Scenario: 冷却与 Cost 两字段默认取值最保守

- **WHEN** 新建一个 `UTcsSkillDef` 资产且不显式设置冷却与消耗
- **THEN** `Cooldown.Tracks` 为空数组（**恒可放**）、`Cooldown.Timing == ECT_OnCastStarted`、
  `Cost.Timing == ECT_None`（**恒可付、零日志**）——两者都不改变既有施法流程的走向
