## MODIFIED Requirements
### Requirement: 根段注册表

本插件 MUST 维护并遵守一份**插件侧根段注册表**——每个根登记「消费角色（哪条代码路径解析它）/ 声明方 / 形态（段数）」三项。新建根之前 MUST 先查该表；表内已有承载同一消费角色的根时 MUST NOT 另立新根。

本插件拥有的根（共 11 个）：

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
| `DamageCategory` | **供条件匹配的伤害分类集**（触发侧 `FTcsTriggerCondition_HasAllTags` + 流程侧 `FTcsConditionHasAllTags`） | 宿主 ini | `DamageCategory.<词>`（2 段） |

**`AttrModDef` 的立根依据与它的前身（2026-10-05）**：台账 `ATTR-1` 记的是"模板身份词今天**零解析消费者**，故**不为它开根**，归属 = 触发条件：出现'按 tag 解析修正器模板'的真实需求时"。本变更**制造**了那个解析方（定义库第四条按类发现路径 + `ResolveAttrModDef`，见 `integration-entity` 能力），触发条件由此成立、根随之落地——这不是"预建根"，而是**条件成立后的按期登记**。**登记边界（MUST 如实记录）**：`ResolveAttrModDef` 的运行期调用者本轮为**零**（`ModifierRows` 仍走资产直引用 `TSoftObjectPtr<UTcsAttrModDef>`，物化器只 `Get()` / `LoadSynchronous()` 取对象、读 `->Def`，从不读 `TemplateTag`）；本条登记的消费角色 = **发现与索引路径本身**（`Initialize` 调它、就绪日志出计数、失败清单拦重复身份），形态与 `StateDef` 在 R5 Task 1 落地时"只有缓存、无消费方"同款。

**状态词（`Def.StatusTag`）本轮 MUST NOT 为它开根**：它今天的**解析消费者为零**（关系表检查器与按它匹配的槽位竞争表整体归 R5.5-e，本轮只有发现期的有效性校验）⇒ 归属待那一轮拍板（零消费者不预建根）；本轮的验收资产里 `StatusTag` 复用同一个 `StateDef.*` 词，不因此构成对状态词根归属的裁定。

**检查词的 `Check` 子段约定（2026-10-04 新增，把已在跑的实践补成规则）**：**仅供人工检查/验收使用**的内容词 MUST 落在其**功能根**下并加一个 `Check` 段（形如 `<功能根>.Check.<名>`，3 段）——MUST NOT 另立根（检查词的**消费路径与正式词完全相同**：链 id 仍须 `RegisterChain` 认、触发定义身份仍须定义库认，另立根会让消费路径读不到它），MUST NOT 把生命周期语义塞进路径中段之外的其它位置（生命周期维度只允许 `Check` 这一段的语义）。**该段是段位预算内的合法用法**：`Check` 词比同根正式词多 1 段，仍在 `词表深度上限` 的 4 段预算内留有余量。既有形态 = `EffectChain.Check.{SelfSub,Sort,SortMulti,WaitEvent,WaitEvent3}`（R4 人工检查链）。

**2026-10-05 扩充（随 `AttrModDef` 根落地）**：`StateDef.Check.*`（Task 1 / Task 7）与 `AttrModDef.Check.*` 同属上述形态；`AttrModDef.Check.*` 是**新根下的第一个 `Check` 词**，证明该约定随新根一并适用（`Check` 段不是既有根的专属特权）。**反例同时被钉死**：把生命周期词放进路径中段的形态（`<功能根>.E2E.<名>` 一类）MUST NOT 出现——宿主契约 `host-gameplay-tag-registry` 已把同形路径（其原文举 `EffectChain.Probe.X`）**当反例点名**，本插件侧同受该判据约束。

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

#### Scenario: 生命周期词不得进路径中段

- **WHEN** 为一个仅供检查使用的内容词起草路径
- **THEN** 生命周期语义 MUST 只由 `Check` 段承载（`<功能根>.Check.<名>`）；`<功能根>.E2E.<名>` 一类把生命周期缩写放进路径中段的写法 MUST NOT 被接受

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
