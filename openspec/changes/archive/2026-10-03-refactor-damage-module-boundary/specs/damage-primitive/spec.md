## MODIFIED Requirements

### Requirement: Damage 链步骤与执行器

> **基准说明**：本条 delta 以 **`reroot-gameplay-tag-vocabulary` 换根后的文本**为基准撰写，依赖该提案**先归档**。
> **本条同时承载审计 `R-2` 的修复**（规格与实现失步）：原规格声称执行器"把 `DamageBase` 与 `FormulaParams` 写入上下文（黑板契约键 `DamageFlowKey.BaseDamage` / 公式参数表）"，而实现**从不写黑板**——**处置 = 改规格、不改实现**（实现的重复计数实测理由更硬，见下）。

`TcsDamage` MUST 提供链原语 `FTcsStepDamage`（住 `Public/Chain/TcsStepDamage.h`）：

- 字段：`FGameplayTag FlowTemplateId`（**无效 tag = 官方默认模板**；**2026-09-22 改造：类型 `FName` → `FGameplayTag`，空值判定 `IsNone()` → `!IsValid()`**）、`FTcsParamValue DamageBase`（基础伤害值输入——参数账本解算结果，PV-7）、`TMap<FGameplayTag, FTcsParamValue> FormulaParams`（公式参数初值；**2026-09-22 改造：键类型改 tag**）、`TargetAttrKey: FGameplayTag`（扣血属性键；**2026-09-22 改造：原 `FTcsAttributeName`**）；
  **MUST NOT 携带 `Conditions`**（链级条件归 M4a 触发行/求值器轮——R3 无链侧条件求值器，留着就是"配了没人执行"的死字段）；
  **MUST NOT 携带 `Delegate` / `HealthAttrKey`**（二者属**流程模板**配置：公式/护盾 hook 在模板的 `FTcsFlowBaseDamage`/`FTcsFlowExecute` 步骤上、扣血属性键在 `FTcsFlowExecute.AttrKey`）——同一件事只有一处配置，避免双真相与死字段（计划 sketch 曾把两者列在本 struct，落地收窄）；
- 执行器 MUST：构 `FTcsDamageFlowContext`（`Attacker`/`Instigator` 取 `Context.Caster`/`Instigator` 句柄、`Targets` 取 `Context.Targets`——**消费目标集，不内嵌选择器**，D4-4 v2）→ 把 `DamageBase` 与 `FormulaParams` 写入**上下文请求字段**（`Context.BaseDamageInput` / `Context.FormulaParams`，外加扣血目标键 `Context.TargetAttrKey`）→ `RunTemplate` → 恒返回 `TSR_Completed`（流程单帧同步完成，无挂起）；
- **执行器 MUST NOT 写黑板（`R-2` 更正项）**：本步骤是**请求方**，只负责把输入填进**上下文请求字段**；黑板写入归流程内的 `FTcsFlowBaseDamage` 步（以 **Add** 提交到契约键 `DamageFlowKey.BaseDamage`）。
  **理由（实测，MUST NOT 回退）**：黑板会被 `CollectStart` 重置（**收集语义**），而输入是"请求"的一部分、**不随收集清除**。若改由本步骤从黑板读输入，则"链侧预写 + 本步提交"会造成**重复计数**（实测：`10 + 20 = 30`）。故输入通道 MUST 保持为**上下文请求字段**，MUST NOT 改为黑板键；
- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **自注册进 TcsEffect 的执行器注册表**（D4-14 的又一次跨模块实证：TcsDamage 不依赖 TcsTargeting，TcsEffect 不认识伤害语义）；
- **流程零计算纪律**：步骤 MUST NOT 自行推导基础伤害（`DamageBase` 即解算结果；复合运算由链侧参数链承载——PV-7/M5 轮）。

#### Scenario: 链上跑通到流程

- **WHEN** 一条链执行到 `[Damage]` 步骤（目标集已由前置步骤或调用方填好）
- **THEN** 流程按模板跑完，目标的 `TargetAttrKey` 属性被事务扣减，记录事件已发布

#### Scenario: 跨模块注册可查

- **WHEN** TcsDamage 模块加载后查询 TcsEffect 的执行器注册表
- **THEN** `FTcsStepDamage` 的执行器已登记（零启动代码、TcsEffect 侧零改动）

#### Scenario: 无效模板 id 走官方默认

- **WHEN** 步骤的 `FlowTemplateId` 为无效 tag（未配置）
- **THEN** 执行器按官方默认模板（`DamageFlowTemplate.Default`）起流程（与改造前"空 FName 兜底 `Default`"行为一致）

