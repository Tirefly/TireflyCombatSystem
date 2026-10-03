## MODIFIED Requirements

### Requirement: Damage 链步骤与执行器

`TcsDamage` MUST 提供链原语 `FTcsStepDamage`（住 `Public/Chain/TcsStepDamage.h`）：

- 字段：`FGameplayTag FlowTemplateId`（**无效 tag = 官方默认模板**；**2026-09-22 改造：类型 `FName` → `FGameplayTag`，空值判定 `IsNone()` → `!IsValid()`**）、`FTcsParamValue DamageBase`（基础伤害值输入——参数账本解算结果，PV-7）、`TMap<FGameplayTag, FTcsParamValue> FormulaParams`（公式参数初值；**2026-09-22 改造：键类型改 tag**）、`TargetAttrKey: FGameplayTag`（扣血属性键；**2026-09-22 改造：原 `FTcsAttributeName`**）；
  **MUST NOT 携带 `Conditions`**（链级条件归 M4a 触发行/求值器轮——R3 无链侧条件求值器，留着就是"配了没人执行"的死字段）；
  **MUST NOT 携带 `Delegate` / `HealthAttrKey`**（二者属**流程模板**配置：公式/护盾 hook 在模板的 `FTcsFlowBaseDamage`/`FTcsFlowExecute` 步骤上、扣血属性键在 `FTcsFlowExecute.AttrKey`）——同一件事只有一处配置，避免双真相与死字段（计划 sketch 曾把两者列在本 struct，落地收窄）；
- 执行器 MUST：构 `FTcsDamageFlowContext`（`Attacker`/`Instigator` 取 `Context.Caster`/`Instigator` 句柄、`Targets` 取 `Context.Targets`——**消费目标集，不内嵌选择器**，D4-4 v2）→ 把 `DamageBase` 与 `FormulaParams` 写入上下文（黑板契约键 `DamageFlowKey.BaseDamage` / 公式参数表）→ `RunTemplate` → 恒返回 `TSR_Completed`（流程单帧同步完成，无挂起）；
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

### Requirement: 伤害记录与事件

`TcsDamage` MUST 以扁平记录承载伤害结果（09 §2.4）：

- `FTcsDamageRecord`（USTRUCT）：`uint64 FlowId` / `FTcsCombatEntityHandle Source` / `FTcsCombatEntityHandle Target` / `FGameplayTag Element` / `bool bHit` / `bool bCrit` / `double Base` / `double Final` / `double Executed` / `double Absorbed` / `bool bKill` / `uint64 Sequence` / `double Timestamp`；
- **参与者用实体句柄**（**MUST NOT 用 `AActor*`**——承接句柄化）；**字段全扁平**（无 `TMap`/`TFunction`——符合过网结构纪律，将来可直接上 FastArray）；
- `bKill` 的语义 = **记账而非裁定**：本次事务提交后目标 `Health ≤ 0`（死亡规则仍归宿主）；
- `FTcsFlowCompleted` 步骤 MUST 填充记录并经总线**立即通道**发布事件（**原生 Tag** `TcsEvent.Damage.Recorded`，载荷 = 该记录；根 = `TcsEvent`，由本模块原生声明）；
- `UTcsDamageSubsystem` MUST 持**环形缓冲**（容量常量初值 128）并提供读取口 `GetRecentRecords(TArray<FTcsDamageRecord>& OutRecords) const`（旧→新序）。

#### Scenario: 完成即出记录

- **WHEN** 默认模板跑完
- **THEN** 记录事件在 `RunTemplate` 返回前派发（立即通道）、环形缓冲可读回同一笔记录（字段与本次伤害一致）

#### Scenario: 记录可跨模块消费

- **WHEN** 装置/宿主订阅该原生 Tag
- **THEN** 收到载荷即拿到完整记录（无需访问流程上下文——记录是结果快照）

#### Scenario: 记录 tag 是插件原生常量

- **WHEN** 检查 `TcsEvent.Damage.Recorded` 的声明处与导出宏
- **THEN** 它是插件原生声明且带模块导出宏（跨模块订阅不需要宿主在 tag 表里配置任何东西）

### Requirement: ModifyFlow 链原语（伤害修改器通道提交侧）

`TcsDamage` MUST 提供链原语 `FTcsStepModifyFlow`（住 `Public/Chain/TcsStepModifyFlow.h`）——D7-6"伤害修改器唯一通道"的**提交侧**：触发行为命中后起一条单步链，本步骤从链上下文取回流程上下文，向流程黑板提交一笔修正。

