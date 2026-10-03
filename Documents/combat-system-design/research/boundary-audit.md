# 框架本体边界审计（TCS 插件）

- **文档 ID**：`RSCH-boundary-audit`
- **类型**：调研与证据（框架本体边界审计；含违规清单 / 不许删名单 / 判据异议）
- **状态**：冻结（`FROZEN`，2026-10-01 定稿）
- **权威范围**：本文独有的是 **`Source/` 七模块的边界判据应用结果**——违规清单（含三分处置）、"合规但极易被误判"不许删名单、真缺陷、以及判据自身的异议与适用边界。**不在本文**：tag 词表与根段注册表（见 `openspec/specs/gameplay-tag-governance/spec.md`）、具体整肃动作的分工与排期（归「伤害模块边界整肃」提案）。
- **最后更新**：2026-10-01

> **换根注记（2026-10-01）**：本文件记录的 tag 名保留**当时原样**（历史现场，MUST NOT 改写）。这些旧名已于 2026-10-01 由提案 `reroot-gameplay-tag-vocabulary` 换根，映射 = `Tcs.Event.*`→`TcsEvent.*`、`Tcs.Flow.Key.*`→`DamageFlowKey.*`、`Tcs.Flow.Template.*`→`DamageFlowTemplate.*`、`Tcs.Attr.*`→`Attribute.*`、`Tcs.Chain.*`→`EffectChain.*`（另 16 个 `Probe` 验证词整批退役删除）。新名以 [SPEC-00-core](../spec/01-module-m0-core.md) 与提案规格为准。

---

## 1. 为什么有这份报告

**它是重导，不是誊写。** 上一轮同主题审计的原文在落盘前丢失（不在仓内、不在记忆库，只剩摘要），故本轮**从源码重导**。行号对应当日代码快照；本次审计**未改动任何源码**，故快照与工作区一致。

**与上一轮的三处实质差异**（重导的价值所在）：

| 项 | 上一轮口径 | 本轮重导 | 依据 |
|---|---|---|---|
| 事件词数量 | 11 个（其中 4 个玩法词） | **9 个**（TcsDamage）+ 1 个（TcsAttribute）；**无一引用宿主词汇** | 全库 `UE_DEFINE_GAMEPLAY_TAG` 清点为 21 处（TcsDamage 9 事件 + 1 模板 + 9 键，TcsAttribute 1 事件），TcsCore 零处 |
| 黑板键 | 9 个，其中 6 个玩法词 | 9 个成立；**"6 个玩法词"不成立**，按"是否宿主玩法概念"最多 2–3 个 | `Public/Flow/TcsFlowKeys.h:29-37` 逐条判；键是**步骤之间的接口** |
| 实质越界数 | 8 处 | **"8 处"虚高**；扎实的收敛为 **3 处越界 + 1 处契约覆盖** | 见 §6 的回溯应用 |

**产出要求**（`MEM-20261001-17`）：任何"找违规"的审计 MUST 同时给出 ① 合规但极易被误判的项（附论证）、② 判定规则的适用边界、③ 审计者对自己判据的异议。**只能用于"删东西"的审计报告不合格**——本文 §5 的 32 项就是"不许删名单"。

---

## 2. 判据（本次审计的分析框架）

1. **主判据**：**凡是需要宿主词汇的内置策略，插件一律不提供。** 检查点 = 该策略/字段**引用了什么词汇**：只引用框架自身契约（如"实体有位置"）⇒ 可内置；引用宿主属性名 / Tag 词表 / 阵营关系 / 元素表 / 职业概念 ⇒ 归宿主。
2. **分层判据**：**形状层**（结构 / 协议 / 时序 / 身份机制）属框架；**内容层**（具体阶段构成 / 元素表 / 伤害公式 / 职业概念）归宿主。**"提供一个可被宿主整体替换的默认模板"本身是形状层。**
3. **字段粒度检查点**：该字段取值是否**必然**是宿主本体论，或框架自下的玩法结论？
4. **判据优先级**：**取值归属 > 有无消费者**。"零消费者"不是判违规的充分理由；"有消费者"也不能证明归框架。
5. **第三态「契约默认」**：框架提供默认值，只引用框架自身契约，且**可被宿主覆盖** ⇒ 合规。

### 2.1 对第 5 条的澄清（本轮新增裁定，**本节是全文最要紧的一条**）

上一轮定义第三态时把两个条件并列写了，但**没有说明缺一不可**。本轮实测暴露出这个空隙：存在一种默认值，**只引用框架自身契约**（满足条件 a），却**无法被宿主覆盖**（不满足条件 b）。它不是契约默认，也不是"引用宿主词汇的策略"。

⇒ 裁定：**第三态 MUST 同时满足两条腿**——

| 腿 | 内容 | 缺失后的名字 |
|---|---|---|
| a | 只引用框架自身契约 | 缺 a ⇒ **策略越界**（§3 的 ①/③） |
| b | **运行期可被宿主覆盖** | 缺 b ⇒ **框架内定**（不可替换的框架模型） |

**判据**："该默认值能不能被宿主换掉？覆盖点在哪一行 API？"——答不出来就不是契约默认。本报告 §5 的 32 项**逐项验证过覆盖点存在**。

**推论（比缺 b 更重的一档）**：若某框架默认值**覆盖了宿主已有的配置点**（宿主本来能配，被框架硬编码抢先钉死），那不是"不给扩展面"，而是**契约被架空**。本轮发现一例，见 §3 的 S9。

### 2.2 内容层豁免的精确边界（本轮新增裁定）