#### Scenario: 输入经上下文请求字段而非黑板

- **WHEN** 检查 `FTcsStepDamage` 的执行器对 `FTcsDamageFlowContext` 的写入
- **THEN** 它只写 `BaseDamageInput` / `FormulaParams` / `TargetAttrKey` 三个**请求字段**，**不调用任何黑板提交**；黑板上的 `DamageFlowKey.BaseDamage` 由流程内的 `FTcsFlowBaseDamage` 步以 Add 写入

#### Scenario: 链侧预写不与流程提交重复计数

- **WHEN** 一条链在 `[Damage]` 之前已向黑板写入一个 `BaseDamage` 值，随后流程跑完
- **THEN** 最终基值**不出现两次相加**（输入只经 `Context.BaseDamageInput` 递送一次；实测反例为 `10 + 20 = 30`）

### Requirement: 伤害记录与事件

> **基准说明**：本条 delta 以 **`reroot-gameplay-tag-vocabulary` 换根后的文本**为基准撰写（tag 名一律用换根后的新名），依赖该提案**先归档**——理由见 `proposal.md`「跨提案依赖」。

`TcsDamage` MUST 以扁平记录承载伤害结果（09 §2.4）：

- `FTcsDamageRecord`（USTRUCT）：`uint64 FlowId` / `FTcsCombatEntityHandle Source` / `FTcsCombatEntityHandle Target` / `bool bHit` / `bool bCrit` / `double Base` / `double Final` / `double Executed` / `double Absorbed` / `bool bKill` / `uint64 Sequence` / `double Timestamp`；
- **参与者用实体句柄**（**MUST NOT 用 `AActor*`**——承接句柄化）；**字段全扁平**（无 `TMap`/`TFunction`——符合过网结构纪律，将来可直接上 FastArray）；
- **`FGameplayTag Element` 字段已删除（2026-10-01，**BREAKING**——记录形状变更）**，元素改由 **`DamageCategory` 词表 + `ClassificationTags` 承载**：全库对 `Element` 的操作只有一处且是**置空**（`TcsFlowStepsCore.cpp` 的 `Record.Element = FGameplayTag();`），**零消费者**；元素的实际承载早已是分类集（`FTcsFlowElement` 步骤 → 宿主 `ResolveElement` → `ClassificationTags.AddUnique(Element)`）。**MUST NOT 在记录形状里复活"元素"这个玩法名词**——它违反主判据 *"凡是需要宿主词汇的内置策略，插件一律不提供"*（判据体系与 32 项"不许删名单"见 `Documents/combat-system-design/research/boundary-audit.md`）：框架记录形状里烘进一个由宿主词表决定的玩法名词，正是该规则要拦的形态；元素语义的正确落点是**宿主词汇（`DamageCategory`）+ 通用载体（`ClassificationTags`）**。**记录里要判元素，经事件载荷/触发条件侧的分类匹配表达**（`FTcsTriggerPayloadInfo::ClassificationTags` 已直通）；
- **`bHit` / `bCrit` 的取值 MUST 从契约键读回**（`DamageFlowKey.Hit` / `DamageFlowKey.Crit`）：框架 **MUST NOT** 在记录里硬编码命中结果——"本次是否命中"是宿主判定（`HitRate` 键由宿主响应方改写、`FTcsFlowHit` 步按 `>= 1.0` 折算），框架只搬运。将 `bHit` 写死为 `true` 属**硬编码常量字段**（取值恒定、零信息量，且替宿主断言"本次一定命中"）；
- **`bKill` 的语义 = 记账而非裁定，且致死线 MUST 可被宿主覆盖**：框架只记"本次事务提交后目标生命是否已到致死线"，判定经 `ITcsDamageFlowDelegate::IsLethal` 提供（中性默认 = 当前值 `<= 0.0`），**死亡规则仍归宿主**——框架 MUST NOT 依据该标志杀实体、MUST NOT 改动其他行为。把阈值硬编码在流程步骤内属**框架内定**（不可替换的框架模型，不满足契约默认的"可被宿主覆盖"条件），MUST 改为**契约默认**；
- `FTcsFlowCompleted` 步骤 MUST 填充记录并经总线**立即通道**发布事件（**原生 Tag** `TcsEvent.Damage.Recorded`，载荷 = 该记录；根 = `TcsEvent`，由本模块原生声明）；
- `UTcsDamageSubsystem` MUST 持**环形缓冲**（容量常量初值 128）并提供读取口 `GetRecentRecords(TArray<FTcsDamageRecord>& OutRecords) const`（旧→新序）。