- 字段（全部 `UPROPERTY`，可经链资产序列化编辑）：
  - `FGameplayTag TargetKey`（目标黑板键；**无效 tag = 落契约键 `DamageFlowKey.BaseDamage`**——按 `project.md` 的 `FGameplayTag` 字段默认值陷阱，兜底 MUST 在执行器内完成，MUST NOT 依赖字段默认值）；
  - `ETcsAttributeOp Op`（运算带；带序唯一真相在 `Op`，MUST NOT 在本原语内另立带序）；
  - `FTcsParamValue Operand`（操作数；PV 载体，`Literal` 可配、引用型源随其来源策略轮）；
  - `FTcsDamageModifierConsumePolicy Consume`（消耗策略；**本原语只存不裁**——裁决与消费归 `FTcsFlowExecute`）；
- 执行器 MUST：
  1. 从 `FTcsEffectContext::EventPayload` 取 `FTcsDamageFlowCollectEvent`（`FInstancedStruct`；**已就位字段**），解出 `FTcsDamageFlowContext*`；
  2. 载荷缺失或类型不符 → **不崩溃、不静默通过**：留 Warning、按完成处理（`TSR_Completed`）；
  3. 调 `FTcsFlowAttributes::Submit(Key, Op, Operand, Consume)`；`Submit` 返回 false（键无效）→ 留 Warning；
  4. 恒返回 `TSR_Completed`（即时步骤，无挂起语义）；
- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **自注册进 TcsEffect 的执行器注册表**（D4-14 的又一次跨模块实证）；
- **MUST NOT**：把流程上下文类型带回 `TcsEffect`（`TcsEffect` 全程不认识任何 TcsDamage 类型——流程上下文经 `EventPayload` 这一已就位的中立通道抵达）；把 `TcsDamageFlowContext` 的结构知识写进 `TcsEffect`；
- **MUST NOT** 自行推导基础伤害（零公式纪律，同 `FTcsStepDamage`）；**MUST NOT** 内嵌目标集或流程模板 id（目标由链上 `SelectTargets` 或上下文默认目标表达）。

#### Scenario: 触发行命中后经单步链提交修正

- **WHEN** 某状态/装备的触发行订阅流程收集事件，命中后起一条单步链 `[ModifyFlow]`，链上下文的 `EventPayload` 装着 `FTcsDamageFlowCollectEvent`
- **THEN** 该步骤向黑板 `TargetKey`（或未配时的 `DamageFlowKey.BaseDamage`）提交一笔 `{Op, Operand, Consume}`，后续 `Execute` 步骤读到的折叠值包含这笔贡献

#### Scenario: 载荷缺失不崩溃

- **WHEN** 该步骤在一个没有流程载荷的链上被执行（`EventPayload` 为空或类型不符）
- **THEN** 步骤留 Warning 并按完成处理，链继续走下一步（不崩溃、不静默通过、不产生半笔提交）

#### Scenario: 跨模块注册可查且 TcsEffect 零改动

- **WHEN** TcsDamage 模块加载后查询 TcsEffect 的执行器注册表
- **THEN** `FTcsStepModifyFlow` 的执行器已登记；且 `git diff -- Source/TcsEffect` 为空

#### Scenario: 消耗策略随链资产可配可存

- **WHEN** 在链资产里为 `FTcsStepModifyFlow` 配置 `Consume.MaxUses = 3` 并保存、重载资产
- **THEN** 配置值原样保留（本需求成立的**前提**是 `FTcsDamageModifierConsumePolicy` 为纯数据可反射结构——见下一条需求）

### Requirement: 消耗策略为纯数据可反射结构

`FTcsDamageModifierConsumePolicy` MUST 是**纯数据**结构，MUST NOT 含任何 `TFunction` / 闭包成员——其字段集合限为 `{ int32 MaxUses; double Cooldown; int32 SortKey; }`（三字段均有既定语义；`SortKey` 已有消费者：`FTcsFlowExecute` 的候选裁决比较）。

- **理由**：含闭包的结构无法出现在任何 `UPROPERTY` 上（UHT 编译错误，`UhtSession.cs:2645`）⇒ 配置面（链步骤 / 数据步骤）一律带不上消耗策略；同时它也是"流程上下文物理不可反射化"的**唯一根因**（台账 `SCRIPT-3`），并违反复制安全纪律（`FTcsDamageRecord.h` 明文：过网结构 MUST NOT 出现 `TFunction`）。
- **消费行为的表达方式 = 事件语义**（`DEC-04` §3.3 裁定 ④）：MUST NOT 用内联闭包；消费成功时的行为由**消费事件 + 宿主经触发行订阅**表达——可序列化、可反射、可脚本可达，且复用宿主既有的订阅通道，零新机制。
- 消费事件 MUST 为插件原生声明的 `FGameplayTag`（**`TcsEvent.Damage.<事件名>`**，根 = `TcsEvent`；命名与归属公约见 `gameplay-tag-governance` 能力），**MUST NOT** 由宿主 ini 声明——该事件属**框架词汇**（框架发布、宿主订阅），宿主漏配会导致事件静默丢失。**边界**：本禁令约束的是**框架发布的契约事件**；宿主**自发布**的事件不在此列（它们由宿主 ini 在 `TcsEvent` 根下声明）。
- **本需求只定形状，不实现消费动作**：`FTcsFlowExecute` "成功才消费"（发事件 / 标记已消费 / 扣 `MaxUses` / 起冷却）归台账 `DAMAGE-4`（R5/M4a），与本形状同批落地时不得再改结构。