上一轮审计的**异议 #1** 提出"内容层不算越界"，硬证据是 `Source/TcsDamage/Private/TcsDamageSubsystem.cpp:50` 的原文（"Hit/Crit/Element/…已就位，**本模板不组装**"）。该异议**方向正确但停在半路**：它没走完最后一步——这条豁免**同时给出了"哪里仍然违规"的判据**：

> **豁免只覆盖"默认模板组装了哪些步骤"，不覆盖"已被组装进默认模板的步骤，其执行器内部的硬编码"。**

这条限定让豁免**可判定**而不是万能挡箭牌：

- `Hit` / `Crit` / `Element` / `PreHit` / `AfterDamage` / `PreExecute` 步**不在**默认模板（`TcsDamageSubsystem.cpp:51-56` 只组装 4 步）⇒ 它们的阈值与步骤内有豁免；
- `Execute` / `Completed` 步**在**默认模板 ⇒ 它们的执行器内部**无豁免**（S1a / S1d / S2 / S9 全部落在 `TcsFlowStepsCore.cpp` 的这两个函数里）。

---

## 3. 违规清单（按三分处置重排）

> 上一轮把 S1 **打包**成一类，实际混了**三类处置完全不同**的东西。本节按处置重排，每条标注 ①硬编码常量字段 / ②接缝但名字是宿主概念 / ③框架自下的模型结论 / ④契约默认 / ⑤契约覆盖。
> 窗口期提示：`S1b`（`Element` 字段）的**处置动作已移交**独立提案「伤害模块边界整肃」（`openspec/changes/reroot-gameplay-tag-vocabulary/proposal.md`「明确不做」），本节只留证据。

### 3.1 ① 硬编码常量字段（真死字段，2 处）

#### S1a `FTcsDamageRecord::bHit` 恒为 true

- **落点**：`Source/TcsDamage/Private/Flow/Steps/TcsFlowStepsCore.cpp:241`（`Record.bHit = true;`）；字段声明 `Source/TcsDamage/Public/Flow/TcsDamageRecord.h:66-67`（默认 `false`）
- **事实**：唯一赋值点是硬编码 `true`。写 `Hit` 键的 `FTcsFlowHit` 步不在默认模板里 ⇒ `Hit` 键永不被写、`bHit` 永不被读。字段取值恒定、零信息量。
- **违反判据**：判据 1（框架替宿主断言"本次一定命中"）+ 判据 3。
- **处置**：**不删字段**——`Hit` 键与 `FTcsFlowHit` 步是合法接缝（见 §5 的 G5）。改为与同函数 `bCrit` 对齐的读法：从契约键 `DamageFlowKey.Hit` 读回。

#### S1b `FTcsDamageRecord::Element` 唯一写入点是置空

- **落点**：`TcsFlowStepsCore.cpp:240`（`Record.Element = FGameplayTag();`）；字段声明 `TcsDamageRecord.h:54-56`
- **事实**：全库唯一赋值点是**置空**。`FTcsFlowElement` 步把 delegate 解析结果写进的是 `Context.ClassificationTags`（`TcsFlowStepsRest.cpp:139`），**不是这个字段**——解析结果与记录字段之间根本没有接线。
- **违反判据**：判据 1（框架断言"本次无元素"）+ 判据 3。
- **处置**：**移交**「伤害模块边界整肃」。两条路（接线 / 删字段）都不在本报告拍板；判据 4 提示：它违规**不是因为零消费者**，而是因为**取值被框架钉死**。

### 3.2 ③ 框架自下的模型结论（2 处，其中 1 处为弱档）

#### S1d `bKill` 的 `<= 0.0` 裁定阈值

- **落点**：`TcsFlowStepsCore.cpp:202`（`if (AttributeSubsystem->EvaluateCurrent(Target, ResolvedAttrKey) <= 0.0)`）；注释 `:194` 自称"记账语义……死亡规则仍归宿主"
- **事实**：框架把"什么算死"钉成 `≤ 0.0`。注释自称记账而非裁定，但该阈值**就是**一次裁定。
- **违反判据**：判据 1 + 判据 2 内容层（死亡规则属内容层）。
- **强度**：**弱档**——框架只写 `Kill` 记账标志、**不杀实体**，且该语义在规格里已明示归宿主（`openspec/specs/damage-primitive/spec.md`「伤害记录与事件」）。故它更接近"框架给了一个记账默认"。
- **处置**：保留记账，但**把阈值提为可覆盖的契约默认**（否则仍是 §2.1 意义上"缺 b 腿"的框架内定）。

#### S2 `Executed = max(0, Candidate - Absorbed)` —— **两轮审计在此直接对撞**

- **落点**：`TcsFlowStepsCore.cpp:162`
- **对撞**：审计 A 判 **③ 框架自下的模型结论**（宿主只能供吸收量，不能换吸收→执行量的映射）；审计 B 判 **④ 契约默认**（`Max(0,·)` 只引用数学，且框架"没写上界"说明只在数学未定义处兜底）。
- **本报告裁定：两者各对一半，须把表达式拆开判**：

| 子部分 | 判定 | 理由 |
|---|---|---|
| `FMath::Max(0.0, ·)` 的**下限钳** | **④ 契约默认** | 收敛到"无操作"（不产生治疗、不产生负数），方向保守，只引用数值 |
| `Candidate - Absorbed` 的**减法模型** | **③（弱档）** | 缺 §2.1 的 b 腿——宿主无法把"吸收"换成按比例分摊 / 带溢出上限等任何别的模型 |

