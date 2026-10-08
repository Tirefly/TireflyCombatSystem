# gameplay-tag-governance Specification

## Purpose

TCS 插件侧 GameplayTag 词表的**规范载体**：钉死插件那 **12 个根段**与各自的消费角色，规定归属规则（框架词汇 vs 宿主内容词汇 vs 验证词）、一角色一根、根名判据（含"根名互不为真前缀"）、检查词的 `Check` 子段约定、深度上限，以及 `TcsStateParam` / `DamageCategory` 等易误读根的语义边界。

> **根数沿革（MUST 与需求内根表同步，勿只改一处）**：9 → 10（2026-10-04 增 `StateDef`，R5 Task 1）→ 11（2026-10-05 增 `AttrModDef`，R5 Task 7，台账 `ATTR-1` 按期闭合）→ **12**（2026-10-06 增 `SkillDef`，R6 Task 1）。**本 `## Purpose` 的数字 MUST 与下方根表的行数一致**——归档器**只替换「需求」、不碰本段**，故每次增根都要**手工**同步此处的数字（`verify-state-layer-e2e` 归档时即为手工补的一处，见 `CONVENTION` §6.6）。

**边界**：本能力只管**插件那一半**。宿主侧 5 个根段的规范载体在 LAC 仓的 `host-gameplay-tag-registry` 能力，本规格只**引用**不复制（两边各登自己那一半）。跨项目的通用治理规则（根名判据、深度上限、`DevComment` 义务、受限 tag 机制、改名与重定向口径）住在用户级 `unreal-gameplay-tags` 技能，同样只引用不复制。

**为什么需要它**：换根前 28 个 tag 全挤在 `Tcs.*` 下（**把 owner 前缀当根**），`Tcs.Event` / `Tcs.Flow.Key` / `Tcs.Flow.Template` 三族已卡在 4 段零余量、再加一层就越过上限。根段的判据本来就不该是 owner 前缀——本能力把"哪个词归哪个根、为什么"变成可校验的规格，并明确"增根不增层"。

## Requirements

### Requirement: 根段注册表

本插件 MUST 维护并遵守一份**插件侧根段注册表**——每个根登记「消费角色（哪条代码路径解析它）/ 声明方 / 形态（段数）」三项。新建根之前 MUST 先查该表；表内已有承载同一消费角色的根时 MUST NOT 另立新根。

本插件拥有的根（共 12 个）：

| 根 | 消费角色（哪条代码路径解析它） | 声明方 | 形态（段数） |
|---|---|---|---|
| `TcsEvent` | TCS 事件总线的订阅/广播 | 插件原生（框架事件）+ 宿主 ini（宿主自发布事件） | `TcsEvent.<域>.<名>`（3 段，无域时 2 段） |
| `TcsStateParam` | 参数表读取（`ITcsParamTableReader::TryGetNumericParam`，实参来自 Def 参数行） | 宿主 ini | `TcsStateParam.<键>`（2 段） |
| `EffectChainRunVar` | 链内变量读写（`UTcsEffectSubsystem::SetRunVariable` / `TryGetRunVariable`） | 宿主 ini | `EffectChainRunVar.<键>`（2 段） |
| `DamageFlowKey` | 流程黑板读写（`FTcsFlowAttributes::Submit` / `Read`） | 插件原生（契约键）+ 宿主 ini（自定义键） | `DamageFlowKey.<键>`（2 段） |
| `DamageFlowTemplate` | 流程模板装配与查找（`RegisterTemplate` / `FindTemplate`） | 插件原生（官方默认模板 `Default`）+ 宿主 ini | `DamageFlowTemplate.<Id>`（2 段） |
| `EffectChain` | 链定义的登记与查找（`RegisterChain` / `FindChain`） | 宿主 ini | `EffectChain.<Id>`（2 段） |
| `EffectTriggerGate` | 触发行的**行级开关**（`SetTriggerGateTag` / `IsTriggerGateTagLit`；四道门的第三道） | 宿主 ini | `EffectTriggerGate.<词>`（2 段） |
| `EffectTriggerDef` | **触发定义资产的身份解析**（`UTcsDefinitionSubsystem::DiscoverTriggerDefs` / `ResolveTriggerDef`；资产身份 `[PrimaryAssetType, TriggerTag]` 与去重键共用它，**2026-10-04 新增**） | 宿主 ini | `EffectTriggerDef.<名>`（2 段） |
| `StateDef` | **状态定义资产的身份解析**（`UTcsDefinitionSubsystem::DiscoverStateDefs` / `ResolveStateDef`；资产身份 `[PrimaryAssetType, DefTag]` 与去重键共用它，**2026-10-04 新增**） | 宿主 ini | `StateDef.<名>`（2 段） |
| `AttrModDef` | **修正器模板资产的身份解析**（`UTcsDefinitionSubsystem::DiscoverAttrModDefs` / `ResolveAttrModDef`；资产身份 `[PrimaryAssetType, TemplateTag]` 与去重键共用它，**2026-10-05 新增**） | 宿主 ini | `AttrModDef.<名>`（2 段） |
| `SkillDef` | **技能定义资产的身份解析**（`UTcsDefinitionSubsystem::DiscoverSkillDefs` / `ResolveSkillDef`；资产身份 `[PrimaryAssetType, DefTag]` 与去重键共用它，**2026-10-06 R6 Task 1 新增**） | 宿主 ini | `SkillDef.<名>`（2 段） |
| `DamageCategory` | **供条件匹配的伤害分类集**（触发侧 `FTcsTriggerCondition_HasAllTags` + 流程侧 `FTcsConditionHasAllTags`） | 宿主 ini | `DamageCategory.<词>`（2 段） |

