## Purpose

定义技能层 Def 家族的数据形状、资产身份、编辑期表行与作者期校验：`FTcsSkillDefData`（继承 `FTcsStateDefBase`）、`FTcsBoolSwitchRow`、`FTcsPhaseSpan` 与三个施法枚举、资产类 `UTcsSkillDef` 与编辑期表行 `FTcsSkillDefTableRow`，让技能定义成为可发现、可校验、可解析的内容资产。

## ADDED Requirements

### Requirement: 技能 Def 数据形状

`TcsSkill` MUST 提供 `FTcsSkillDefData : FTcsStateDefBase`（`USTRUCT(BlueprintType)`），承载**施法语义**。它 MUST 继承基类的全部字段并**白拿**之——`StatusTag` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows` MUST NOT 在本类重复声明。

本类 MUST 只补**施法语义**字段：

- **布尔开关表**：`TArray<FTcsBoolSwitchRow> BoolSwitches`——与基类的 `Params`（数值参数行）构成设计所称的"参数双表"；
- **施法时段表**：`TArray<FTcsPhaseSpan> Phases`——任意段数，"前摇/后摇"是项目命名惯例；**瞬发 MUST 表达为空表**（MUST NOT 用"单一零时长段"这一等价写法，否则查询契约的"瞬发"档无法与"一个零时长段"区分）；
- **查询契约三档**：`ECastQueryMode CastQueryMode`（`DefSwitches` 默认 / `PhaseTable` / `Custom`）+ `bool bInterruptibleDefault` / `bool bCanMoveDefault`（`DefSwitches` 档）+ `FInstancedStruct CastQueryFragment`（`Custom` 档）——三档**各有字段载体**，MUST NOT 出现"枚举声明了某一档但没有对应字段"的形状缺口；
- **AttrCapture 声明列表**：`TArray<FTcsCastAttrCapture> AttrCaptureList`——每条含**属性键**与**取值方**（`Instigator` / `Target`）两项，activate 时按声明捕获进运行态。**作用域边界（MUST 记录）**：这是**技能侧**捕获，与伤害流程侧的步骤级 `AttrCaptureList` 是**两层各自独立的机制**（`SPEC-04-skill` §2）；后者的字段 `FTcsDamageFlowContext::CapturedAttrs` 归台账 `WAIT-7`，**真消费者 = 第一个属性修正型伤害修改器，不是本轮** —— 本需求 MUST NOT 被读成"交付了 `WAIT-7`"；
- **主链**：`FGameplayTag CastChainId` + `EMainChainStart MainChainStart`（`OnCastStarted` 默认 / `OnPhaseEnter` / `OnCastCompleted` / `Custom`）+ `FGameplayTag MainChainStartPhaseTag`；
- **实例化**：`ECastInstancing Instancing`（默认 `InstancePerExecution`）；
- **关系字段**（同 `FTcsBuffDef` 形状）：`Blocks` / `Requires` / `Priority` / `Cancels`；
- **内联触发行**：`TArray<FTcsEffectTriggerDef> Triggers`。

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

### Requirement: 布尔开关行的归属

`FTcsBoolSwitchRow` MUST 定义在 **`TcsSkill`**（`Source/TcsSkill/Public/Def/TcsBoolSwitchRow.h`），MUST NOT 定义在 `TcsState`。形状 = `USTRUCT(BlueprintType) FTcsBoolSwitchRow{FGameplayTag Key, bool Base, ETcsParamMode Mode}`，其中 `ETcsParamMode` **从 `TcsState` 复用**（`EPM_Snapshot` 默认 / `EPM_Live`），本模块 MUST NOT 另立同义枚举。

**归属判据（2026-10-06 用户裁定 Q-8，MUST 记录以免后人"补回"基类）**：判据是基类自己的**"无时值 / 无堆叠 = 基类；有时值 / 有堆叠 = 派生"**——基类的数值参数表（`Params: TArray<FTcsNumericParamRow>`）被状态与技能两层共用故留基类；布尔开关的消费者**只有技能侧**故落派生类。**实测依据**：设计语料中 `BoolSwitches` 的消费者只有技能侧四处，且 `SPEC-02-states` 全文**零 `BoolSwitch` 命中**。

**反向风险登记**：若将来状态 / buff 也要布尔开关，把该纯数据类型**下移**到 `TcsState` 是机械改动（无行为、无反射契约）——现在为它预建在基类才是违反"零消费者不预建"。

#### Scenario: 布尔开关行住技能模块

- **WHEN** 检查 `FTcsBoolSwitchRow` 的声明位置
- **THEN** 它在 `TcsSkill` 模块内（`Source/TcsSkill/Public/Def/TcsBoolSwitchRow.h`），`TcsState` 中不存在该类型的声明；`TcsSkill` 不因此新增编译依赖（`ETcsParamMode` 复用 `TcsState` 的既有类型）

#### Scenario: 布尔开关本轮零内置消费者

- **WHEN** 检查本轮落地后谁读取 `BoolSwitches`
- **THEN** 只有**账本读取面**（Task 2）与**作者期校验**（本能力的资产校验需求）——`GateCheck` 求值器（住 `TcsEffect`）归 R6.5；本条如实登记"零内置运行期消费者"，MUST NOT 因此删掉字段（它的消费者是账本与宿主）

### Requirement: 技能 Def 资产身份

`TcsSkill` MUST 提供 `UTcsSkillDef : UTcsStateDef`（`UCLASS(BlueprintType)`），持 `FTcsSkillDefData SkillDef`。它 MUST 复用基类的资产身份机制：`static const FPrimaryAssetType PrimaryAssetType`（取值 = 类名 `"TcsSkillDef"`）+ 覆写 `GetPrimaryAssetId()`，身份 = `[PrimaryAssetType, DefTag.GetTagName()]`。

**本族数据结构名的消歧后缀 MUST 取 `Data`，资产类名保持 `UTcsSkillDef`**（`UTcsSkillDef` 对应数据 `FTcsSkillDefData`）——UHT 按"去前缀引擎名"判重，数据与资产同名会在 UHT 阶段即失败。**判据是二选一的**（`openspec/project.md:19`）：**资产名已有文档在位 ⇒ 数据加 `Data`**（属性族 / 技能族）；**数据名已被交付类型占用 ⇒ 资产加 `Asset`**（触发族 `UTcsEffectTriggerDefAsset` / Buff 族 `UTcsBuffDefAsset`）。**MUST NOT 把任一支写成唯一正解**。

本类 MUST NOT 让 `SkillDef` 字段可被蓝图写入（`BlueprintReadOnly` 取舍与状态族同款），也 MUST NOT 在族基类 `UTcsStateDef` 上添加技能专属字段。

#### Scenario: 主资产身份按 tag 解析

- **WHEN** 一个 `UTcsSkillDef` 资产的 `DefTag` 为 `SkillDef.Check.Fireball`
- **THEN** 其 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType("TcsSkillDef"), "SkillDef.Check.Fireball"]`，视觉上等同 `SkillDef.Check.Fireball` 这一身份；定义库可据该身份发现与去重