- **关键澄清**：减法模型**不违反主判据**——它不引用任何宿主词汇，纯算术。故它不是"策略越界"，而是"**框架内定**（不可替换）"。**这个区分决定了处置**：不是删掉它，而是给它一个覆盖点。
- **处置**：新增一个带中性默认实现的委托方法（默认实现即当前减法），把它从"框架内定"升格为"契约默认"。**这是契约默认与框架内定的分界操作。**

### 3.3 ⑤ 契约覆盖（本轮新命名的一档，1 处 —— **本轮最强的发现**）

#### S9 `Execute` 步自设零下限，架空宿主的值域配置点

- **落点**：`TcsFlowStepsCore.cpp:179`（`const double NewValue = FMath::Max(0.0, Current - Executed);`）
- **核实过的对照证据**（本轮独立复核，两处源码）：

| 事实 | 落点 | 原文 |
|---|---|---|
| 属性层**只在宿主配了边界时**才钳制 | `Source/TcsAttribute/Private/Attribute/TcsAttributePipeline.cpp:298-308` | `case ETcsAttributeValueDomain::AVD_Clamp:` → `if (bHasMin && Value < MinValue) { Value = MinValue; }` / `if (bHasMax && Value > MaxValue) { … }` |
| 边界模式默认是"**无边界**" | `Source/TcsAttribute/Public/Attribute/TcsAttributeBounds.h:21` | `ABM_None = 0`：`DisplayName = "无边界"`，`ToolTip = "该侧不设边界（默认，值 0）"` |
| `Execute` 步**自行**钉了 0 下限 | `TcsFlowStepsCore.cpp:179` | 在 `SetBaseValue` **之前**钳制，绕过属性层的值域收口 |

- **事实**：宿主若不配 Min，属性层**允许**值降到 0 以下（"过量伤害记负数"是合法配置）。但 `Execute` 步在这一步之前硬钉了 0 下限，**宿主无法表达负值语义**。
- **违反判据**：判据 1（框架替宿主定了数值模型）+ 判据 2 内容层；**且它比 S2 更重**——S2 是"没有扩展面"，S9 是**框架抢在宿主已有的配置点之前下了结论**，即 §2.1 的"契约被架空"。
- **为什么它最强**：修复成本极低且**无新增 API**——删掉该 `FMath::Max`，下限交回属性层的宿主配置。**一行删除即可把这一档消灭**。
- **处置**：删除该钳制（或改为可配）；MUST NOT 保留"框架与宿主各有一套值域结论"的双真相。
- **诚实标注**：本项为审计 A 独有发现（审计 B 未识别它与属性层值域机制的冲突），**该冲突已由本报告独立复核坐实**（上表三行）。

### 3.4 ② 接缝但名字是宿主概念（**不删**，处置 = 改名/补注释）

| 编号 | 落点 | 事实 | 处置 |
|---|---|---|---|
| S1c | `TcsFlowStepsCore.cpp:242` / `:247`（`bCrit` / `Absorbed` 从契约键读回） | 取值由宿主钩子决定（`GetBaseCritRate` / `ModifyShield`）⇒ 满足判据 4 的取值归属，**明确不算越界** | **不删**；仅补注释说明"写入者 `Crit` 步不在默认模板，故默认恒 false" |
| S5 | `Public/Flow/TcsFlowKeys.h:29-37` 的 `Hit` / `Crit` / `Kill` 三键 | 键是**步骤之间的接口**（框架词汇），但承载宿主判定语义 | **不删**；仅在文档登记"这三键承载宿主判定语义" |

> **S4（事件词）与 S5（黑板键整体）已降级**，不再计入违规——回溯应用见 §6。

---

## 4. 真缺陷（非边界问题）

### R-1 `UTcsPieEntityQuery` 是唯一映射登记点，但契约允许宿主整体替换 ⇒ **静默失效**

- **落点**：`Source/TcsIntegration/Private/Entity/TcsCombatEntityComponent.cpp:16-33`（`ResolveEntityQuery` 内 `return Cast<UTcsPieEntityQuery>(Query);`）；调用点 `:82`（`BeginPlay` 登记）与 `:96`（`EndPlay` 注销）；契约侧 `Source/TcsEffect/Public/Host/TcsEntityQuery.h:23-24`（"遍历、定位、存活都是宿主本体论，由宿主/上层经本接口注入"）
- **事实**：`Cast` 是硬耦合。宿主按文档建议**换成独立实现类**后 `Cast` 返回 `nullptr`，`BeginPlay` 静默跳过 `RegisterEntity`（**无 else 分支、无日志**）：组件仍拿到句柄并自认已注册，但句柄**永远进不了任何"句柄↔Actor"映射** ⇒ `GetLocation` 恒 false、`EnumerateEntities` 永不吐它、`IsAlive` 恒 false。
- **为什么是缺陷**：**契约（可自由替换）与实现（只有一种实现能被登记）自相矛盾**，且失效形态**静默**（不崩溃、无日志，表现为"范围选择打不中任何人"）。词汇完全合规——判据 4 不适用。
- **影响面**：范围选择、依赖 `EnumerateEntities` 的宿主选敌、所有走 `IsAlive` 的存活核对。**子类化** `UTcsPieEntityQuery` 尚可（`Cast` 到基类成功），**换实现类**则全断；文档同时给出"覆写本类**或换实现**"两条建议，只有前者安全。
- **建议**：把"句柄↔Actor 映射"抽为**中立容器**（挂在 `UTcsEffectSubsystem` 或 `TcsIntegration`），`UTcsPieEntityQuery` 只作其消费者；或把 `RegisterEntity` / `UnregisterEntity` 提升到 `ITcsEntityQuery` 契约面。**至少**：`Cast` 失败 MUST Warning，不得静默。