**`SkillDef` 的立根依据（2026-10-06 R6 Task 1）**：本变更**制造**了一个新的身份解析方（定义库第五条按类发现路径 `DiscoverSkillDefs` + `ResolveSkillDef`，见 `integration-entity` 能力）——这是一个**新的消费角色**，表内没有任何既有根承载它：`StateDef` 是**状态**定义资产的身份角色（`UTcsBuffDefAsset`），`AttrModDef` 是修正器模板的身份角色，二者与"技能定义资产的 `DefTag` 解析"都不是同一条代码路径。**先例**：触发定义身份→`EffectTriggerDef`（2026-10-04）、修正器模板身份→`AttrModDef`（2026-10-05），本条是**同款第三次**。**登记边界（MUST 如实记录）**：`ResolveSkillDef` 的运行期调用者本轮为**零**——本 Task 只交付**发现与索引**（资产身份 `[PrimaryAssetType, DefTag]` 可寻址、重复身份在发现期被拦、就绪日志出计数）；真正的运行期取用从 Task 2（账本）起。形态与 `StateDef` 在 R5 Task 1 落地时（"只有缓存、无消费方"）、`AttrModDef` 在 R5 Task 7 落地时同款——**这不是"预建根"，而是条件成立后的按期登记**。

**`AttrModDef` 的立根依据与它的前身（2026-10-05）**：台账 `ATTR-1` 记的是"模板身份词今天**零解析消费者**，故**不为它开根**，归属 = 触发条件：出现'按 tag 解析修正器模板'的真实需求时"。本变更**制造**了那个解析方（定义库第四条按类发现路径 + `ResolveAttrModDef`，见 `integration-entity` 能力），触发条件由此成立、根随之落地——这不是"预建根"，而是**条件成立后的按期登记**。**登记边界（MUST 如实记录）**：`ResolveAttrModDef` 的运行期调用者本轮为**零**（`ModifierRows` 仍走资产直引用 `TSoftObjectPtr<UTcsAttrModDef>`，物化器只 `Get()` / `LoadSynchronous()` 取对象、读 `->Def`，从不读 `TemplateTag`）；本条登记的消费角色 = **发现与索引路径本身**（`Initialize` 调它、就绪日志出计数、失败清单拦重复身份），形态与 `StateDef` 在 R5 Task 1 落地时"只有缓存、无消费方"同款。

**状态词（`Def.StatusTag`）本轮 MUST NOT 为它开根**：它今天的**解析消费者为零**（关系表检查器与按它匹配的槽位竞争表整体归 R5.5-e，本轮只有发现期的有效性校验）⇒ 归属待那一轮拍板（零消费者不预建根）；本轮的验收资产里 `StatusTag` 复用同一个 `StateDef.*` 词，不因此构成对状态词根归属的裁定。**同款边界适用于技能侧**：`FTcsSkillDefData` 继承白拿的 `StatusTag` 同样**不为它开根**，理由与处置相同。

