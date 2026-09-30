# Change: ModifyFlow 链原语（伤害修改器通道的提交侧）

## Why

R4 的核心承诺是**伤害修改器唯一通道**（D7-6）：状态/装备/被动不改属性、不改流程，而是"**订阅流程收集事件 → 命中 → 起一条单步链向黑板提交修正**"。

Task 1/2 已交付订阅与求值侧（触发行 + 条件注册表 + 登记表），但**提交侧还没有原语**——触发行命中后起的链里无法表达"往黑板提交一笔修正"。本提案补上这一环，使"一条修改器 = 触发行 + 单步链"的创作糖（09 §2.3）成立。

同时本提案修掉一处**会直接导致编译失败**的既有障碍：`FTcsConsumePolicy` 含 `TFunction<void()> OnConsumed`，使任何**配置面**（`UPROPERTY`）都无法携带消耗策略——而"破甲只能用 3 次"这类需求恰恰要求它可配。这不是新问题（`TcsFlowDataSteps.h:44` 已自行记录该限制），但 Task 3 是第一个必须正面解决的消费者。

## What Changes

- **`damage-primitive` MODIFIED**：新增 `FTcsStepModifyFlow` 链原语需求——字段（`TargetKey` / `Op` / `Operand` / `Consume`）、执行器行为（从 `Context.EventPayload` 解出流程上下文 → 向黑板 `Submit`）、跨模块自注册、依赖方向不破（`TcsEffect` 全程不认识 TcsDamage 类型）。
- **`damage-primitive` MODIFIED**：新增"消耗策略为纯数据可反射结构"需求——`FTcsConsumePolicy` **改名 `FTcsDamageModifierConsumePolicy`**（用户 2026-09-30 裁定）并去掉 `TFunction`，`OnConsumed` 改**具名事件语义**（消费时发事件，行为由项目经触发行订阅表达）。此改动同时解除三项既有阻塞：配置面可携带、结构可反射、将来可过网。
- **`damage-primitive` MODIFIED**：新增"收集事件载荷读取器登记"需求——补上 `UE_DEFINE_TRIGGER_PAYLOAD_READER` 的**零登记**欠账（TcsDamage 该给自己的 `FTcsDamageFlowCollectEvent` 登记读取器）。**用户 2026-09-27 裁定并入 Task 3**（`LEDGER-reflection` R-2 调研结论 ②：属"功能未做"，与反射无关）。
- **`damage-flow` MODIFIED**：流程属性黑板需求同步改名并删除 `OnConsumed` 回调位描述（该规格 `:61` 明文列着"`OnConsumed` 回调位'），否则规格与新结构矛盾。顺带校正一处**规格与源码不符**：该规格 `:58` 把 `SortKey` 写成 `Submit` 的独立形参（`Submit(Key, Op, Operand, SortKey, ConsumePolicy)`），而源码签名为 `Submit(Key, Op, Operand, Consume)`（`SortKey` 是消耗策略的字段）——本批以源码为准校正。
- **`damage-primitive` MODIFIED × 1（2026-09-30 补，第四份 delta）**：既有需求「流程上下文反射视图」的**过期理由**改写——该需求原文写"MUST NOT 含黑板，因为提交项深处嵌 `FTcsConsumePolicy::OnConsumed`（`TFunction`）⇒ **物理不可反射**"，而该技术事实**随本提案的消耗策略改造消失**。**禁令不变**（视图仍不含黑板），理由改为**嵌套容器（`TMap<键, TArray<提交>>` 不可作 `UPROPERTY`）+ 纯 C++ 提交记录**；同时把"真前置 = 上下文/黑板分层、成本低一个数量级"一句更新为 SCRIPT-3 已降级（"物理不可能 → 常规工作量"）的口径。**为什么必须本批做**：`openspec archive` 会把 delta 合入生效规格——不补则生效规格里留下一句**已被证伪**的论断（同类修正本提案已给 `damage-step-library` 做过一次）。
- **不做**（保持本批边界）：
  - 不做 `MaxUses` / `Cooldown` 的**实际消费落地**（"扣次数、起冷却、标记已消费"）——那是台账 `DAMAGE-4`，归 R5/M4a，与 `ModifyFlow` 同批但不属本提案；
  - 不做 `FTcsFlowExecute` 的 `BestIndex` 消费动作（本提案只让**策略可配**，不让**消费发生**）；
  - 不做消费事件的**发布**（本批只声明 tag 与形状）；
  - 不做 `Heal` / `ModifyFlow` 之外的链原语（4 个原语是 Task 3.5）；
  - 不做目标集装填（台账 `DAMAGE-1`，R4 的 `ModifyFlow` 消费者不需要目标）。