### R-2 `damage-primitive` 规格与实现失步（写黑板 vs 只写上下文）

- **落点**：规格 `openspec/specs/damage-primitive/spec.md:15`（声称执行器"把 `DamageBase` 与 `FormulaParams` 写入上下文（黑板契约键 `Tcs.Flow.Key.BaseDamage` / 公式参数表）"）；实现 `Source/TcsDamage/Private/Chain/TcsStepDamage.cpp:53-58`（只写 `FlowContext.BaseDamageInput` / `FlowContext.TargetAttrKey`，**从不写黑板**）
- **事实**：写黑板的是后续 `FTcsFlowBaseDamage` 步（`TcsFlowStepsCore.cpp:115-116`）。实现在 `:94-96` 给出了**有实测支撑**的反驳理由：黑板会被 `CollectStart` 重置，若从黑板读输入，"链侧预写 + 本步提交"会造成**重复计数（实测 10 + 20 = 30）**。
- **为什么是缺陷**：规格是唯一真相源，此处规格描述的是**已被证伪的实现** ⇒ 下一个照规格改代码的人会把实测修好的重复计数 bug 重新引入。
- **建议**：**改规格，不改实现**——实现的实测理由比规格更硬。改写为"写入上下文请求字段 `BaseDamageInput`，黑板写入归 `FTcsFlowBaseDamage` 步"，并把重复计数的实测理由记进规格。

### R-3 键语义最敏感的文件里留着 `FName` 时代的死常量

- **落点**：`Source/TcsDamage/Private/Chain/TcsStepDamage.cpp:17-18`（`const FName TcsChainDamage_BaseKey(TEXT("BaseDamage"));`）
- **事实**：**零使用**（全库 grep 仅此一处命中）。是 2026-09-22 `FName` → `FGameplayTag` 改造的残留；住在匿名 namespace ⇒ 编译器不报 unused。
- **为什么是缺陷**：留着诱人的 `FName("BaseDamage")` 字面量在键语义最关键的文件里，下一个人很容易据它改成 `FGameplayTag(TEXT("BaseDamage"))` 裸字面量——**正是 `openspec/specs/damage-step-library/spec.md:16` 明文禁止的形态**。同规格 `:16` 还点名要求清理同类死常量（`TcsFlowRestKey_HitRate` / `TcsFlowRestKey_CritRate`，已登记在 `openspec/changes/reroot-gameplay-tag-vocabulary/tasks.md` 第 2.6 项）。
- **建议**：删除该常量。

### 附：一处规格内在张力（**不判为缺陷**）

`openspec/specs/damage-step-library/spec.md:66` 对"宿主能否覆盖 `Default` 模板"给出自我否定的行文（先问"MUST NOT 拒绝宿主覆盖 `Default`？"再答"**否**"），实现（`TcsDamageSubsystem.cpp:123-128` 重复登记拒绝）与最终答案一致。故不算缺陷，但 `TcsDamageSubsystem.cpp:48` 与 `TcsDamageSubsystem.h:37` 的注释措辞"**可整表替换**"偏乐观（同 id 不可覆盖）——建议改为"可整表替换**为宿主自建模板**"。

---

## 5. 「合规但极易被误判为违规」清单（**不许删名单**，32 项）

> **用途**：后续清理 MUST NOT 按"框架零默认"删除本节各项。每项均附"为何是接缝 / 契约默认"的论证与已核实的宿主覆盖点。
> 行号口径：本节行号为当日快照；源码改动后以"为何合规"栏的符号名为锚。