**检查词的 `Check` 子段约定（2026-10-04 新增，把已在跑的实践补成规则）**：**仅供人工检查/验收使用**的内容词 MUST 落在其**功能根**下并加一个 `Check` 段（形如 `<功能根>.Check.<名>`，3 段）——MUST NOT 另立根（检查词的**消费路径与正式词完全相同**：链 id 仍须 `RegisterChain` 认、触发定义身份仍须定义库认，另立根会让消费路径读不到它），MUST NOT 把生命周期语义塞进路径中段之外的其它位置（生命周期维度只允许 `Check` 这一段的语义）。**该段是段位预算内的合法用法**：`Check` 词比同根正式词多 1 段，仍在 `词表深度上限` 的 4 段预算内留有余量。既有形态 = `EffectChain.Check.{SelfSub,Sort,SortMulti,WaitEvent,WaitEvent3}`（R4 人工检查链）。

**2026-10-05 扩充（随 `AttrModDef` 根落地）**：`StateDef.Check.*`（Task 1 / Task 7）与 `AttrModDef.Check.*` 同属上述形态；`AttrModDef.Check.*` 是**新根下的第一个 `Check` 词**，证明该约定随新根一并适用（`Check` 段不是既有根的专属特权）。**反例同时被钉死**：把生命周期词放进路径中段的形态（`<功能根>.E2E.<名>` 一类）MUST NOT 出现——宿主契约 `host-gameplay-tag-registry` 已把同形路径（其原文举 `EffectChain.Probe.X`）**当反例点名**，本插件侧同受该判据约束。**2026-10-06 R6 Task 1 同批适用**：`SkillDef.Check.*` 是 `SkillDef` 新根下的第一个 `Check` 词（验收资产 `DA_SkillDef_E2E` 的身份词）。

**`DamageCategory` 的共享说明（MUST 记录，两个匹配面共用一套词）**：该根**同时**被**触发侧**与**流程侧**两个匹配面读取——**不是**两个消费角色，而是**同一套分类词汇的两个读取点**。链路（写入 → 传递 → 匹配，全库唯一写入点）：

1. `FTcsFlowElement` 步骤经宿主 `ITcsDamageFlowDelegate::Execute_ResolveElement` 取回一个元素 Tag；
2. `Context.ClassificationTags.AddUnique(Element)`（`TcsFlowStepsRest.cpp:139`，**全库唯一写入点**）；
3. 经 `TcsDamageFlowCollectEvent.cpp:32` 直通到流程收集事件载荷（`FTcsTriggerPayloadInfo::ClassificationTags` ← `FTcsDamageFlowContext::ClassificationTags`，由 TcsDamage 的载荷读取器登记完成）；
4. `TcsTriggerEvaluator.cpp:70` 把它拷进触发上下文（`FTcsTriggerContext::ClassificationTags`）；
5. 两处 `Contains` 匹配：触发侧 `TcsTriggerCondition.cpp:234`（`FTcsTriggerCondition_HasAllTags`）、流程侧 `TcsFlowStepConditions.h:80`（`FTcsConditionHasAllTags`）。

⇒ 词的归属只有一份（**宿主 ini 声明，词由宿主的 `ResolveElement` 实现决定**），插件 MUST NOT 声明任何具体分类词，也 MUST NOT 为"触发侧"与"流程侧"各立一根。**判据澄清**：分根判据是"**词的归属是否唯一**"，不是"**读取点是否唯一**"——同一套词被两处匹配面读取仍是一个消费角色（见「一角色一根」的下述补充）。

**非本插件的根**：宿主侧另有 `Attribute` / `Probe` / `TgfsEvent` / `GameFlowPhase` / `GameFlowCompleter` 等根，其注册表条目住宿主仓的契约。本插件 MUST NOT 复制宿主侧的注册表条目；本插件规格与文档中出现的 `Attribute.<名>` 一类文本是**示例内容**（用于说明宿主内容词的形态），不构成本插件声明的词，也不构成对宿主侧根名的二次定义。

**根数量判据**：根的数量应约等于**消费角色**的数量，MUST NOT 随内容增长——某个根下的词在无限膨胀说明它是"内容根"而非"角色根"，划错了。

