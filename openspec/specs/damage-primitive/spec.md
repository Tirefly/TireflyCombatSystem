# damage-primitive Specification

## Purpose
定义 Damage 链步骤与执行器、供宿主实现的流程委托契约、流程上下文的反射视图、伤害记录与事件，以及流程模板登记面的反射可达性。

## Requirements

### Requirement: Damage 链步骤与执行器

`TcsDamage` MUST 提供链原语 `FTcsStepDamage`（住 `Public/Chain/TcsStepDamage.h`）：

- 字段：`FGameplayTag FlowTemplateId`（**无效 tag = 官方默认模板**；**2026-09-22 改造：类型 `FName` → `FGameplayTag`，空值判定 `IsNone()` → `!IsValid()`**）、`FTcsParamValue DamageBase`（基础伤害值输入——参数账本解算结果，PV-7）、`TMap<FGameplayTag, FTcsParamValue> FormulaParams`（公式参数初值；**2026-09-22 改造：键类型改 tag**）、`TargetAttrKey: FGameplayTag`（扣血属性键；**2026-09-22 改造：原 `FTcsAttributeName`**）；
  **MUST NOT 携带 `Conditions`**（链级条件归 M4a 触发行/求值器轮——R3 无链侧条件求值器，留着就是"配了没人执行"的死字段）；
  **MUST NOT 携带 `Delegate` / `HealthAttrKey`**（二者属**流程模板**配置：公式/护盾 hook 在模板的 `FTcsFlowBaseDamage`/`FTcsFlowExecute` 步骤上、扣血属性键在 `FTcsFlowExecute.AttrKey`）——同一件事只有一处配置，避免双真相与死字段（计划 sketch 曾把两者列在本 struct，落地收窄）；
- 执行器 MUST：构 `FTcsDamageFlowContext`（`Attacker`/`Instigator` 取 `Context.Caster`/`Instigator` 句柄、`Targets` 取 `Context.Targets`——**消费目标集，不内嵌选择器**，D4-4 v2）→ 把 `DamageBase` 与 `FormulaParams` 写入上下文（黑板契约键 `Tcs.Flow.Key.BaseDamage` / 公式参数表）→ `RunTemplate` → 恒返回 `TSR_Completed`（流程单帧同步完成，无挂起）；
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
- **THEN** 执行器按官方默认模板（`Tcs.Flow.Template.Default`）起流程（与改造前"空 FName 兜底 `Default`"行为一致）

### Requirement: 流程委托契约（宿主实现）

`TcsDamage` MUST 提供 `ITcsDamageFlowDelegate`（UINTERFACE，宿主实现；**全部函数带中性默认实现**——"普通项目零 delegate"，PV-7/D7-2）：