| # | 项 | 落点（符号） | 为何是接缝 / 契约默认 | 误删后果 |
|---|---|---|---|---|
| G1 | `FTcsSelSelf`（内置"选自己"） | `TcsTargeting/Public/Targeting/TcsSelSelf.h` | 只引用 `Context.Caster` + 句柄有效性，零宿主词汇；覆盖点 = `Selector` 槽（`FInstancedStruct`） | 存量链资产**静默变空**（`FInstancedStruct` 存类型身份，删类型即空） |
| G2 | 选择器基类 `Resolve` 空实现 | `TcsTargetSelectorStrategy.h` | 空语句 ⇒ 词汇集为空；`meta=(Hidden)` 禁选；禁纯虚是引擎约束 | 改 `=0` ⇒ C2259 或 Dev/Shipping 行为分裂 |
| G3 | 过滤器基类 `Pass` 返回 `true` | `TcsTargetFilterStrategy.h` | `true` 是"无判定"编码；改 `false` 是**下了淘汰结论** | 漏配 Filter 静默变成"打不到人" |
| G4 | `FTcsFilterHostDelegate` 未配置 ⇒ 通过 | `TcsTargeting/Private/Targeting/TcsFilterHostDelegate.cpp` | 与 G3 同口径；连 `Candidate` 都没读 | 与 G3 形成双真相 |
| G5 | `FTcsSelHostDelegate` 未配置 ⇒ 空集 + Warning | `TcsSelHostDelegate.cpp` | 空集 = 零判定；与 G4 方向相反是**有意区分** | 删 Warning 则装配缺失与合法空集混淆 |
| G6 | `ITcsEntityQuery::GetLocation` | `TcsEffect/Public/Host/TcsEntityQuery.h` | **判据明文列举的正面样本**（"实体有位置"）；纯虚契约，实现权在宿主 | 范围选择/距离判定失去唯一位置通路 |
| G7 | `ITcsEntityQuery::IsAlive` / `EnumerateEntities` | 同上 | **只声明"能问"，不声明答案**；框架执行器**拒绝**内部裁决存活 | 宿主无法表达存活语义 |
| G8 | 伤害委托 5 个中性默认实现 | `TcsDamage/Public/Flow/TcsDamageFlowDelegate.h` | 五值全是**单位元**（1.0 必中 / 0.0 不暴击 / 空 Tag / 原样返回 / 0 无护盾）；**对称性**：都落在"不介入"一侧 | "普通项目零 delegate"承诺（PV-7/D7-2）当场破产 |
| G9 | `FTcsFlowBaseDamage` 空 `Delegate` ⇒ 原样用输入 | `TcsDamage/Public/Flow/Steps/TcsFlowSteps.h` | 输入由调用方给，框架不推导（流程零计算） | 不配公式的项目无法造成伤害 |
| G10 | `FTcsFlowExecute` 键兜底 → 框架契约键 | `TcsFlowStepsCore.cpp` | 框架词汇（步骤间接口）；步骤字段优先，宿主可改指 | 官方默认模板跑不通；"漏配静默读到 0"回归 |
| G11 | 黑板 `Read` 折叠初值 0.0 | `TcsDamage/Private/Flow/TcsFlowAttributes.cpp` | 加法单位元 + "流程工作值空集语义" | 无收集时按残留值扣血（不可复现） |
| G12 | 护盾默认 0 | `TcsFlowStepsCore.cpp` | 数值单位元 ⇒ 不吸收 | 同 G8 |
| G13 | `FTcsFlowHit/Crit/Element` 空 Delegate 降级 | `TcsFlowStepsRest.cpp` | 与 G8 同源同值；三步**不在**默认模板 | 宿主失去步骤级挂点，只能改全局接口默认 |
| G14 | `FTcsStepDamage::FlowTemplateId` 空 = 官方默认模板 | `TcsDamage/Public/Chain/TcsStepDamage.h` | 判据 2 形状层；替换点三处（字段 / `RegisterTemplate` / `RunTemplate`） | 每个宿主须先登记模板才能发伤害 |
| G15 | 官方默认流程模板四步骨架 | `TcsDamage/Private/TcsDamageSubsystem.cpp:51-56` | 判据 2 明文授权；**刻意不组装 Hit/Crit/Element** 恰是不下结论的证据 | `RunTemplate` 直接 Error；零配置项目一次伤害都打不出 |
| G16 | `ResolvedAttrKey` 双空字段 | `TcsFlowStepsCore.cpp:165-168` | **判据 3 逐字适用**：取值必然是宿主本体论 ⇒ 框架**不填**，只提供请求方指定 | 扣血功能整体消失；硬编码属性名才是真违规 |
| G17 | 流程步骤 `Conditions` 空 = 无条件 | `TcsDamage/Public/Flow/TcsFlowStepConditions.h` | 空数组 ⇒ 循环不执行 ⇒ `true`（"无判定"编码） | 默认模板四步全部静默停止 |
| G18 | `RandomValue` 默认形参 0.0 | `TcsFlowStepConditions.h` / `TcsEffect/Public/Trigger/TcsTriggerCondition.h` | 语义 = "未注入随机源"（D0-1 确定性纪律），非"随机值是 0" | 合法装配态失去表达；改非 0 才真成玩法结论 |
| G19 | `Probability` 默认 0.0 | `TcsTriggerCondition.h` / `TcsFlowStepConditions.h` | 概率下确界 = "未配置"；不带信息 | 默认值缺失 ⇒ POD 未初始化；改 1.0 ⇒ 忘配即必触发 |
| G20 | 记录环形缓冲容量 128 | `TcsDamageSubsystem.cpp:35-39` | 工程形状常量；"全量归档归宿主"已明示 | 删上限 ⇒ 无界增长内存泄漏 |
| G21 | `UTcsCombatEntityComponent::ExecuteChainById` 上下文装配 | `TcsIntegration/Private/Entity/TcsCombatEntityComponent.cpp` | 与 G1 同源；`Context` 可被调用方整体改写；组件三职责由类注释穷举封口 | 组件无法独立起链，R3 竖切入口断掉 |
| G22 | `FTcsTimeSource_Default`（帧增量 × `TimeDilation`） | `TcsCore/Public/Clock/TcsTimeSource.h` | 只引用引擎自身量；覆盖点 `SetTimeSource`；"回合制宿主可注入替换"已明示 | `TimeSource` 为空时无实现可调 |
| G23 | `ETcsAttrOverrideTieBreak` 四值 + 默认 `OTB_Max` | `TcsAttribute/Public/Attribute/TcsAttrModInstance.h` | 引用词汇全是**数学比较**；**封闭枚举**保证"赢家与遍历顺序无关"；默认 = 兼容性锚点 | 开放自定义 ⇒ "顺序无关"结论崩塌 |
| G24 | `FoldTcsAttributeBands` 默认形参 | `TcsAttribute/Public/Attribute/TcsAttributeBandFold.h` | 同 G23；服务"没有这个概念的调用方" | "唯一实现、三处共用"纪律失效 |
| G25 | `ABM_None` / `AVD_Clamp` 默认态 | `TcsAttribute/Public/Attribute/TcsAttributeBounds.h` | `ABM_None` = "无边界" = **不下结论**；`AVD_Clamp` = 纯数学钳制 | 删 `AVD_Clamp` ⇒ `uint8` 默认落到未实现的 `AVD_Custom`，**静默语义漂移** |
| G26 | `ETcsAttributeOp` 五带 + `TAO_Add = 0` | `TcsAttrModInstance.h` | 引用词汇是**代数运算**；**无 Custom 位**（计算在上游求值传入终值） | 删默认 ⇒ "默认 = 加法"从显式变隐式，枚举重排即静默换语义 |
| G27 | `ETcsOperandKind` 两值 + `OPK_Literal = 0` | `TcsAttrModInstance.h` | 框架只提供"值从哪来"的**来源形状**，不含公式 | `AttributeScaled` 通路成孤儿 |
| G28 | `SelectTargets` 未配 `Selector` ⇒ 目标集保持原样 | `TcsTargeting/Private/Chain/TcsStepSelectTargets.cpp` | "保持不变" = 本步骤未生效；与"选中空集"的**可区分性**是诊断契约 | 改成清空 ⇒ "技能放出去没打任何人" |
| G29 | 触发器 `GateTags` 空 = 无开关 / `Conditions` 空 = 无条件 | `TcsEffect/Private/Trigger/TcsTriggerEvaluator.cpp` | `TcsEffectTrigger.h:46` 原文「空 = 默认行为」——**本仓对第三态最早的文字表述** | 翻成"空 = 拒绝" ⇒ 整个触发行机制默认失效 |
| G30 | `EMT_Exact = 0` / 空过滤 = 不限 / `EED_Immediate = 0` | `TcsCore/Public/EventBus/TcsAsyncAction_ListenForCombatEvent.h` / `TcsEventBus.h` | 引用词汇全是框架自身机制；三者都是最窄/最直接的中性起点 | 删 `EED_Immediate` ⇒ 收集协议与订阅通道不匹配，**静默失效** |
| G31 | `FTcsParamValueSource` 两个能力位中性默认 | `TcsCore/Public/Parameter/TcsParamValueSource.h` | "能力位默认开、由实现方声明关闭"——把决定权交给来源（宿主扩展点） | 退化为中心 `switch`（正是源码明文禁止的） |
| G32 | `UTcsEventBus::EventObserver` 全量钩子 | `TcsCore/Public/EventBus/TcsEventBus.h` | 钩子**不做任何过滤**（"绑定者自行过滤"）⇒ 框架拒绝在此下路由结论；未绑定则完全不存在 | BP 订阅面与 `OnCombatEvent` 失去数据源 |

