## ADDED Requirements

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

## MODIFIED Requirements

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