## Impact

- **Affected specs**：
  - `damage-primitive`（**ADDED × 3**：`ModifyFlow` 链原语 / 消耗策略为纯数据可反射结构 / 收集事件载荷读取器登记；**MODIFIED × 1**：流程上下文反射视图——2026-09-30 补第四份 delta，把该需求里"提交项深处嵌 `OnConsumed` ⇒ 物理不可反射"的过期理由改写为"嵌套容器 + 运行态记录"）
  - `damage-flow`（MODIFIED：流程属性黑板——同步改名、删 `OnConsumed` 回调位、校正 `Submit` 签名）
  - `damage-step-library`（**需一并 MODIFIED**：其"通用数据步骤"需求明文写着"`FTcsFlowModify` MUST NOT 携带消耗策略，理由是 `FTcsConsumePolicy` 含不可作 `UPROPERTY` 的 `TFunction`"——该理由随本次改造**失效**。本次**不改变该条禁令本身**（数据步骤仍不带消耗策略，理由改为"数据步骤只做纯数值写入"），但必须把过期的理由句替换掉，否则规格里留下一句与技术现实矛盾的话）
- **Affected code**（`Source/TcsDamage/`）：
  - 新增：`Public/Chain/TcsStepModifyFlow.h` + `Private/Chain/TcsStepModifyFlow.cpp`、载荷读取器登记（住 TcsDamage，随模块自注册）
  - 修改：`Public/Flow/TcsFlowAttributes.h`（改名 + 去 `TFunction`）、`Private/Flow/TcsFlowAttributes.cpp`（`Submit` 形参类型跟改）、`Public/Flow/TcsFlowDataSteps.h`（注释同步）、`Public/Flow/TcsDamageFlowContextView.h`（注释里的旧名同步）、`Private/TcsDamageSubsystem.cpp`（可选：消费事件 tag 声明）
  - **零改动面**：`TcsEffect`（跨模块自注册的既定形态）、`TcsTargeting`、`FTcsDamageFlowContext` 的形状（只被读取）
- **跨模块判据**：Task 1/2 已建立的"TcsEffect 不具名领域类型"纪律继续成立——`FTcsStepModifyFlow` 的执行器住 TcsDamage，经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自注册；流程上下文经 `FTcsEffectContext::EventPayload`（`FInstancedStruct`，已就位字段）抵达。

## 关键决策点：`FTcsDamageModifierConsumePolicy::OnConsumed` 的改造形态 —— **已拍板（方案 A）**

**问题**：`TFunction<void()> OnConsumed` 使消耗策略无法出现在任何 `UPROPERTY` 上（UHT 编译错误，`UhtSession.cs:2645`）。而 `FTcsStepModifyFlow` 是链资产里的配置 struct，字段必须可反射/可序列化。

**三个候选**：

| 候选 | 做法 | 判定 |
|---|---|---|
| **A 事件语义**（`DEC-04` §3.3 建议） | 去掉 `OnConsumed`；消费成功时发布**事件**（载荷含提交键 / 来源）；"被消费时要做什么"由项目**经触发行订阅**表达 | ✅ **采纳（用户 2026-09-30 拍板）** |
| B 拆两层（数据面 + 行为面） | 数据面只留 `{MaxUses, Cooldown, SortKey}`；另在提交上留 `TScriptInterface<ITcsConsumeBehavior>` 行为挂点 | ❌ 未采纳——能表达"这条提交专属行为"，但引入对象寿命语义（`reflection-backlog` 花了整个 R4.5-a 才修掉同类缺陷），且与"行为归配置/事件"的既有体系不一致 |
| C 原语语义 | `OnConsumed` 换成"消费时执行某条链"（`FGameplayTag ChainId`） | ❌ 未采纳——把"消费"与"起链"焊死；项目可能只想记个数、发个表现 |

**采纳 A 的连带收益**（`DEC-04` 已量化）：`OnConsumed` 是"`FTcsDamageFlowContext` 物理不可反射化"的**唯一根因**，也是台账 `SCRIPT-3` 成本被评"高一个数量级"的原因。改掉它，`SCRIPT-3` 从"物理不可能"变成"逐字段反射化"的常规工作量。