### 5.1 十条最容易被误删的（清理开工前 MUST 先读）

`G1` `FTcsSelSelf` · `G3` 空 Filter = 全过 · `G6` `GetLocation` · `G8` 伤害委托 5 个中性默认 · `G14`/`G15` 官方默认模板（四步骨架 + 空 id 兜底）· `G16` `ResolvedAttrKey` 双空字段 · `G23`/`G26` 覆盖带四值 / 运算带五带 · `G29` `GateTags` 空 = 无开关。

**最需要论证的一条 = `G8`**（五个默认值第一眼全是玩法结论）。其合规性靠**对称性论证**：`1.0`（必中）与 `0.0`（不暴击）在各自判定式（`TcsFlowStepsRest.cpp:91` 的 `Modified >= 1.0`、`:118` 的 `Modified > 0.0`）里**都落在"该阶段不改变流程走向"的那一侧**。框架若要下玩法结论，会去挑一个**有区分度**的值（如命中率 0.85）；选单位元是唯一不携带玩法信息的选法。

---

## 6. 判据的回溯应用（上一轮异议 #1 走完最后一步）

按 §2.2 的豁免边界回溯上一轮清单：

| 编号 | 是否降级 | 理由 |
|---|---|---|
| S1a `bHit = true` | **不降级** | 在**默认模板的 `Completed` 步**里 ⇒ 豁免不覆盖 |
| S1b `Element = 空` | **不降级** | 同上，是默认路径的行为 |
| S1c `bCrit` / `Absorbed` | **降级（本就不是违规）** | 从契约键读回，取值归宿主钩子（判据 4） |
| S1d `bKill` 阈值 | **不降级** | 在**默认模板的 `Execute` 步**里，属内容层裁定（强度：弱） |
| S2 `Executed = max(0, …)` | **部分降级** | 下限钳 ⇒ ④；减法模型 ⇒ ③（弱）。见 §3.2 |
| S4 **事件词** | **降级 → 不再算违规** | 计数错误（9 而非 11）；全是阶段/结果名，属**形状层协议节点**；对应的 `Hit`/`Crit`/`Element` 步**不在**默认模板。**上一轮把形状层误算进了内容层** |
| S5 **黑板键** | **降级 → 不再算违规** | "6 个玩法词"不成立（最多 2–3 个）；键是**步骤之间的接口**（框架词汇），且宿主可用项目 ini 自由声明自定义键、框架不预设不校验 |
| S6 默认模板"不组装" | 不降级（本就合规） | 判据 5 在**模板粒度**的实例 |
| S7 实体查询 | 不降级（边界合规） | 保留 R-1 缺陷 |
| S8 零公式自述 | 不降级（本就合规） | "识别出宿主本体论并把取值交出去"的正面样本 |
| S9 零下限钳 | **不降级（本轮最强）** | 在默认模板的 `Execute` 步里，且**架空宿主配置点**（§3.3） |

**收敛结论**：上一轮"8 处越界全在 `TcsDamage`"的**粗结论成立**（非 `TcsDamage` 模块零违规已独立复核），但**"8 处"虚高**。扎实的结果是：