- **反射面（2026-09-24 新增，台账 SCRIPT-8）**：5 方法 MUST 带 `UFUNCTION(BlueprintNativeEvent)`，且 MUST 在接口类内声明对应的 `virtual <名>_Implementation(...)`——UHT 检测到声明则**不生成默认 stub**（`UhtFunction.cs:681` 的 `ImplFound`），故中性默认实现住接口声明处（同款先例 = 引擎 `ISequencerAnimationOverride`，`SequencerAnimationOverride.h:31-44`；本仓 `ITcsAttributeProvider` 为无默认体的同族先例）；
- **形参 MUST 全反射**：上下文参数类型 MUST 为 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)` 反射只读视图），**MUST NOT** 为 `FTcsDamageFlowContext`——后者是**纯 C++ struct（无 `USTRUCT`）**，出现在 `UFUNCTION` 签名里会让 UHT 报 `Unable to find 'struct' with name ...`（同款既有实证：`UTcsEffectSubsystem::ExecuteChain` 因此无法标记，只能另开 `ExecuteChainForCaster`）；
- **C++ 调用点 MUST 走 `ITcsDamageFlowDelegate::Execute_<名>`**（**MUST NOT** 虚表直调）——虚表直调会**静默跳过**脚本层实现（C# 覆写走 `ProcessEvent`），表现为"公式不生效"而非崩溃。`Execute_` 内部先查 `UFunction` 走反射、查不到才回落原生接口实现（`TcsAttributeProvider.gen.cpp:141-157` 同款生成代码），故**C++ 实现与脚本实现双轨并存**；
- 5 方法签名（换型后）：
  - `double GetBaseHitRate(攻击者句柄, 目标句柄, const FTcsDamageFlowContextView&)`——默认 `1.0`；
  - `double GetBaseCritRate(...)`——默认 `0.0`；
  - `FGameplayTag ResolveElement(...)`——默认空 Tag；
  - `double CalculateBaseDamage(double IncomingBase, ...)`——**降级逃生口**（默认实现 = 返回传入值，即"不改动输入"）；
  - `double ModifyShield(目标句柄, double IncomingDamage, ...)`——默认 `0.0`（无护盾）；
- 契约全部以**实体句柄**为参与者（承接 2026-09-20 句柄化）；插件 MUST NOT 内置任何公式。

#### Scenario: 零 delegate 也能跑

- **WHEN** 链步骤的 `Delegate` 为空（宿主完全不实现）
- **THEN** 默认模板仍能跑通（命中率 1、暴击 0、元素空、基础值不被改写、无护盾）

#### Scenario: 宿主自定义公式

- **WHEN** 宿主实现 `CalculateBaseDamage` 返回自己的公式结果
- **THEN** `BaseDamage` 步骤采用该值（逃生口生效）

#### Scenario: 脚本层实现可被 C++ 调用点抵达

- **WHEN** 宿主脚本层（C#）实现 `CalculateBaseDamage` 并把实现对象配进流程模板的 delegate 字段，随后流程跑到 `FTcsFlowBaseDamage`
- **THEN** 基础伤害值等于脚本实现返回的值（`Execute_` 经 `ProcessEvent` 抵达脚本实现）——**这是"自定义伤害流程可用脚本编写"的判据**

#### Scenario: C++ 实现与脚本实现并存

- **WHEN** 宿主以 C++ 类（非脚本）实现同一接口并配进模板
- **THEN** 行为与换型前一致（`Execute_` 的 `else if` 分支回落原生 `_Implementation`，无需宿主额外适配）

### Requirement: 流程上下文反射视图（`FTcsDamageFlowContextView`）

`TcsDamage` MUST 提供 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)`）——**`FTcsDamageFlowContext` 的可反射数据面投影**，专供宿主脚本层在插槽实现里读取流程状态：

- 字段 MUST 覆盖：参与者句柄（`Attacker` / `Instigator` / `Targets`）、公式参数初值（`FormulaParams`）、分类 Tag 集（`ClassificationTags`）、请求字段（`BaseDamageInput` / `TargetAttrKey`）；
- **MUST NOT 含黑板**（`FTcsFlowAttributes`）——它是**纯运行态容器**：存储形状 `TMap<键, TArray<提交>>` 是**嵌套容器**（UHT 层面不可作 `UPROPERTY`），提交项 `FTcsFlowAttributeSubmit` 又是纯 C++ 记录（**2026-09-30 理由改写**：原理由为"提交项深处嵌 `FTcsConsumePolicy::OnConsumed`（`TFunction<void()>`）⇒ 物理不可反射（`TcsFlowAttributes.h` 自注、UHT 实证）"，**该闭包已随本提案的消耗策略改造删除**，该技术理由不再成立；**禁令本身不变**，理由改为容器形状与运行态记录）；
- **MUST NOT 含宿主门面弱引用**（`Owner`）——脚本层经自身 `GetWorld()` 取门面，无需框架代传（且 `TWeakObjectPtr` 非脚本友好）；
- **MUST NOT 含 `FlowSource` / `CapturedAttrs`**（框架簿记面，宿主脚本无消费场景）；
- **构造方向 = 单向**：由框架从 `FTcsDamageFlowContext` 投影（`MakeView`）；**MUST NOT** 提供反向写回——视图是**只读投影**，不是"另一个可写上下文"（避免双真相）；
- **为什么是视图而非整体反射化**：把 `FTcsDamageFlowContext` 整体反射化需先做**上下文/黑板分层**（把纯运行态面从数据面摘出）——那是台账 SCRIPT-3 的范围。视图只摘可反射面，**成本低一个数量级**，且满足插槽的全部读取需求。**2026-09-30 口径更新**：SCRIPT-3 的原**物理阻碍**（提交项深处的 `OnConsumed` 闭包）已随消耗策略改造消除，其难度由"物理不可能"降为"逐字段反射化的常规工作量"——但**本视图的立项理由不因此消失**（嵌套容器与运行态提交记录仍不可直接反射），故视图保留。

#### Scenario: 视图不含不可反射成员

- **WHEN** 检查 `FTcsDamageFlowContextView` 的 `UPROPERTY` 字段集
- **THEN** 全部字段可反射（无 `TFunction`、无 `TWeakObjectPtr`、无裸 C++ struct）；UHT 编译通过