#### Scenario: 结构可作 UPROPERTY

- **WHEN** 某配置 struct（链步骤或数据步骤）以 `UPROPERTY` 持有 `FTcsDamageModifierConsumePolicy`
- **THEN** UHT 编译通过（改造前必定报错）

#### Scenario: 消费行为经事件表达

- **WHEN** 宿主希望"某修改器被消费时执行一段自定义逻辑"
- **THEN** 宿主订阅消费事件并由触发行起链表达该逻辑——无需插件提供回调字段、无需写 C++

#### Scenario: 拆掉闭包后上下文反射化的阻碍消失

- **WHEN** 评估"把 `FTcsDamageFlowContext` 逐字段反射化"（台账 `SCRIPT-3`）
- **THEN** 其递归链上的 `TFunction` 已不存在（本改造前，`FTcsFlowAttributes → FTcsFlowAttributeSubmit → FTcsConsumePolicy::OnConsumed` 是唯一根因——该类型现已改名 `FTcsDamageModifierConsumePolicy` 且该成员已删除）

### Requirement: 收集事件载荷读取器登记

`TcsDamage` MUST 为自己发布的流程收集事件载荷（`FTcsDamageFlowCollectEvent`）登记**载荷读取器**——这是台账登记册 `LEDGER-reflection` R-2 调研发现的**功能未做**（`UE_DEFINE_TRIGGER_PAYLOAD_READER` 宏全库只有定义、零登记），用户 2026-09-27 裁定**并入本批**（与反射无关，属领域侧该做未做）。

- 登记 MUST 走**静态自注册宏**（`UE_DEFINE_TRIGGER_PAYLOAD_READER`，住 TcsDamage 模块）——属主模块登记自己领域载荷的读取器，MUST NOT 依赖宿主或运行期动态入口；
- 读取器 MUST 从 `FTcsDamageFlowCollectEvent::Context` 映射 `FTcsTriggerPayloadInfo`：
  - `Caster` ← `FTcsDamageFlowContext::Attacker`（**攻击方**；"谁发起的"语义在触发行侧由 `Instigator` 承载，不在此）；
  - `ClassificationTags` ← `FTcsDamageFlowContext::ClassificationTags`（直通）——**这一条是本读取器的关键作用：它是"流程侧匹配面 ↔ 触发侧匹配面"的桥**。分类集的词 MUST 住 **`DamageCategory`** 根（消费角色 = 供条件匹配的伤害分类集；声明方 = 宿主 ini；形态 `DamageCategory.<词>`，2 段；全库唯一写入点 `TcsFlowStepsRest.cpp:139`，写链与判据边界见 `gameplay-tag-governance` 的根段注册表备注）。**MUST NOT** 在本读取器里做词表转换/映射（例如把元素词翻译成另一套词）——那会让两侧匹配面各持一套词，破坏"两个匹配面共用一套词"；
- 载荷缺失或 `Context` 为空指针 → MUST 返回**默认构造**的 `FTcsTriggerPayloadInfo`（空句柄 + 空标签集），MUST NOT 崩溃、MUST NOT 静默编造主体；
- 读取语义 MUST 保住"**每事件一次**"（求值器已在各行之间共用一次读取）——本登记 MUST NOT 把读取点挪进逐候选行循环；
- **本需求是"登记缺失"的补齐，不是新机制**：注册表本体与寿命语义已由 `effect-trigger` 能力规定，本需求只规定"TcsDamage 该登记而没登记"这一件事。

#### Scenario: 收集事件的主体可被触发行读到

- **WHEN** 某触发行订阅 `TcsEvent.Damage.AfterDamage`（或任一收集事件），流程走到该步骤发布事件
- **THEN** 求值器经本读取器拿到 `Caster` = 本流程的 `Attacker`、`ClassificationTags` = 流程上下文的分类标签集；以 `Caster` 为施法者的触发行能正常起链（不再因"无读取器"而拿到空句柄）

#### Scenario: 空载荷安全降级

- **WHEN** 载荷的 `Context` 为空指针（或缺读取器时的降级路径）
- **THEN** 读取器返回默认构造结果（空句柄 / 空标签集），不崩溃、不误判主体
