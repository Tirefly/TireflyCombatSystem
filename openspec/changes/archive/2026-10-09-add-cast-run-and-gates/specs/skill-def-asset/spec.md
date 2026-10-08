## MODIFIED Requirements

### Requirement: 技能 Def 数据形状

`TcsSkill` MUST 提供 `FTcsSkillDefData : FTcsStateDefBase`（`USTRUCT(BlueprintType)`），承载**施法语义**。它 MUST 继承基类的全部字段并**白拿**之——`StatusTag` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows` MUST NOT 在本类重复声明。

本类 MUST 只补**施法语义**字段：

- **布尔开关表**：`TArray<FTcsBoolSwitchRow> BoolSwitches`——与基类的 `Params`（数值参数行）构成设计所称的"参数双表"；
- **施法时段表**：`TArray<FTcsPhaseSpan> Phases`——任意段数，"前摇/后摇"是项目命名惯例；**瞬发 MUST 表达为空表**（MUST NOT 用"单一零时长段"这一等价写法，否则查询契约的"瞬发"档无法与"一个零时长段"区分）；
- **查询契约三档**：`ECastQueryMode CastQueryMode`（`DefSwitches` 默认 / `PhaseTable` / `Custom`）+ `bool bInterruptibleDefault` / `bool bCanMoveDefault`（`DefSwitches` 档）+ `FInstancedStruct CastQueryFragment`（`Custom` 档）——三档**各有字段载体**，MUST NOT 出现"枚举声明了某一档但没有对应字段"的形状缺口；
- **主链**：`FGameplayTag CastChainId` + `EMainChainStart MainChainStart`（`OnCastStarted` 默认 / `OnPhaseEnter` / `OnCastCompleted` / `Custom`）+ `FGameplayTag MainChainStartPhaseTag`；
- **实例化**：`ECastInstancing Instancing`（默认 `InstancePerExecution`）；
- **顶替位**：`bool bRetriggerOnActive = false`——`InstancePerEntity` 在飞时的分流位：`false`（**默认**）⇒ 驳回；`true` ⇒ 顶替。默认值 `false` 是**判据不是偏好**（GAS 的对应位无初值即 `false`，且本仓"框架零默认"纪律下默认档 MUST 取**行为最保守**的一档——驳回 > 顶替）；
- **关系字段**（同 `FTcsBuffDef` 形状）：`Blocks` / `Requires` / `Priority` / `Cancels`；
- **内联触发行**：`TArray<FTcsEffectTriggerDef> Triggers`。

**属性捕获声明列表 MUST NOT 存在（2026-10-08 删除，MUST 记录理由以免后人"补回"）**：本类**先前**含 `TArray<FTcsCastAttrCapture> AttrCaptureList`，本变更**整体删除**该字段与 `FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型。删除判据三条**同时成立**：

1. **零消费者**：技能侧 `CapturedAttrs` 全仓**无任何类型读取**；伤害流程侧的同名概念是"0 填充 0 读取"（台账 `WAIT-7`）；
2. **机制重叠**：设计给捕获写的用途是"捕获命中 → 改 `CapturedAttrs`（随流程消失、零账本污染）"，而**流程属性黑板**（`FTcsFlowAttributes`："作用域 = 流程用完即弃"）与 `FlowModify` 数据步骤已提供同一件事 ⇒ 同一个"流程局部可变值空间"存在两份；
3. **技能侧已被参数快照占满**：技能**自己的参数**由施法运行态的 `ParamSnapshot` 在激活瞬间冻结（`AttributeScaled` 类源在快照构建时即取值）；**流程内工作值**由 `FTcsFlowAttributes` 承担。**剩下的空间说不出技能侧独有的业务场景**，设计语料中也**没有给出**技能侧独有的用例。

**删除的边界（MUST 记录）**：本删除**只涉技能侧**——伤害流程侧的 `FTcsDamageFlowContext::CapturedAttrs` 归台账 `WAIT-7`，其触发条件（"第一个属性修正型伤害修改器"）**不因此改变**，本删除**MUST NOT** 被读成"交付或关闭了 `WAIT-7`"。

**连带（MUST 记录）**：删除后 `TcsSkill` 对属性门面（`UTcsAttributeSubsystem` 及其白名单薄壳）**零依赖** ⇒ 本模块 MUST NOT 新建属性访问白名单薄壳（那是"为零消费者预建"）；`attribute-pipeline` 能力要求的"两模块各自白名单薄壳"**不扩为三处**。