- **越界 3 处**：S1a、S1b（① 硬编码常量字段）+ S1d（③ 弱档）
- **框架内定 1 处**：S2 的减法模型（③ 弱档，缺覆盖点）
- **契约覆盖 1 处**：S9（**本轮最强，一行删除即可消灭**）
- **全部集中在 `Source/TcsDamage/Private/Flow/Steps/TcsFlowStepsCore.cpp` 的 `Execute` / `Completed` 两个函数里** ⇒ **修复面收束到单文件双函数**

---

## 7. 非 `TcsDamage` 模块"零违规"的正面论证

不是"看起来没问题"，逐模块给论证：

- **TcsTargeting**：`FTcsTargetSelectorStrategy::Resolve` 默认体**空实现**；`FTcsTargetFilterStrategy::Pass` 默认 `return true`（"无判定"编码）。二者只引用框架契约。`TcsTargetFilterStrategy.h:18-19` 还把"原枚举式 `TTF_Alive/TTF_Hostile`"作为**已删除的反例**记录在案。唯一自带选择器 `FTcsSelSelf` 只引用 `Context.Caster`。
- **TcsIntegration**：`UTcsCombatEntityComponent` 三职责全部基于句柄与门面转发，不认识链/步骤类型；`UTcsPieEntityQuery::IsAlive` 自注为宿主本体论。⇒ 边界合规（缺陷另计 R-1）。
- **TcsNotation**：`ETcsValueConventionFlag` 四值是 `Percent` / `OneMinus` / `Negate`——**数值记法变换**，只引用"值是个数"。ToolTip 举例写了"减少 1000 生命"，属**举例用词**而非内建策略（建议中性化，仅登记）。
- **TcsAttribute**：属性**名**一律由宿主 `defTag` 给出（`TcsAttributeDef.h:134-136` 明文"属性名是**项目词汇**"）；值域钳制只在宿主配了边界时生效（`TcsAttributePipeline.cpp:298-308`）。⇒ 合规。
- **TcsCore**：`UTcsDeveloperSettings` 是**空壳**，零配置项；全模块零 `UE_DEFINE_GAMEPLAY_TAG`。
- **TcsEffect**：两个内置条件的默认值是空集 / `Probability = 0.0`（中性起点）；`ETcsExecutionGate` 只有 `TEG_Always` 与占位。

---

## 8. 边界情形（**审计者未能判定**，登记待裁决）

| # | 项 | 两种读法 | 为何未定 |
|---|---|---|---|
| E1 | `Hit = 1.0` / `Crit = 0.0` 借用**合法取值域**而非"未提供"哨兵 | A：单位元 ⇒ 契约默认（本报告 §5 G8 采用）。B：与"命中率真的是 100%"不可区分 ⇒ 框架**确实**宣告了"默认必中无暴击" | 取决于"单位元借用合法取值域"算不算下结论。**建议**：判为契约默认，但 MUST 补注释明示"1.0/0.0 是'该阶段不介入'的编码，不是推荐的命中/暴击设定" |
| E2 | `ETcsExecutionGate::TEG_AuthorityOnly` 未实现但恒通过 | A：网络姿态形状层字段，"未实现"已写进 `DisplayName`，属透明。B：**有名字但语义未兑现**且给了相反行为 ⇒ 属误导性默认 | "透明地给错行为"与"不给"哪个更合规，需口径裁决 |
| E3 | 三处**过期注释**（`TcsTargetSelectorStrategy.h:18` 与 `TcsCombatEntityComponent.{h,cpp}` 的"R3 不提供任何框架默认选择器" / "R3 无 `FTcsSelSelf` 选择器"） | 与代码已不一致（R3 vs R5 台账） | **MUST NOT** 据陈旧注释判 `FTcsSelSelf` 越界——注释写的是 R3 事实，代码是 R5 事实。**只改注释、不动代码** |

---

## 9. 本报告的适用边界（**清理开工前 MUST 明确**）

1. **"可被宿主覆盖"是契约默认的定义条件之一**。反过来说：**一旦某默认值在宿主项目里成为不可替换的硬编码，它就不再是契约默认**，应重新按"框架下结论"审计。本报告 §5 各项**逐条验证过覆盖点在运行期可达**（G8→`Step->Delegate`；G10→步骤字段；G16→`Context.TargetAttrKey`；G14/G15→`RegisterTemplate`；G22→`SetTimeSource`；G6/G7→实现接口）。
2. **唯一需要持续盯的是 G15（默认模板）**：它登记后若被某宿主当"唯一模板"使用，那是**宿主的用法问题**（`RegisterTemplate` 允许登记多个、`UnregisterTemplate` 允许撤下），不改变插件归属。
3. **判据 4 的双向陷阱**：`TcsEffectSubsystem.h:406` 写着"R3 无消费者"——这类台账句 **MUST NOT** 被读成删除理由；同理"有消费者"也不因此归框架。归属只由"引用了什么词汇"决定。
4. **若清理口径严于判据 2**（连形状层默认也删），本报告 32 项**全部落入待删**——但那样 `G14`/`G15`（判据 2 明文授权的可替换默认模板）与 `G2`/`G3`/`G31`（中性默认实现 + `meta=(Hidden)`）都会被删，**框架将无法编译，或无法跑通一次伤害**。**这条边界必须在清理开工前明确：判据 2 是否仍然有效。**
5. **不覆盖**：`openspec/specs/**` 与 `openspec/changes/**` 的规格文本（本报告以**源码**为锚；规格与代码冲突时，本报告 §4 R-2 即为实例）；`Intermediate/UnrealSharp/UHT/**` 生成文件；`Content/` 资产（未扫）。
6. **行号时效**：全部行号来自当日实际读取（未做任何写入）。源码一旦改动即漂移，以"符号名"锚定。