#### Scenario: 新建根前先查表

- **WHEN** 需要承载一个新消费角色的词
- **THEN** 先查本表：已有根承载该消费角色则归入该根并加词；仅当出现**新的消费角色**时才新增根，且 MUST 同步登记本表（含消费角色、声明方、形态段数）

#### Scenario: 新增词按消费角色归位

- **WHEN** 新增一个"会被流程黑板读写"的键
- **THEN** 它 MUST 落在 `DamageFlowKey` 根下，MUST NOT 落在 `TcsStateParam` 或其它根下

#### Scenario: 插件不自建宿主侧根

- **WHEN** 插件代码或规格需要一个属性名 / 链内部验证词
- **THEN** MUST NOT 在本插件内声明 `Attribute.*` / `Probe.*` 的词，也 MUST NOT 在本插件规格里为它们写规范条文

#### Scenario: 检查词按 Check 子段落进功能根

- **WHEN** 为一次人工检查新增一个只供检查使用的链 id 或触发定义身份
- **THEN** MUST 取其功能根并加 `Check` 段（如 `EffectChain.Check.Sort` / `EffectTriggerDef.Check.Seed`），MUST NOT 为它另立根、MUST NOT 塞进 `Probe` 根（`Probe` 只承载宿主侧验证装置词）

#### Scenario: 修正器模板身份根按角色登记

- **WHEN** 定义库新增一条"按 `TemplateTag` 解析修正器模板资产"的代码路径，需要一个承载该身份的根
- **THEN** MUST 新立 `AttrModDef` 根（`AttrMod` 为机制限定词、`Def` 为需限定的泛指角色名）并同步登记本表，MUST NOT 把该身份寄居进 `StateDef`（那是状态定义资产的身份角色）或 `Attribute`（那是宿主侧属性名词根）

#### Scenario: 技能定义身份根按角色登记

- **WHEN** 定义库新增一条"按 `DefTag` 解析技能定义资产"的代码路径（第五条按类发现路径），需要一个承载该身份的根
- **THEN** MUST 新立 `SkillDef` 根并同步登记本表（含消费角色、声明方、形态段数），MUST NOT 把该身份寄居进 `StateDef`（那是**状态**定义资产的身份角色，两者是不同的 `PrimaryAssetType` 与不同的发现路径），MUST NOT 复用 `TcsStateParam`（那是参数表读取角色，不是 Def 身份）

#### Scenario: 生命周期词不得进路径中段

- **WHEN** 为一个仅供检查使用的内容词起草路径
- **THEN** 生命周期语义 MUST 只由 `Check` 段承载（`<功能根>.Check.<名>`）；`<功能根>.E2E.<名>` 一类把生命周期缩写放进路径中段的写法 MUST NOT 被接受

### Requirement: 词归属规则（谁拥有那个词，谁声明）

**框架词汇**（框架自己分发或自己解析的契约词：框架事件、流程黑板契约键、官方默认模板 id）MUST 由**插件模块原生声明**——契约词若由宿主配置，宿主漏配即**静默破坏框架的广播面与读写面**，故不可下放。

**宿主词汇**（由宿主内容规定含义的词：属性名、参数表键、链内变量键、链 id、宿主自己的流程模板 id、宿主自发布的事件、伤害分类集词、触发行开关词）MUST 由**宿主项目的 `Config/DefaultGameplayTags.ini`** 声明；本插件 MUST NOT 声明任何宿主内容词。

**判据只看"谁拥有那个词"**，不看"插件代码有没有按名解析它"——`RequestGameplayTag(TEXT("..."))` 只是运行期按名解析，不构成声明方依据。

**唯一声明处**：同一 tag 文本 MUST 只在一处声明（原生 / ini / DataTable 的任一组合下都算违规）。原生侧的重复声明**引擎不报错**（按指针分别注册），故该违规是静默的，只能靠本规则与校验脚本拦截。

#### Scenario: 框架契约键由插件原生声明

- **WHEN** 检查流程黑板契约键（`DamageFlowKey.BaseDamage` 等）与官方默认模板 id（`DamageFlowTemplate.Default`）的声明处
- **THEN** 它们是插件模块的原生 tag 常量；宿主即使完全不配置 tag 表，框架的广播面与读写面依然成立