**参数链行 `ParamChainRows` MUST NOT 在本轮声明（2026-10-06 边界裁决，MUST 记录理由）**：其元素类型 `FTcsNumericParamModifier` 是 **Task 4** 的交付物（`TcsNumericParamModifier.h`），本 Task 在 Task 4 之前 ⇒ 该类型**尚不存在**。`USTRUCT` 的**反射数组属性要求元素类型完整**（`TArray<T>` 作 `UPROPERTY` 时 UHT 需完整类型），故**不能用前向声明占位**，也 MUST NOT 临时造一个"同形但不同名"的类型（那会制造第二真相）。⇒ 本字段**延后到 Task 4 一并落地**（Task 4 以 MODIFIED 增补本需求），本 Task 的形状需求 MUST NOT 出现该字段。**这不是遗漏，是实施顺序的必然**——计划 `PLN-R6` Task 1 Step 2 原文已交代该字段"按实施顺序定"，本裁定即其落点。

**结构性字段 MUST NOT 落在本类**（零消费者不预建 + 轮次切分）：**冷却策略与 Cost 配置归 R6.5**——本类 MUST NOT 声明任何冷却轨道 / 冷却时机 / Cost 策略 / Cost 时机字段；`FTcsEntrySelector` 引用位归 Task 4 的宿主命令式入口（其类型同样在 Task 4 落地），本类 MUST NOT 声明。

**`TcsSkill` MUST NOT 依赖 `TcsDamage` / `TcsTargeting`**：链步骤是 `FInstancedStruct` 数据、运行时经执行器注册表分派，本模块代码 MUST NOT 具名任何领域步骤类型。

#### Scenario: 六个基类字段不在派生类重复声明

- **WHEN** 检查 `FTcsSkillDefData` 的字段列表与其基类 `FTcsStateDefBase`
- **THEN** `StatusTag` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows` 只出现在基类；派生类**不含**这六个的同名同型字段

#### Scenario: 瞬发表达为空时段表

- **WHEN** 一个技能定义被配成瞬发（无施法过程）
- **THEN** 其 `Phases` 为**空数组**（长度 0）；查询契约据此把 `CanMoveNow` / `IsInterruptibleNow` 走 `DefSwitches` 档，而不是"有一个零时长段"的路径

#### Scenario: 查询契约三档各有字段载体

- **WHEN** 把 `CastQueryMode` 分别设为 `DefSwitches` / `PhaseTable` / `Custom`
- **THEN** `DefSwitches` 档读 `bInterruptibleDefault` / `bCanMoveDefault`、`PhaseTable` 档读 `Phases`、`Custom` 档读 `CastQueryFragment`——三档**都有对应字段**，不存在"枚举有档、字段缺失"

#### Scenario: 冷却与 Cost 字段不在本轮

- **WHEN** 检查 `FTcsSkillDefData` 的字段列表
- **THEN** 不存在冷却轨道 / 冷却时机 / Cost 策略 / Cost 时机 / `FTcsEntrySelector` 引用位字段（分别归 R6.5 与 Task 4）

#### Scenario: 参数链行延后到 Task 4

- **WHEN** 检查本轮落地后的 `FTcsSkillDefData` 字段列表
- **THEN** **不含** `ParamChainRows`（其元素类型 `FTcsNumericParamModifier` 尚不存在，`UPROPERTY TArray<T>` 要求元素类型完整 ⇒ 不能前向声明占位）；本 Task 的编译 MUST NOT 依赖该类型

#### Scenario: 属性捕获字段与类型已整体移除

- **WHEN** 检查本轮落地后的 `FTcsSkillDefData` 字段列表与 `Source/TcsSkill/Public/Def/` 的类型清单
- **THEN** **不含** `AttrCaptureList` 字段，也不存在 `FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型或 `TcsCastAttrCapture.h` 文件；且本模块**不存在**属性访问白名单薄壳（删除后本模块对属性门面零依赖）

#### Scenario: 顶替位默认值取最保守档

- **WHEN** 新建一个 `UTcsSkillDef` 资产且不显式设置顶替位
- **THEN** `bRetriggerOnActive` 为 `false`（`InstancePerEntity` 的默认行为是**驳回**而非顶替）