**已知证据不足项（诚实登记）**：① 上一轮 S1–S8 的原始条目文本未获取，本报告编号为**按线索反推重建**；② `bHit` / `Element` 在 LAC 侧是否有订阅者**未核**（按判据 4 不影响归类，但影响面描述需上调）；③ `TcsCore` 的 `TcsClockSubsystem` / `TcsEventBusSubsystem` 为**定向 grep 级**证据，非逐行；④ `UTcsPieEntityQuery` 是否被 LAC **子类化**（决定 R-1 实际影响面：子类化则 `Cast` 成功、无静默失效）。

---

## 10. 对后续提案的输入

1. **「伤害模块边界整肃」提案**（S1–S7）：MUST 按 **①硬编码常量 / ②接缝但名字是宿主概念 / ③框架自下的模型结论 / ④契约默认 / ⑤契约覆盖** 五分处置，**MUST NOT** 按上一轮"一并删"的建议执行。**§5 的 32 项是删除禁令**。
2. **`Element` 字段删除**随该提案一同落地（已从 `reroot-gameplay-tag-vocabulary` 移交——理由：两条提案都 MODIFY `openspec/specs/damage-primitive/spec.md` 的「伤害记录与事件」会造成归档覆盖）。
3. **S9 是无争议的低成本首项**：删除 `TcsFlowStepsCore.cpp:179` 的 `FMath::Max`，值域交回属性层的宿主配置点。
4. **R-1 / R-2 / R-3 是真缺陷**，与边界整肃分开排期（R-2 建议**改规格不改实现**）。
5. **§2.1 的"第三态双条件"与 §2.2 的"豁免边界"** 是本报告对判据体系的两条贡献，建议固化进 `unreal-development-workflow` 的框架边界规则。

---

### 10.1 实施注记（2026-10-03 回填）

> **本报告正文是冻结文档，本节只加实施注记、不改写正文**（`tasks.md` 8.2 的纪律）。

「伤害模块边界整肃」提案（`refactor-damage-module-boundary`）**已按本节 1–5 条落地**，逐条对账：

| 本节条 | 落地情况 |
|---|---|
| 1 **五分处置** | **已按五分逐类执行**，未按上一轮"一并删"：① 2 处（`bHit` 改读契约键 / `Element` 字段删除）· ③ 2 处（`ResolveExecutedDamage` / `IsLethal` 两个新委托方法，中性默认 = 现状）· ⑤ 1 处（删 `Execute` 步零下限）· ② 补注释与登记。**§5 的 32 项删除禁令逐项复核：一项未删** |
| 2 **`Element` 字段删除** | 已删（含唯一写入点）；全仓反查只命中**步骤名 / 委托方法 / 事件名 / 注释**（零字段声明、零写入、零读取）；元素语义改由 `DamageCategory` + `ClassificationTags` 承载 |
| 3 **S9 是无争议的首项** | 已删（`TcsFlowStepsCore.cpp` 的 `Execute` 步），值域交回属性层宿主配置点；`ABM_None` = 不设边界的既有默认未动 |
| 4 **`R-2` 改规格不改实现** | 已并入本案并落地：delta 改为"写入**上下文请求字段**" + 一条 MUST NOT 写黑板的规范句；实现零改动（重复计数实测理由在规格里写明为 MUST NOT 回退） |
| 4 **`R-1` / `R-3`** | `R-3`（`FName` 时代死常量）已随 `reroot-gameplay-tag-vocabulary` 清掉；**`R-1` 仍未排期**——需设计改动（把"句柄 ↔ Actor 映射"抽为中立容器），与本报告一致地视为**缺陷而非边界问题** |
| 5 **判据固化** | §2.1 第三态双条件与 §2.2 豁免边界已固化进 `unreal-development-workflow` 技能的「框架与宿主的边界」节 |

**两处实施期回填（供后续提案参考，不构成对本报告判据的更正）**：

1. **委托方法的多目标口径**：`ResolveExecutedDamage` 的形参含"目标句柄"（`design.md` Open Question 3 预留的回报口），但 `Absorbed` 按现状是**全目标之和** ⇒ 该映射实为**流程级**的一次取值，调用点取 `Targets[0]`（与既有 `CalculateBaseDamage` 步同款）。**逐目标吸收/执行是行为变更，须另开提案**。
2. **新增委托方法对 UnrealSharp 宿主不满足"零适配"**：接口声明在插件模块、实现类在宿主模块时，宿主 **glue 镜像**不会随接口刷新（导出按"声明该类型的模块"判脏）⇒ `CS0535`。纯 C++ 宿主与 fresh clone 不受影响。详见 `log/decisions-log.md` 同日条目与 `tasks.md` 7.5。

**未发现本报告的判据适用错误**（`tasks.md` 8.3）：五分处置的每一档都经实测站得住；上述两处是**提案的兼容性承诺与签名口径**问题，不是审计判据问题。

---

## 相关文档

- 入口：[INDEX.md](../INDEX.md)
- tag 契约（根段注册表）：`openspec/specs/gameplay-tag-governance/spec.md`
- 换根提案（`Element` 移交的落点与理由）：`openspec/changes/reroot-gameplay-tag-vocabulary/proposal.md`
- 边界判据的来源规则：`~/.agents/skills/unreal-development-workflow`（框架边界规则一节）