#### Scenario: 宿主内容词由宿主声明

- **WHEN** 需要一个属性名 / 链 id / 流程模板 id / 参数表键
- **THEN** 该词 MUST 由宿主 `Config/DefaultGameplayTags.ini` 声明；插件 MUST NOT 声明它，也 MUST NOT 预设它存在（存在性由宿主内容侧负责）

#### Scenario: 同一 tag 文本两处声明即违规

- **WHEN** 某 tag 文本同时被插件原生声明与宿主 ini 声明
- **THEN** 属违规（原生侧重复声明引擎不报错，故 MUST 由本规则与校验脚本拦截）

### Requirement: 同一根共享（共享根）

`TcsEvent` / `DamageFlowKey` / `DamageFlowTemplate` 是**共享根**——框架先在其下声明自己的契约词，宿主 MUST 能在同一根下声明自己的词；框架 MUST NOT 因宿主加词而拒绝登记或改变既有行为。

**理由**：同一根下"框架契约词 + 宿主内容词"并存是既有口径（"契约键原生声明；项目自定义键自由"），换根不改变各自的声明方，只是把两者放进同一个角色根。故本插件 MUST NOT 用受限 tag 之类的机制**禁止**宿主在共享根下加词——扩展面是"多一个词/多一个根"，不是"把契约词变成配置项"。

#### Scenario: 宿主在共享根下加自己的词

- **WHEN** 宿主在 `Config/DefaultGameplayTags.ini` 里于 `DamageFlowKey` 下新增一个自定义键，并把该键配进某个流程数据步骤
- **THEN** 该键可用且不触发任何插件侧拒绝；框架契约键的含义与行为不变

#### Scenario: 契约词不可配

- **WHEN** 宿主想改写框架契约词（例如让 `DamageFlowKey.BaseDamage` 指向别的文本）
- **THEN** MUST NOT 通过配置实现——契约词是插件原生常量，宿主的扩展方式是在共享根下**新增自己的词**

### Requirement: 一角色一根（正交化）

一个根 MUST 只承载**一个消费角色**。同一机制下的不同角色 MUST 各自成根，MUST NOT 用一个 facet 段（如 `Key` / `Template`）把两个角色兼收在同一个根里。

**判据 = 两条消费者是不是不同的代码路径**：`DamageFlowKey` 的消费者是"运行期读写值槽"（黑板提交/读取），`DamageFlowTemplate` 的消费者是"装配期按 id 查模板"（`RegisterTemplate` / `FindTemplate`）——两条路径 = 两个角色 = 两个根，哪怕它们同属"伤害流程"这一个机制。

**判据的边界（MUST，防止把"多读取点"误判成"多角色"）**：分根判据是"**这个词的归属是否唯一**"，**不是**"读取点是否唯一"。同一套宿主词若被两处匹配面读取（`DamageCategory`：触发侧与流程侧两个 `HasAllTags` 匹配器共用宿主 `ResolveElement` 产出的同一套分类词），它仍是**一个消费角色、一个根**——两处只是同一词汇的两个读取点，词的归属只有一份。反向边界同样成立：**归属不同的词即使读取点相同**（如 `DamageFlowKey` 下的框架契约键与宿主自定义键）也**不拆根**（它们同属"黑板读写"这一个角色），拆的只是**声明方**（见「同一根共享」）。

**收益不止省段位**：拆根后每个根各自拿到完整的深度预算，两个角色可独立演进（一方加分区不影响另一方）。

#### Scenario: 机制内的两个角色各自成根

- **WHEN** 检查"伤害流程"机制的根
- **THEN** 黑板键住 `DamageFlowKey`、模板 id 住 `DamageFlowTemplate`；两者 MUST NOT 合并回 `DamageFlow.Key.*` / `DamageFlow.Template.*` 一类"一根两 facet"的形态

#### Scenario: 新角色不寄居在既有根下

- **WHEN** 出现一个"既不是黑板键、也不是模板 id"的新消费角色（例如一个新的运行期变量袋）
- **THEN** MUST 新立根并登记注册表，MUST NOT 以 `DamageFlowKey.<分区>.<词>` 之类的方式寄居在既有根下

### Requirement: 根名判据

