## MODIFIED Requirements

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