#### Scenario: 宿主脚本读得到流程状态

- **WHEN** 脚本层在 `CalculateBaseDamage` 实现里读取 `Context.Attacker` / `Context.Targets` / `Context.FormulaParams`
- **THEN** 读到的是本次流程的真实值（非默认值/空集）

#### Scenario: 投影不改动原上下文

- **WHEN** 框架从 `FTcsDamageFlowContext` 构造视图
- **THEN** 原上下文内容不变（视图是拷贝投影，非别名）

### Requirement: 伤害记录与事件

`TcsDamage` MUST 以扁平记录承载伤害结果（09 §2.4）：

- `FTcsDamageRecord`（USTRUCT）：`uint64 FlowId` / `FTcsCombatEntityHandle Source` / `FTcsCombatEntityHandle Target` / `FGameplayTag Element` / `bool bHit` / `bool bCrit` / `double Base` / `double Final` / `double Executed` / `double Absorbed` / `bool bKill` / `uint64 Sequence` / `double Timestamp`；
- **参与者用实体句柄**（**MUST NOT 用 `AActor*`**——承接句柄化）；**字段全扁平**（无 `TMap`/`TFunction`——符合过网结构纪律，将来可直接上 FastArray）；
- `bKill` 的语义 = **记账而非裁定**：本次事务提交后目标 `Health ≤ 0`（死亡规则仍归宿主）；
- `FTcsFlowCompleted` 步骤 MUST 填充记录并经总线**立即通道**发布事件（**原生 Tag** `Tcs.Event.Damage.Recorded`，载荷 = 该记录）；
- `UTcsDamageSubsystem` MUST 持**环形缓冲**（容量常量初值 128）并提供读取口 `GetRecentRecords(TArray<FTcsDamageRecord>& OutRecords) const`（旧→新序）。

#### Scenario: 完成即出记录

- **WHEN** 默认模板跑完
- **THEN** 记录事件在 `RunTemplate` 返回前派发（立即通道）、环形缓冲可读回同一笔记录（字段与本次伤害一致）

#### Scenario: 记录可跨模块消费

- **WHEN** 装置/宿主订阅该原生 Tag
- **THEN** 收到载荷即拿到完整记录（无需访问流程上下文——记录是结果快照）

### Requirement: 流程模板登记面的反射可达性

`UTcsDamageSubsystem` 的模板登记方法 MUST 对宿主脚本层可达（`RegisterTemplate` / `UnregisterTemplate`）：

- 标记 MUST 为 **`UFUNCTION()` 无 specifier**（口径同 `effect-interpreter` 的门面反射面——`FTcsFlowTemplate` 是 `USTRUCT()` 非 `BlueprintType`，加 `BlueprintCallable` 会被 UHT 蓝图参数校验拒绝）；
- 可见性依赖 C++ 侧 `public` 访问级别；实现后 MUST 以生成的 glue 产物验证（读 `*.generated.cs` 确认修饰符为 `public`）；
- 覆盖范围含步骤数据反射入口：`RegisterStepExecutor`（见 `effect-step-dispatch` 的"步骤执行器插槽"）——**脚本层要能"登记自己的步骤执行器"就必须能先"登记含该步骤的模板"**，两者是同一闭环的两半。

#### Scenario: 脚本层可登记流程模板

- **WHEN** 检查 `UTcsDamageSubsystem` 的生成绑定产物
- **THEN** `RegisterTemplate` / `UnregisterTemplate` 出现且修饰符为 `public`

### Requirement: ModifyFlow 链原语（伤害修改器通道提交侧）

`TcsDamage` MUST 提供链原语 `FTcsStepModifyFlow`（住 `Public/Chain/TcsStepModifyFlow.h`）——D7-6"伤害修改器唯一通道"的**提交侧**：触发行为命中后起一条单步链，本步骤从链上下文取回流程上下文，向流程黑板提交一笔修正。