根名 MUST = `[唯一化限定] + 角色名`，且 MUST 唯一指向一个消费场景；不唯一时 MUST 加限定词。裸角色名（`Param` / `Run` / `Chain` / `Template` 一类泛指词）MUST NOT 单独作根。**任一根名 MUST NOT 是另一根名的真前缀（2026-10-04 新增）**——BP/CS 动态订阅层支持**部分匹配**，"订阅前缀根"会把另一根下的词一并命中（两个语义不同的根互为前缀是治理地雷，发现即 MUST 给其中一个换名或加限定）。

**为什么唯一性可以由根名自己承担**：UE 的 tag 命名空间全局共享（引擎 + 全部插件 + 商城内容同处一棵树），唯一性确实必须有人承担——但这份唯一性由**根名**承担即可，不要求统一的命名空间前缀。代价是把"一刀切的前缀保护"换成了"**逐根的唯一性论证**"：引入第三方内容或新插件时 MUST 回头查根段注册表有没有撞车。

本插件 11 个根名的由来（2026-10-04 增 `StateDef`；2026-10-05 增 `AttrModDef`）：

| 根 | 唯一化限定 | 角色名 |
|---|---|---|
| `TcsEvent` | `Tcs`（事件按系统分根，故加系统限定） | `Event` |
| `TcsStateParam` | `TcsState`（"State" 取广义，见「`TcsStateParam` 的语义边界」） | `Param` |
| `EffectChainRunVar` | `EffectChain` | `RunVar` |
| `DamageFlowKey` | `DamageFlow` | `Key` |
| `DamageFlowTemplate` | `DamageFlow` | `Template` |
| `EffectChain` | `Effect`（`Chain` 是泛指词、MUST 加限定，限定词取机制名） | `Chain` |
| `EffectTriggerGate` | `EffectTrigger`（`Gate` 是泛指词、MUST 加限定，限定词取机制名） | `Gate` |
| `EffectTriggerDef` | `EffectTrigger`（`Def` 是泛指词、MUST 加限定，限定词取机制名） | `Def` |
| `StateDef` | `State`（`Def` 是泛指词、MUST 加限定，限定词取机制名） | `Def` |
| `AttrModDef` | `AttrMod`（`Def` 是泛指词、MUST 加限定；限定词取**机制名** `AttrMod`——修正器模板，而非 `Attribute`，见下条） | `Def` |
| `DamageCategory` | `Damage`（`Category` 是泛指词、MUST 加限定，限定词取机制名） | `Category` |

**`AttrModDef` / `StateDef` 两个 `Def` 根的并存论证（MUST，防"一根两角色"误读）**：两者角色名同为 `Def`（"定义资产身份"），但**消费角色不同**——`StateDef` 解析 `UTcsBuffDefAsset`（状态定义），`AttrModDef` 解析 `UTcsAttrModDef`（修正器模板）；按「一角色一根」的判据（两条**不同的代码路径** = 两个角色），二者 MUST 各自成根，MUST NOT 合并为 `Def.<族>.<名>` 一类一根两 facet 的形态。限定词取机制名正是为了在**同角色名**下把两个机制钉唯一。

**与宿主根 `Attribute` 的撞车回查（MUST，逐根唯一性论证的记录）**：`AttrModDef` 与宿主侧属性名根 `Attribute` 前 4 字符相同（`Attr`）。二者**互不为真前缀**（`Attribute` 的第 5 字符为 `i`、`AttrModDef` 为 `M`），故不命中"互不为前缀"禁令，也不存在"订阅 `Attribute` 命中 `AttrModDef.*`"的部分匹配风险；但仍 MUST 在本文留下这次回查记录——**同前缀段的两个根是未来最可能撞车的一对**，引入新的属性机制词时 MUST 重查。

#### Scenario: 泛指词必须加限定

- **WHEN** 为"效果链 id"或"链内变量键"起根名
- **THEN** MUST NOT 用 `Chain` / `RunVar` / `Var` / `Param` 一类泛指形态；本插件分别取"机制 + 角色"的 `EffectChain` 与 `EffectChainRunVar`

#### Scenario: 不唯一就加限定词