**实现与迁移纪律（记录形状的破坏性变更）**：删字段 MUST 同批完成三件事——① 删 `FTcsDamageRecord::Element` 声明与唯一写入点；② 全仓反查零残留（`Element` 只允许作为**步骤名 / 事件名**出现：`FTcsFlowElement` 步骤、`TcsEvent.Damage.Element` 事件）；③ 兼容性核对——该字段虽**零消费者**，但记录是**过网/载荷结构**（`TcsEvent.Damage.Recorded` 的载荷），已序列化的**蓝图/资产里若手工拼过该字段**会在加载期失配，故 MUST 用加载告警面 + 一次指向性检查（打开含该事件的蓝图/DataTable 资产）确认无残留引用，并在实现记录里留证。

#### Scenario: 完成即出记录

- **WHEN** 默认模板跑完
- **THEN** 记录事件在 `RunTemplate` 返回前派发（立即通道）、环形缓冲可读回同一笔记录（字段与本次伤害一致）

#### Scenario: 记录可跨模块消费

- **WHEN** 装置/宿主订阅该原生 Tag
- **THEN** 收到载荷即拿到完整记录（无需访问流程上下文——记录是结果快照）

#### Scenario: 记录 tag 是插件原生常量

- **WHEN** 检查 `TcsEvent.Damage.Recorded` 的声明处与导出宏
- **THEN** 它是插件原生声明且带模块导出宏（跨模块订阅不需要宿主在 tag 表里配置任何东西）

#### Scenario: 命中结果由契约键决定而非硬编码

- **WHEN** 默认模板跑完，且模板未组装 `FTcsFlowHit` 步（`DamageFlowKey.Hit` 无提交）
- **THEN** `bHit` 取契约键的读回值（无提交即折叠初值 0 ⇒ `false`）；框架 MUST NOT 写入恒 `true`

#### Scenario: 命中步组装后 bHit 如实反映

- **WHEN** 模板组装了 `FTcsFlowHit` 步且宿主响应方把 `DamageFlowKey.HitRate` 改写为 `>= 1.0`
- **THEN** `bHit = true`；未达 1.0 时 `bHit = false`

#### Scenario: 致死线可被宿主覆盖

- **WHEN** 宿主经 `IsLethal` 覆写致死判定（如"生命 ≤ 1 即视为致死"或引入无敌帧）
- **THEN** `bKill` 按宿主判定记账；框架不杀实体、不改动其他行为

#### Scenario: 元素语义经分类集表达而非记录字段

- **WHEN** 检查 `FTcsDamageRecord` 的字段集与 `FTcsFlowElement` 步骤的写入路径
- **THEN** 记录中**不含**任何元素字段；元素 Tag 由 `ResolveElement` 产出后进 `ClassificationTags`（`DamageCategory` 根），需要按元素判定的消费者经分类匹配表达

#### Scenario: 删字段后零残留

- **WHEN** 全仓反查 `Element`
- **THEN** 只命中步骤名（`FTcsFlowElement`）、事件名（`TcsEvent.Damage.Element`）与历史文档；无字段声明、无字段写入、无字段读取

### Requirement: 流程委托契约（宿主实现）

> **基准说明**：本条**未被换根提案改动**（其 delta 不含本条），故基准 = 当前生效规格文本。

`TcsDamage` MUST 提供 `ITcsDamageFlowDelegate`（UINTERFACE，宿主实现；**全部函数带中性默认实现**——"普通项目零 delegate"，PV-7/D7-2）：