**★ 类型改名（用户 2026-09-30 裁定）**：`FTcsConsumePolicy` → **`FTcsDamageModifierConsumePolicy`**。理由——"Consume" 单独出现语义不清晰；`DamageModifier` 指名"**伤害修改器**"这一提交者身份。**关键澄清**：此处 `Modifier` **不是**指走运算带聚合的**属性**修正器（`FTcsAttrModInstance`/`UTcsAttrModDef`，它们没有消耗语义），而是 09 §2.3 的"伤害修改器通道"提交者——因此该词根在此**歧义更小**，与 `FTcsStepModifyFlow`（住 `Category = "Tcs|Damage|Chain"`）同域一致。
字段名保持短的 `Consume`（类型名全称、字段名从简，本仓既有做法）。

**时机裁定（用户 2026-09-30）**：**形状提前到本批（Task 3）**——不改编不过；**动作**（扣次数 / 起冷却 / 标记已消费）仍归台账 `DAMAGE-4`（R5/M4a）。`LEDGER-reflection` R-6 的登记据此改为"**形状已定、动作待 `DAMAGE-4`**"（原登记为"形状待 DAMAGE-4 落定时定"——该口径因本批而作废）。

**本提案只改形状、不落消费动作**：本批交付"**策略可配 + 事件形状就位**"，让 R5 落地消费时不必再改结构。

## 提案内钉名（计划/设计未钉或需收窄）

| 项 | 钉名 | 依据 |
|---|---|---|
| 链步骤类型 | `FTcsStepModifyFlow`（住 `Public/Chain/`） | 与 `FTcsStepDamage` 同族（`damage-primitive` 既有需求） |
| 消耗策略类型 | **`FTcsDamageModifierConsumePolicy`**（原 `FTcsConsumePolicy`） | 用户 2026-09-30 裁定（语义直观化）；正文 §关键决策点有完整理由 |
| 消费事件 tag | **待定**——建议 `Tcs.Event.Damage.SubmitConsumed`（或 `...Consumed`） | `project.md` 事件命名公约 `Tcs.Event.<域>.<事件名>`；本提案**只在规格里留契约**，不实现发布（DAMAGE-4 才发布） |
| 空 `TargetKey` 兜底 | 落契约键 `Tcs.Flow.Key.BaseDamage` | 与 `FTcsStepModifyFlow` 的执行器纪律一致（`project.md` 的 `FGameplayTag` 字段默认值陷阱：字段默认值不能是 tag，MUST 在执行器内兜底） |
| 改造后字段 | `{ int32 MaxUses; double Cooldown; int32 SortKey; }`（**纯数据**）；字段名保持 `Consume` | 三字段在案均有语义（`SortKey` 已有一个消费者：`TcsFlowStepsCore.cpp:141`）；"类型全称 + 字段从简"是本仓既有做法 |
| 载荷读取器的 `Caster` 取值 | `Caster ← FTcsDamageFlowContext::Attacker`（**攻击方**）；`ClassificationTags` 直通 | 收集协议里"谁发起的"由触发行侧 `Instigator` 承载，读取器的 `Caster` 应给**攻击方**（业务语义：这次伤害是谁打的） |

## 检查点（人工验证点，用户执行）

1. **编译**：Development **与 Shipping** 双配置零警告；`FTcsStepModifyFlow` 出现在链步骤 picker 中且四个字段可配（含 `Operand` 的 `FTcsParamValue` 内嵌编辑）。
2. **注册可达**：TcsDamage 模块加载后，TcsEffect 的执行器注册表能查到 `FTcsStepModifyFlow`（跨模块自注册生效、`TcsEffect` 零改动）。
3. **端到端**（Task 4 主体验收的一部分）：触发行命中 → 单步链 → 黑板多出一笔提交 → 后续 `Damage` 步骤读到修正后的值（"破甲"现场）。
4. **配置面实证**：链资产里配 `Consume.MaxUses = 3` 能保存并加载（本次改造解除的直接阻塞）。
5. **载荷读取器实证**：订阅 `Tcs.Event.Damage.AfterDamage` 的触发行，其 `Caster` 解析为本次流程的 `Attacker`（改造前读到空句柄）。
6. **依赖面**：`grep "^#include" Source/TcsEffect/` 零 TcsDamage 类型；`TcsEffect` 侧本批零改动。