- **WHEN** 一个候选根名可能指向两个消费场景（如 `Param` 既可指参数表键、又可指运行期变量键）
- **THEN** MUST 用限定词把两个场景各自钉唯一（参数表键 = `TcsStateParam`，链内变量键 = `EffectChainRunVar`）

#### Scenario: 根名互不为前缀

- **WHEN** 触发定义身份的新根被提议为裸 `EffectTrigger`（它会是 `EffectTriggerGate` 的真前缀）
- **THEN** MUST 改用 `EffectTriggerDef`（与 `EffectTriggerGate` 成兄弟节点、互不为前缀）；MUST NOT 接受任何"一根是另一根真前缀"的根名组合

#### Scenario: 同角色名的两个机制各自成根

- **WHEN** 修正器模板的身份解析需要一个新根，而 `StateDef` 的角色名同为 `Def`
- **THEN** MUST 取机制限定词立 `AttrModDef` 根（与 `StateDef` 并存），MUST NOT 并入 `StateDef` 根、MUST NOT 改用一个"一根两 facet"的合并根

#### Scenario: 引入新插件时回查撞车

- **WHEN** 本插件要与新的第三方插件或宿主系统共存
- **THEN** MUST 回查根段注册表：任一根名与对方重叠或语义撞义时，MUST 加唯一化限定而不是靠"约定俗成"共存

### Requirement: 词表深度上限

tag 路径深度 MUST ≤ 4 段。第 4 段是**留给未来变化的余量，MUST NOT 现在用满**——用满说明根或分区没划对，而不是"再分细一点"。

**分类细化的手段 = 增根不增层**（沿用既有口径"增域不增层"）：要细分就新增根，或换更精确的词，MUST NOT 在既有路径上继续加层。

**度量（2026-10-05 订正）**：本插件 **11 个根**的代表词全部 ≤ 3 段——`TcsEvent.<域>.<名>` 3 段（余 1 段）；`DamageFlowKey.<键>` / `DamageFlowTemplate.<Id>` / `EffectChain.<Id>` / `TcsStateParam.<键>` / `EffectChainRunVar.<键>` / `EffectTriggerGate.<词>` / `EffectTriggerDef.<名>` / `StateDef.<名>` / `AttrModDef.<名>` / `DamageCategory.<词>` 2 段（余 2 段）；
**检查词**（`<功能根>.Check.<名>`）占 3 段——比同根正式词多用 1 段，仍余 1 段。换根前 `Tcs.Event.Damage.<名>` / `Tcs.Flow.Key.<键>` / `Tcs.Flow.Template.<Id>` 三族**全部卡在 4 段零余量**。

#### Scenario: 每个根至少留一段余量

- **WHEN** 检查本插件 11 个根下现有词的段数
- **THEN** 每族都存在未被用满的余量（3 段族余 1 段、2 段族余 2 段，含 `Check` 词后仍余 1 段），不存在 4 段词

#### Scenario: 细分走增根而不是加层

- **WHEN** 需要给某族词加一档细分
- **THEN** MUST 通过新增根或换更精确的词实现；MUST NOT 把路径加到 4 段去表达细分

### Requirement: `TcsStateParam` 的语义边界

`TcsStateParam` 的 "State" 取**广义**——包含**技能激活运行态**；它**不是** `TcsState` 模块，MUST NOT 被读作"只有状态模块的参数才住这里"。**技能运行态归状态。**

**依据**：`TcsState` 是本插件的既有模块名（状态实例句柄 `FStateInstance`），技能却住 `TcsSkill`（施法运行句柄 `FCastRun`）——代码把两者当作**两个运行身份**处理；但两者产出的参数行**同属"参数表读取"这一个消费角色**，因而同住 `TcsStateParam` 根。根段注册表的判据是**消费角色**，不是"哪个模块产出它"。

#### Scenario: 技能运行态的参数键住 `TcsStateParam`

- **WHEN** 一个技能激活运行态需要按参数表读一个键
- **THEN** 该键 MUST 落在 `TcsStateParam` 根下，MUST NOT 因"技能不住 `TcsState` 模块"而另立根

#### Scenario: 不得读成模块名

- **WHEN** 审查 `TcsStateParam` 根下的词
- **THEN** 判据是该词的消费角色（参数表读取），不是"它是否来自 `TcsState` 模块"——两种运行身份的词可以同根共存