- **反射面（2026-09-24 新增，台账 SCRIPT-8）**：**7 方法** MUST 带 `UFUNCTION(BlueprintNativeEvent)`，且 MUST 在接口类内声明对应的 `virtual <名>_Implementation(...)`——UHT 检测到声明则**不生成默认 stub**（`UhtFunction.cs:681` 的 `ImplFound`），故中性默认实现住接口声明处（同款先例 = 引擎 `ISequencerAnimationOverride`，`SequencerAnimationOverride.h:31-44`；本仓 `ITcsAttributeProvider` 为无默认体的同族先例）；
- **形参 MUST 全反射**：上下文参数类型 MUST 为 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)` 反射只读视图），**MUST NOT** 为 `FTcsDamageFlowContext`——后者是**纯 C++ struct（无 `USTRUCT`）**，出现在 `UFUNCTION` 签名里会让 UHT 报 `Unable to find 'struct' with name ...`（同款既有实证：`UTcsEffectSubsystem::ExecuteChain` 因此无法标记，只能另开 `ExecuteChainForCaster`）；
- **C++ 调用点 MUST 走 `ITcsDamageFlowDelegate::Execute_<名>`**（**MUST NOT** 虚表直调）——虚表直调会**静默跳过**脚本层实现（C# 覆写走 `ProcessEvent`），表现为"公式不生效"而非崩溃。`Execute_` 内部先查 `UFunction` 走反射、查不到才回落原生接口实现，故**C++ 实现与脚本实现双轨并存**；
- 7 方法签名：
  - `double GetBaseHitRate(攻击者句柄, 目标句柄, const FTcsDamageFlowContextView&)`——默认 `1.0`；
  - `double GetBaseCritRate(...)`——默认 `0.0`；
  - `FGameplayTag ResolveElement(...)`——默认空 Tag；
  - `double CalculateBaseDamage(double IncomingBase, ...)`——**降级逃生口**（默认实现 = 返回传入值，即"不改动输入"）；
  - `double ModifyShield(目标句柄, double IncomingDamage, ...)`——默认 `0.0`（无护盾）；
  - `double ResolveExecutedDamage(目标句柄, double CandidateDamage, double AbsorbedDamage, ...)`——**吸收 → 执行量的映射**（默认实现 = `FMath::Max(0.0, CandidateDamage - AbsorbedDamage)`，即"减法模型 + 零下限"，与整肃前行为一致）；
  - `bool IsLethal(目标句柄, double CurrentValueAfterApply, ...)`——**致死线判定**（默认实现 = `CurrentValueAfterApply <= 0.0`，与整肃前行为一致）；
- **为什么"吸收 → 执行量"必须可覆盖（判据，MUST NOT 回退为步骤内硬编码）**：`FMath::Max(0.0, ·)` 的**下限钳**只引用数值、方向收敛到"不介入"，属合法的**契约默认**；但 **`Candidate - Absorbed` 这个减法模型本身**是框架内定——宿主只能提供"吸收了多少"，无法换成按比例分摊 / 带溢出上限 / 先扣护盾再溢出到生命等任何别的模型。**没有覆盖点的框架模型不是契约默认**（契约默认 MUST 同时满足"只引用框架自身契约"与"运行期可被宿主覆盖"两条）；
- **同理，致死线 MUST NOT 由步骤硬编码**：框架自称"记账而非裁定"，而 `<= 0.0` 恰是一次裁定——它断言了"生命归零即死亡"这一玩法模型；改为委托后，框架只保留"中性默认"，裁定权归宿主；
- 契约全部以**实体句柄**为参与者（承接 2026-09-20 句柄化）；插件 MUST NOT 内置任何公式。

#### Scenario: 零 delegate 也能跑

- **WHEN** 链步骤的 `Delegate` 为空（宿主完全不实现）
- **THEN** 默认模板仍能跑通（命中率 1、暴击 0、元素空、基础值不被改写、无护盾、执行量 = 候选减吸收且不为负、致死线 = 0）

#### Scenario: 宿主自定义公式

- **WHEN** 宿主实现 `CalculateBaseDamage` 返回自己的公式结果
- **THEN** `BaseDamage` 步骤采用该值（逃生口生效）

#### Scenario: 脚本层实现可被 C++ 调用点抵达

- **WHEN** 宿主脚本层（C#）实现 `CalculateBaseDamage` 并把实现对象配进流程模板的 delegate 字段，随后流程跑到 `FTcsFlowBaseDamage`
- **THEN** 基础伤害值等于脚本实现返回的值（`Execute_` 经 `ProcessEvent` 抵达脚本实现）——**这是"自定义伤害流程可用脚本编写"的判据**

#### Scenario: C++ 实现与脚本实现并存

- **WHEN** 宿主以 C++ 类（非脚本）实现同一接口并配进模板
- **THEN** 行为与换型前一致（`Execute_` 的 `else if` 分支回落原生 `_Implementation`，无需宿主额外适配）

#### Scenario: 吸收模型可被宿主替换

- **WHEN** 宿主实现 `ResolveExecutedDamage` 返回非减法模型的结果（如按比例分摊、或护盾溢出到生命）
- **THEN** 步骤采用该返回值作为本次执行量，MUST NOT 回落为框架的减法结果

#### Scenario: 致死线可被宿主替换

- **WHEN** 宿主实现 `IsLethal` 返回自定义判定
- **THEN** `bKill` 依据该判定记账；框架 MUST NOT 用内置阈值覆盖它

#### Scenario: 新钩子不破坏既有宿主实现

- **WHEN** 宿主已有实现类**未**声明这两个新方法
- **THEN** 编译通过且行为等于中性默认（接口内 `_Implementation` 默认体生效，UHT 不生成 stub）——宿主零适配，只需重编译