#### Scenario: 族基类不含技能专属字段

- **WHEN** 检查 `UTcsStateDef` 的字段列表
- **THEN** `UTcsStateDef` 只持 `DefTag`（及状态族共同机制），**不含**任何技能专属字段（`Phases` / `CastChainId` / `BoolSwitches` 等全在 `UTcsSkillDef.SkillDef` 内）

### Requirement: 技能 Def 表行（编辑期载体）

`TcsSkill` MUST 提供 `FTcsSkillDefTableRow : FTableRowBase`，形状 = `{FGameplayTag DefTag, FTcsSkillDefData SkillDef}`。

身份分工与状态族同款：`DefTag` = **内容身份**，`RowName` = **编辑期定位**——两者 MUST NOT 被要求同名。**运行期 MUST NOT 读 DataTable**（本行只服务作者期批量编辑与往返保真）。

#### Scenario: 表行字段往返保真

- **WHEN** 在 DataTable 内填写一行 `FTcsSkillDefTableRow`（`DefTag` + 完整 `SkillDef`），随后读回
- **THEN** 字段值逐一保真（纯反射数据，MUST NOT 有 DataTable 之外的额外约束）

#### Scenario: 运行期不读 DataTable

- **WHEN** 检查运行期代码路径
- **THEN** 没有任何运行期消费者读 `FTcsSkillDefTableRow` / DataTable（其消费面只有作者期编辑与校验）

### Requirement: 技能 Def 作者期校验

`UTcsSkillDef` MUST 在 `#if WITH_EDITOR` 下提供 `IsDataValid` 派生段，**在基类身份级校验之外**补内容级规则。至少覆盖下列各类，各自 MUST 报出可定位的错误：

1. `Params` 键重复 / `BoolSwitches` 键重复（同一键两行）；
2. `ModifierRows` 内含空引用；
3. `Phases` 内某段的 `Duration` 参数来源为空；
4. `Triggers` 内含 `EventTag` 无效或 `EffectChainId` 无效的行；
5. `CastChainId` 无效；
6. `Descriptions` 内配置项缺项（`DescriptionId` / `TextKey` 为空）。

**校验通过时 MUST 把 `NotValidated` 提升为 `Valid`**——MUST NOT 让一条已校验通过的资产停在 `NotValidated`（那会让编辑器一直显示"未校验"，是 R5 踩过的同款缺陷）。

#### Scenario: 六类错误各自报出

- **WHEN** 分别构造上述六类缺陷的 `UTcsSkillDef` 资产并触发校验
- **THEN** 每一类都产生可定位的错误信息（指向具体字段与下标），资产判定为 `Invalid`

#### Scenario: 校验通过时提升为 Valid

- **WHEN** 一个完全合法的 `UTcsSkillDef` 资产（含空 `Phases`）触发校验
- **THEN** 校验结果为 `Valid`，**不是** `NotValidated`；MUST NOT 出现 Error / ensure