- 字段（全部 `UPROPERTY`，可经链资产序列化编辑）：
  - `FGameplayTag TargetKey`（目标黑板键；**无效 tag = 落契约键 `Tcs.Flow.Key.BaseDamage`**——按 `project.md` 的 `FGameplayTag` 字段默认值陷阱，兜底 MUST 在执行器内完成，MUST NOT 依赖字段默认值）；
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
- **THEN** 该步骤向黑板 `TargetKey`（或未配时的 `Tcs.Flow.Key.BaseDamage`）提交一笔 `{Op, Operand, Consume}`，后续 `Execute` 步骤读到的折叠值包含这笔贡献

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
- **消费行为的表达方式 = 事件语义**（`DEC-04` §3.3 裁定 ④）：MUST NOT 用内联闭包；消费成功时的行为由**消费事件 + 项目经触发行订阅**表达——可序列化、可反射、可脚本可达，且复用项目既有的订阅通道，零新机制。
- 消费事件 MUST 为插件原生声明的 `FGameplayTag`（`Tcs.Event.Damage.<事件名>`，公约同 `project.md`），**MUST NOT** 由项目 ini 声明（事件 tag 属框架词汇；项目漏配会导致事件静默丢失）。
- **本需求只定形状，不实现消费动作**：`FTcsFlowExecute` "成功才消费"（发事件 / 标记已消费 / 扣 `MaxUses` / 起冷却）归台账 `DAMAGE-4`（R5/M4a），与本形状同批落地时不得再改结构。

#### Scenario: 结构可作 UPROPERTY

- **WHEN** 某配置 struct（链步骤或数据步骤）以 `UPROPERTY` 持有 `FTcsDamageModifierConsumePolicy`
- **THEN** UHT 编译通过（改造前必定报错）

#### Scenario: 消费行为经事件表达

- **WHEN** 项目希望"某修改器被消费时执行一段自定义逻辑"
- **THEN** 项目订阅消费事件并由触发行起链表达该逻辑——无需插件提供回调字段、无需写 C++

#### Scenario: 拆掉闭包后上下文反射化的阻碍消失

- **WHEN** 评估"把 `FTcsDamageFlowContext` 逐字段反射化"（台账 `SCRIPT-3`）
- **THEN** 其递归链上的 `TFunction` 已不存在（本改造前，`FTcsFlowAttributes → FTcsFlowAttributeSubmit → FTcsConsumePolicy::OnConsumed` 是唯一根因——该类型现已改名 `FTcsDamageModifierConsumePolicy` 且该成员已删除）

### Requirement: 收集事件载荷读取器登记

`TcsDamage` MUST 为自己发布的流程收集事件载荷（`FTcsDamageFlowCollectEvent`）登记**载荷读取器**——这是台账登记册 `LEDGER-reflection` R-2 调研发现的**功能未做**（`UE_DEFINE_TRIGGER_PAYLOAD_READER` 宏全库只有定义、零登记），用户 2026-09-27 裁定**并入本批**（与反射无关，属领域侧该做未做）。

- 登记 MUST 走**静态自注册宏**（`UE_DEFINE_TRIGGER_PAYLOAD_READER`，住 TcsDamage 模块）——属主模块登记自己领域载荷的读取器，MUST NOT 依赖宿主或运行期动态入口；
- 读取器 MUST 从 `FTcsDamageFlowCollectEvent::Context` 映射 `FTcsTriggerPayloadInfo`：
  - `Caster` ← `FTcsDamageFlowContext::Attacker`（**攻击方**；"谁发起的"语义在触发行侧由 `Instigator` 承载，不在此）；
  - `ClassificationTags` ← `FTcsDamageFlowContext::ClassificationTags`（直通）；
- 载荷缺失或 `Context` 为空指针 → MUST 返回**默认构造**的 `FTcsTriggerPayloadInfo`（空句柄 + 空标签集），MUST NOT 崩溃、MUST NOT 静默编造主体；
- 读取语义 MUST 保住"**每事件一次**"（求值器已在各行之间共用一次读取）——本登记 MUST NOT 把读取点挪进逐候选行循环；
- **本需求是"登记缺失"的补齐，不是新机制**：注册表本体与寿命语义已由 `effect-trigger` 能力规定，本需求只规定"TcsDamage 该登记而没登记"这一件事。

#### Scenario: 收集事件的主体可被触发行读到

- **WHEN** 某触发行订阅 `Tcs.Event.Damage.AfterDamage`（或任一收集事件），流程走到该步骤发布事件
- **THEN** 求值器经本读取器拿到 `Caster` = 本流程的 `Attacker`、`ClassificationTags` = 流程上下文的分类标签集；以 `Caster` 为施法者的触发行能正常起链（不再因"无读取器"而拿到空句柄）

#### Scenario: 空载荷安全降级

- **WHEN** 载荷的 `Context` 为空指针（或缺读取器时的降级路径）
- **THEN** 读取器返回默认构造结果（空句柄 / 空标签集），不崩溃、不误判主体
