# Design: GameplayTag 词表换根（re-root）

## Context

- **词表现状**（本提案前）：45 个 tag = 宿主 LAC `Config/DefaultGameplayTags.ini` 24 个 + 本插件原生 21 个。原生 21 个全在 3 个 `.cpp` 里定义、4 个公开头里导出声明；插件 `Source/` 内零宿主 tag 字面量、零 `Probe` 引用。
- **契约现状**：tag 命名/归属/常量名规则写在 `openspec/project.md:17/20/21`。用户已声明该文件**当前不作为规范来源、之后会整体重写**，故 tag 契约需要迁到规格层（`specs/` 是事实来源）。
- **规范来源**：跨项目的 tag 治理规范与引擎机制事实住用户级技能 `unreal-gameplay-tags`（`SKILL.md` + `references/taxonomy.md` / `authoring.md` / `engine-facts.md` / `governance.md`）。本设计**不复制**该技能的规则正文，只落"本插件怎么用"的契约。
- **约束**：
  - 本仓**不允许提交**（用户级 `AGENTS.md` 的 SourceControl 规则最高优先级）——改动只落工作区；
  - 三仓拆分（插件 / TGFS / LAC），顺序 **插件 → TGFS → LAC**：契约修订便宜且唯一决定 LAC 那 20+ 处改动的目标形态；TGFS 侧提案未开工，纯文本改动；
  - 宿主侧 `add-tirefly-game-flow-system` 提案（0/23）将引入宿主自有的原生 tag 根，其规范**归 LAC 侧自己的 spec**，本插件契约只管自己这 8 个根。

## Goals / Non-Goals

- **Goals**
  - 根段判据落到"消费角色"，一角色一根；根名唯一指向一个消费场景；
  - 每个根各自拿回完整深度预算（现状三族卡 4 段零余量）；
  - 插件侧 tag 契约有唯一规范载体（新能力 `gameplay-tag-governance`），`project.md` 缩为指针；
  - 受影响的 10 个既有能力的规格文本与新词表**零矛盾**。
- **Non-Goals**
  - 不改任何 C++ / ini（实现归 `tasks.md`）；
  - 不改宿主仓与 TGFS 仓（各自提案）；
  - 不引入受限 tag、不新增 `Probe` 根、不动模块依赖与类形状；
  - 不把引擎侧机制规则（声明位置四分类、改名与重定向、段命名风格、`DevComment` 写法细节）搬进本仓规格。

## Decisions

### D1 根 = 消费角色（不是业务系统、不是 owner 前缀）

**决策**：词表的根段判据 = "这个词会被**哪条代码路径**解析/匹配"。

**理由**：`Tcs` 是 **owner 前缀**，不是消费角色——按 owner 分域时，跨系统共用的词无处安放，且同一机制的不同角色被强行挤进一根；而按消费角色划分时，一个消费者只属于一侧，域所有权天然清晰。根新增的**唯一理由**是出现了新的消费角色；根数量应约等于消费角色数量，不随内容增长。

**代价（明示接受）**：这等于把"一刀切的前缀保护"换成"**逐根的唯一性论证**"——UE 的 tag 命名空间全局共享（引擎 + 全部插件 + 商城内容同处一棵树），引入第三方内容或新插件时 MUST 回头查根段注册表有没有撞车。

### D2 根段注册表（13 根；两边各登自己那一半）

**决策**：注册表按仓拆分——本插件登 **8 个**根（写进 `gameplay-tag-governance`），宿主侧 5 根登 LAC 侧契约。

| # | 根 | 消费角色（哪条代码路径解析它） | 声明方 | 形态 | 登记处 |
|---|---|---|---|---|---|
| 1 | `TcsEvent` | TCS 事件总线订阅/广播 | 插件原生（框架事件）+ 宿主 ini（宿主自发布） | `TcsEvent.<域>.<名>`（3 段） | **插件** |
| 2 | `TcsStateParam` | 参数表读取（`ITcsParamTableReader`，实参来自 Def 参数行） | 宿主 ini | `TcsStateParam.<键>`（2 段） | **插件** |
| 3 | `EffectChainRunVar` | 链内变量（`SetRunVariable` / `TryGetRunVariable`） | 宿主 ini | `EffectChainRunVar.<键>`（2 段） | **插件** |
| 4 | `DamageFlowKey` | 黑板读写（`FTcsFlowAttributes::Submit` / `Read`） | 插件原生（契约键）+ 宿主 ini（自定义键） | `DamageFlowKey.<键>`（2 段） | **插件** |
| 5 | `DamageFlowTemplate` | 流程装配（`RegisterTemplate` / `FindTemplate`） | 插件原生（`Default`）+ 宿主 ini | `DamageFlowTemplate.<Id>`（2 段） | **插件** |
| 6 | `EffectChain` | 链登记/查找（`RegisterChain` / `FindChain`） | 宿主 ini | `EffectChain.<Id>`（2 段） | **插件** |
| 7 | `EffectTriggerGate` | 触发行的**行级开关**（`SetTriggerGateTag` / `IsTriggerGateTagLit`；四道门第三道） | 宿主 ini | `EffectTriggerGate.<词>`（2 段） | **插件** |
| 8 | `DamageCategory` | **供条件匹配的伤害分类集**（触发侧 `FTcsTriggerCondition_HasAllTags` + 流程侧 `FTcsConditionHasAllTags`，**两个匹配面共用一套词**） | 宿主 ini | `DamageCategory.<词>`（2 段） | **插件** |
| 9 | `Attribute` | 属性存储/聚合（`FTcsAttributeStore`） | 宿主 ini | `Attribute.<名>`（2 段） | LAC |
| 10 | `Probe` | 验证装置 | 宿主 ini + 非 Shipping 消费模块 | `Probe.<机制>.<词>`（3 段） | LAC |
| 11 | `TgfsEvent` | TGFS 枢纽订阅/广播 | TGFS 原生（生命周期）+ 宿主（领域事件） | `TgfsEvent.<域>.<名>`（3 段） | LAC |
| 12 | `GameFlowPhase` | 流程栈 phase 查询（`IsActivePhase`） | 宿主 | `GameFlowPhase.<阶段>`（2 段） | LAC |
| 13 | `GameFlowCompleter` | 完成声明者白名单 | 宿主 | `GameFlowCompleter.<声明者>`（2 段） | LAC |

**共享根**（框架契约词 + 宿主内容词同根并存）：1 / 4 / 5。三者各自的既有口径（"契约词原生声明；宿主自定义键自由"）直接成立，**一行都不用改**。

**`DamageCategory` 的"多读取点 / 单角色"（2026-10-01 裁定，必须记进注册表）**：该根被**两个匹配面**读取（触发侧 `TcsTriggerCondition.cpp:234` + 流程侧 `TcsFlowStepConditions.h:80`），但**不是两个角色**——写链为：`FTcsFlowElement` → 宿主 `Execute_ResolveElement` → `ClassificationTags.AddUnique(Element)`（`TcsFlowStepsRest.cpp:139`，全库唯一写入点）→ `TcsDamageFlowCollectEvent.cpp:32` 直通载荷 → `TcsTriggerEvaluator.cpp:70` 拷进触发上下文 → 两处 `Contains` 匹配。⇒ 判据是"**词的归属是否唯一**"，不是"**读取点是否唯一**"。这条同时写进 `gameplay-tag-governance` 的「一角色一根」判据边界与 `effect-trigger` / `damage-step-library` 两个匹配面。

### D3 29 个词的精确映射

**决策**：只有 **29** 个词需要 re-root（21 原生 + 8 宿主正式词）；**16 个 `Probe` 词直接删、不 re-root**（比"45 词全量 re-root"轻得多）。两个 `GcDelegate` 同名冲突随 `Probe.<机制>.*` 自动消解（`Probe.EffectChain.…` vs `Probe.DamageFlowTemplate.…`）。

#### 原生词（21 个，本插件；实现见 `tasks.md`）

| # | 旧 tag 文本 | 新 tag 文本 | 旧常量名 → 新常量名 | 声明点 |
|---|---|---|---|---|
| 1 | `Tcs.Event.Attribute.ValueChanged` | `TcsEvent.Attribute.ValueChanged` | `Tag_Tcs_Event_Attribute_ValueChanged` → `Tag_TcsEvent_Attribute_ValueChanged` | `TcsAttributeChangedEvent.cpp` |
| 2 | `Tcs.Event.Damage.FlowStarted` | `TcsEvent.Damage.FlowStarted` | `Tag_Tcs_Event_Damage_FlowStarted` → `Tag_TcsEvent_Damage_FlowStarted` | `TcsDamageSubsystem.cpp` |
| 3 | `Tcs.Event.Damage.PreHit` | `TcsEvent.Damage.PreHit` | `…_PreHit` → `Tag_TcsEvent_Damage_PreHit` | 同上 |
| 4 | `Tcs.Event.Damage.Hit` | `TcsEvent.Damage.Hit` | `…_Hit` | 同上 |
| 5 | `Tcs.Event.Damage.Crit` | `TcsEvent.Damage.Crit` | `…_Crit` | 同上 |
| 6 | `Tcs.Event.Damage.Element` | `TcsEvent.Damage.Element` | `…_Element` | 同上 |
| 7 | `Tcs.Event.Damage.AfterDamage` | `TcsEvent.Damage.AfterDamage` | `…_AfterDamage` | 同上 |
| 8 | `Tcs.Event.Damage.PreExecute` | `TcsEvent.Damage.PreExecute` | `…_PreExecute` | 同上 |
| 9 | `Tcs.Event.Damage.Completed` | `TcsEvent.Damage.Completed` | `…_Completed` | 同上 |
| 10 | `Tcs.Event.Damage.Recorded` | `TcsEvent.Damage.Recorded` | `…_Recorded` | 同上 |
| 11 | `Tcs.Event.Damage.ModifierConsumed` | `TcsEvent.Damage.ModifierConsumed` | `…_ModifierConsumed` | 同上 |
| 12 | `Tcs.Flow.Key.BaseDamage` | `DamageFlowKey.BaseDamage` | `Tag_Tcs_Flow_Key_BaseDamage` → `Tag_DamageFlowKey_BaseDamage` | `TcsFlowKeys.cpp` |
| 13 | `Tcs.Flow.Key.Executed` | `DamageFlowKey.Executed` | `…_Executed` | 同上 |
| 14 | `Tcs.Flow.Key.Absorbed` | `DamageFlowKey.Absorbed` | `…_Absorbed` | 同上 |
| 15 | `Tcs.Flow.Key.Kill` | `DamageFlowKey.Kill` | `…_Kill` | 同上 |
| 16 | `Tcs.Flow.Key.Hit` | `DamageFlowKey.Hit` | `…_Hit` | 同上 |
| 17 | `Tcs.Flow.Key.Crit` | `DamageFlowKey.Crit` | `…_Crit` | 同上 |
| 18 | `Tcs.Flow.Key.ExecuteCandidates` | `DamageFlowKey.ExecuteCandidates` | `…_ExecuteCandidates` | 同上 |
| 19 | `Tcs.Flow.Key.HitRate` | `DamageFlowKey.HitRate` | `…_HitRate` | 同上 |
| 20 | `Tcs.Flow.Key.CritRate` | `DamageFlowKey.CritRate` | `…_CritRate` | 同上 |
| 21 | `Tcs.Flow.Template.Default` | `DamageFlowTemplate.Default` | `Tag_Tcs_Flow_Template_Default` → `Tag_DamageFlowTemplate_Default` | 同上 |

#### 宿主正式词（8 个，属 LAC 仓；本插件规格中的示例文本同步）

| # | 旧 tag 文本 | 新 tag 文本 | 所属根 |
|---|---|---|---|
| 22 | `Tcs.Attr.Health` | `Attribute.Health` | `Attribute` |
| 23 | `Tcs.Attr.MaxHealth` | `Attribute.MaxHealth` | `Attribute` |
| 24 | `Tcs.Attr.Attack` | `Attribute.Attack` | `Attribute` |
| 25 | `Tcs.Attr.Armor` | `Attribute.Armor` | `Attribute` |
| 26 | `Tcs.Chain.Slice_Chain` | `EffectChain.SliceChain` | `EffectChain` |
| 27 | `Tcs.Chain.Formula_Chain` | `EffectChain.FormulaChain` | `EffectChain` |
| 28 | `Tcs.Flow.Template.Default_Slice` | `DamageFlowTemplate.DefaultSlice` | `DamageFlowTemplate` |
| 29 | `Tcs.Flow.Template.Slice_Flow` | `DamageFlowTemplate.SliceFlow` | `DamageFlowTemplate` |

#### 规格内示例名（同规则修 + 顺手去掉语义重复）

| 出处 | 旧示例 | 新示例 | 说明 |
|---|---|---|---|
| `effect-chain/spec.md` | `Tcs.Chain.Chain_Test` | `EffectChain.ChainTest` | `Chain.Chain_Test` 的 "Chain" 重复两次 |
| `effect-chain-asset/spec.md` | `Tcs.Chain.Chain_Whirlwind` | `EffectChain.Whirlwind` | 同上 |
| `damage-flow/spec.md` | `Tcs.Flow.Template.Flow_Test` | `DamageFlowTemplate.FlowTest` | `Template.Flow_Test` 的语义重复 |
| `attribute-types/spec.md` | `Tcs.Attr.AttackPower` | `Attribute.AttackPower` | 段内下划线（`Chain_Test` / `Slice_Chain` 一类）一并消失 |

#### 宿主侧 `Probe` 词（16 个，**整批退役删除**——不换根）

裁定（用户 2026-10-01 拍板）：这 16 个词**不参与 re-root，整批退役删除**（判据 = 用户级 `unreal-gameplay-tags` 的「验证词退役就是整根删」）。各自的验证提案（`add-scripting-reflection-surface` / `add-host-scripting-slots` / `verify-tcs-host-scripting-e2e`）**均已归档且有证据**，ini 里的 `DevComment` 也逐条写着"验证后可删"。

> **2026-10-01 修订**：本节此前写的是"换根规则 + 形态示例"（`Tcs.Chain.Probe.X` → `Probe.EffectChain.X`），与本提案「被否方案」D 行的"直接删"**自相矛盾**。用户 2026-10-01 裁定以**删除**为准，换根规则随本节作废。

**连带面（LAC 提案 MUST 同批处理）**：词删了而消费方还在 ⇒ `new("Tcs.Chain.Probe.*")` 解析为**未注册 tag**，探针命令响亮失败。全部消费点（本轮**逐词核过**，16 词一个不漏）：

| 消费文件 | 消费的 Probe 词 | 词数 |
|---|---|---|
| `Script/LegendAutoChessCS/TcsProbe/TcsChainProbe.cs` | `Tcs.Chain.Probe.ScriptingReflection` · `Tcs.Chain.Probe.HostSlot` · `Tcs.Flow.Template.Probe.HostSlot` · `Tcs.Param.Probe.SlotTest` | 4 |
| `Script/LegendAutoChessCS/TcsProbe/TcsHostScriptingE2EProbe.cs` | `Tcs.Chain.Probe.HostScriptingE2E{,.Flow,.Generation,.SelectorOnly,.EmptySelector,.EmptyFilter,.GcSlots,.GcDelegate}` · `Tcs.Flow.Template.Probe.HostScriptingE2E{,.GcDelegate}` · `Tcs.Param.Probe.EffectMarker` | 11 |
| `Script/LegendAutoChessCS/TcsProbe/TcsProbeEffectStep.cs` | `Tcs.Param.Probe.EffectMarker` · `Tcs.Param.Probe.TargetCount` | 2 |

4 + 11 + 2 − 1（`EffectMarker` 两处共用）= **16 词，全部落在这 3 个文件**。**17 个 `.uasset`/`.umap` 二进制扫描这 16 词零命中** ⇒ 退役面只在脚本侧，不触任何序列化资产。

`Probe` **根保留**（注册表第 10 根）：本轮退役的是这 16 个**词**、不是该角色；将来自建验证词仍按 `Probe.<机制>.<词>` 登记（≤4 段）。

#### 宿主侧拒绝面夹具词（3 个，**故意不登记**——A 表的补漏项）

`Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp` 的 3 个**故意无效**字面量（`RequestGameplayTag(..., ErrorIfNotFound=false)`，拒绝面检查的输入夹具；首轮清点漏登记，2026-10-01 补）：

| 旧文本 | 新文本 | 夹具语义 |
|---|---|---|
| `Tcs.Dev.Reject.Mismatch` | `Probe.Reject.Mismatch` | "与资产 `ChainId` 不一致的那个 id"（瞬态双真相资产被 `IsDataValid` 拒） |
| `Tcs.Dev.Reject.Other` | `Probe.Reject.Other` | 同上，`Chain.ChainId` 侧 |
| `Tcs.Dev.Reject.NotRegisteredChain` | `Probe.Reject.NotRegisteredChain` | "未登记链 id"（**必须解析失败**：Error 日志 + 无效句柄，不崩溃） |

判据三条：①旧根 `Tcs` 是 owner 前缀，直接违反根名判据（与 29 个换根词同因）；②消费角色 = **验证装置**（拒绝面 rig）⇒ 归 `Probe` 根，`Reject` 作机制段；③它们 **MUST NOT 进任何 tag 表**——"解析不到"正是被检查的行为本身，故**刻意不声明**（这也解释了 `Validate-GameplayTags.ps1` 看不见它们：脚本只扫 ini 与原生声明宏，不扫 `RequestGameplayTag` 字面量）。3 段，深度合规。

### D4 原 B5（受限 tag 强制"一处声明"）整条撤销

**背景**：此前的 B5 方案是"用受限 tag 强制 `Tcs.Event` 与 `Tcs.Flow.Key` 下只能有一处声明"（最强档，`bAllowNonRestrictedChildren = false`），代价是必须为"项目自定义黑板键"另开新域。

**决策（用户拍板）：整条撤销**——不是放弃目标，是**自然消解**：re-root 后 `DamageFlowKey` 是**共享根**（框架契约键插件原生 + 宿主自定义键宿主 ini），既有两处规格写的"契约键原生声明；项目自定义键自由，由项目 ini 声明"**直接成立，一行都不用改**。原冲突（给 `Tcs.Flow.Key` 设限会挡住宿主自定义键）随 re-root 消失；`Tcs.Flow.Template` 的混合归属也没了（拆成 `DamageFlowTemplate` 后框架的 `Default` 与宿主的模板同根不同词，本就是既有口径）。

**连带取证（记录在案，供实现与校验参考）**：受限机制在 UE 5.8 里**物理上无法施加于原生词**，且拦截只在"显式声明过的祖先"上生效（隐式父段写了也不生效）；它本身是**编辑器防呆、不是加载期拒绝**（手改 ini 照样加载成功）。故"一处声明"的强制层实际是**归属规则（契约）+ 重复声明校验（脚本）**，这也正是本提案的做法。

### D5 `TcsStateParam` 的 "State" 取广义

**决策**：`TcsStateParam` 的 "State" 包含**技能激活运行态**；**不是** `TcsState` 模块，MUST NOT 被读作"只有状态模块的参数才住这里"。**技能运行态归状态。**

**依据**：`TcsState` 是插件既有模块名（`project.md:8`），技能却住 `TcsSkill`；代码把两者当**两个运行身份**处理——状态实例句柄 `FStateInstance` vs 施法运行句柄 `FCastRun`（`11-module-notation.md` 并列两个 `ParamSnapshot`）。两者产出的参数行**同属"参数表读取"这一个消费角色**，故同住一根；根段注册表的判据是消费角色，不是"哪个模块产出它"。

### D6 载体与拆分（三仓三提案）

**决策**：
- **插件仓（本提案）**：**8 根**注册表 + 21 原生词 re-root + A 组 `DevComment`（交付归 `tasks.md`，**规范义务不落本仓规格**）+ **11 个**能力的 MODIFIED delta + `project.md` 指针化（`FTcsDamageRecord::Element` 字段删除**不在本提案**，已移交「伤害模块边界整肃」）；
- **TGFS 仓**：改其提案 spec 文本（`Flow.Phase.*` → `GameFlowPhase.*`、`Flow.Completer.*` → `GameFlowCompleter.*`、`Flow.Event.*` → `TgfsEvent.*`、`Tgfs.FlowEvent.Phase.*` → `TgfsEvent.Phase.*`）；
- **LAC 仓**：24 个 ini 词（8 换根 + 16 删 `Probe`）+ `Source/TcsDev/` + `Script/LegendAutoChessCS/TcsProbe/`（约 18 处硬编码 tag 串）+ 2 个 `Content/TcsDev/*.uasset` + 约 10 份文档 + **29 条 `FGameplayTagRedirect`** + 宿主侧 5 根注册表。
- **顺序 = 插件 → TGFS → LAC**（理由见 Context）。

**`FGameplayTagRedirect` 的落点（取证结论）**：规则 = "redirect 写进**该 tag 所属列表所在的那个配置文件**"。本次 29 次改名**全部落一处**——LAC 的 `Config/DefaultGameplayTags.ini` 的 `[/Script/GameplayTags.GameplayTagsSettings]` 节（原生词走"无 TagList 源 → 回落 `UGameplayTagsSettings`"分支；ini 词本就住该节）。两条约束：① 同一 `OldTagName` 指向多个目标会命中 `ensureMsgf`，故 29 条 `OldTagName` 必须唯一；② 多跳被压平成单跳，故**直接写终态目标，不链式**。已弃用位置 = `DefaultEngine.ini` 的 `[/Script/Engine.Engine]`（命中打 Error 日志）。

### D7 载体边界（不复制引擎侧规则）

**决策**：本仓规格只承载**插件侧**契约（8 根注册表、归属规则、共享根、一角色一根、根名判据、深度上限、`TcsStateParam` 语义边界）。"声明位置四分类 / 段命名风格 / 改名与重定向细节 / **`DevComment` 义务（通用规范）** / 引擎构建顺序与时序"等**跨项目规则与引擎机制事实**住 `unreal-gameplay-tags` 技能（其 `governance.md` 检查项 A1/A4），本仓 MUST NOT 保留第二份。

**`DevComment` 的处分（2026-10-01 用户裁定）**：原写的第 8 条 ADDED 需求「原生词的可读语义说明（`DevComment`）」**删除**——它是**通用规范**，唯一载体是技能。本插件 spec 只保留**这一次的具体交付**（`tasks.md` 第 3 节的 21 处补齐），不把通用义务复述成插件契约。相应地，规格正文里指向该义务的措辞一并去掉了（`damage-primitive` 的「伤害记录与事件」原写"由本模块原生声明并带 `DevComment`"，改为只写"由本模块原生声明"）。

**连带处置**：
- `openspec/project.md:17`（ownership rule + 项目 tag 命名 `Tcs.Attr.*` 等）、`:20`（事件 tag 命名公约 `Tcs.Event.<域>.<事件名>`）、`:21`（常量名逐点换下划线）**缩为指向 `gameplay-tag-governance` 的指针**；`:18` 的示例常量名 `Tag_Tcs_Flow_Key_X` 按新根修正为 `Tag_DamageFlowKey_X`；
- `project.md` 保留与 tag 无关的条目（identifier = `FGameplayTag` 标准、`FTcsAttributeName` 移除、跨模块注册与反射可见性、字段默认值陷阱、模块词汇边界）；
- 规格里出现 `Attribute.<名>` 一类宿主词时，一律标注为**示例文本**，不构成插件声明的词、也不二次定义宿主侧根名。

### D8 第二轮裁定（消费者清点后，2026-10-01）

**决策**：对 8 根注册表逐词做消费者清点后，补四条裁定——三条把"看起来像新角色"的词**归回既有角色**，一条把规则的应用点挪到消费者 API 处。

| # | 项 | 裁定 | 依据 |
|---|---|---|---|
| 1 | `DevComment` 需求 | **从规格删除**（只留 `tasks.md` 的 21 处交付） | 通用规范的唯一载体是技能 `governance.md`（A1/A4）；插件 spec 不复述通用义务（见 D7） |
| 2 | `FTcsDamageFlowContext::FormulaParams` 的键 | **归 `TcsStateParam`**，MUST NOT 另立根 | 三层值空间的层次关系（`TcsDamageFlowContext.h:24-27`）：第 1 层「技能参数（账本，Entry 级持久）」**不进流程**、经解算后作为第 2 层传入——第 2 层就是 `FormulaParams`，故其键与 Def 参数行**同词汇、同消费角色**；为传参层另立根 = 把"同一批键"误读成两个角色 |
| 3 | `FTcsDamageRecord::Element` | ~~归"分类"角色（与 `ClassificationTags` 同根）~~ → ~~D9-3：字段直接删除~~ → **D9-3 改判：移交独立提案（2026-10-01 评审）** | `TcsDamageFlowContext.h:57` 原文"分类 Tag 集（来源标签启动写入 / **元素标签由 Element 步骤写入**——词表归项目，插件只搬运与匹配）"——元素标签就是写进分类集的那一个标签；**但字段本身零消费者，故最终裁定不是"给它一个根"**；删除动作本身归「伤害模块边界整肃」 |
| 4 | `EffectChainRunVar` 命名契约的落点 | **移到 `effect-interpreter`**（`SetRunVariable` / `TryGetRunVariable` 的 API 定义处）；`effect-chain` 只留一句指针 | 同一主题只允许一个载体：规则本体在 `gameplay-tag-governance` 的注册表，**应用点跟消费者 API 走** |

**连带效应**：`FTcsParamSource_ParamRef` 的存在性校验分工同时收口——**运行期不校验**（miss 落 `Fallback` 是 PV-2 既定设计）、**编辑器期校验归 M8 校验器**（与 DAG 去重同属 M8 职责）。原措辞"插件 MUST NOT 校验其存在性"把"静默降级"写成了规范，与"拼错的 tag 在解析处 ensure、不静默降级"的既有口径相背，且让 M8 丢掉一项职责。

### D9 第三轮裁定（按 `FGameplayTag` 槽位清点，2026-10-01）

**决策**：对**槽位**（而非词）做消费者清点后补三条——两个"无根角色"定名成根（6 根 → **8 根**），一项记录字段删除**移交独立提案**，一项文档/代码偏差登记为未决。

| # | 项 | 裁定 | 依据 |
|---|---|---|---|
| 1 | `EffectTriggerGate`（**新根**，第 7 根） | 角色 = 触发行的**行级开关**；单一解析方 = `SetTriggerGateTag` / `IsTriggerGateTagLit`（四道门第三道）；声明方 = 宿主 ini；形态 `EffectTriggerGate.<词>`（2 段） | 槽位 `FTcsEffectTriggerDef::GateTags`（`TcsEffectTrigger.h:99`）；**机制名限定成立**（只有这两条 API 解析它） |
| 2 | `DamageCategory`（**新根**，第 8 根） | 角色 = **供条件匹配的伤害分类集**；声明方 = 宿主 ini（词由宿主 `ResolveElement` 决定）；形态 `DamageCategory.<词>`（2 段）；**两个匹配面共用一套词**，注册表 MUST 记该事实与写链 | 槽位 `ClassificationTags`（4 处 header：`TcsTriggerContext:44` / `TcsTriggerPayloadReader:49` / `TcsDamageFlowContext:58` / `TcsDamageFlowContextView:75`）+ 两处条件 `Tags`（`TcsTriggerCondition.h:279` / `TcsFlowStepConditions.h:27`）+ `FTcsDamageRecord::Element`；写链见 D2 备注 |
| 3 | `FTcsDamageRecord::Element` **删除，移交独立提案**（**2026-10-01 评审改判**：原"并入本提案"作废——该字段与「伤害模块边界整肃」的 S1 属**同一个 struct、同一行字段枚举**（`openspec/specs/damage-primitive/spec.md:99`），两提案各改一半会让后归档者覆盖前者的粘贴全文） | 删字段；元素改由 `DamageCategory` 词表 + `ClassificationTags` 承载 | ① 全库对它的操作只有一处且是**置空**（`TcsFlowStepsCore.cpp:240`）；② **零消费者**（`grep Element` 全 18 处，除声明 `TcsDamageRecord.h:56` 与置空外全是步骤名 / 事件名 / 注释）；③ 与 `ClassificationTags` **语义重叠**（元素的实际承载已是分类集，`TcsFlowStepsRest.cpp:139` `AddUnique`）；④ **违反内置边界规则**（`decisions-log.md:129`，2026-09-30 用户拍板）：*"凡是需要宿主词汇的内置策略，插件一律不提供"*——框架**记录形状**里烘进"元素"这个玩法名词（其取值由宿主 `ResolveElement` 的词表决定）正是该规则要拦的形态；⑤ **宿主侧代码影响面 = 0**：LAC `Source/` + `Script/` 对 `Element` **零引用**（本轮已核） |
| 4 | 文档/代码偏差**不在本提案拍板** | 登记为 Open Question + `tasks.md` 项：`TcsDamageFlowContext.h:57` 与 `TcsDamageFlowContextView.h:73` 都写"**来源标签启动写入**"，但 `CollectStart`（`TcsFlowStepsCore.cpp:67-70`）**只发流程开始事件 + 重置黑板、不写分类集**；全库唯一写入点在 Element 步骤 | 要么补实现（让"来源标签"真的在启动时写入）、要么改注释；两条路都改行为或改契约，故按用户裁定**登记不拍板** |

**"多读取点 / 单角色"的判据补充（写进 `gameplay-tag-governance` 的「一角色一根」）**：分根判据是"**词的归属是否唯一**"，不是"**读取点是否唯一**"。`DamageCategory` 被触发侧与流程侧两处 `HasAllTags` 读取 ⇒ 仍是**一个角色、一个根**；反之 `DamageFlowKey` 下框架契约键与宿主自定义键**归属不同却读取点相同** ⇒ 也**不拆根**（拆的只是声明方）。这条补充是防止后续把"两处匹配"误判成"两个角色"而错误拆根。

## 被否方案与理由

| # | 方案 | 判定 | 理由 |
|---|---|---|---|
| A | **保留 `Tcs` 单前缀，靠"再分细一点"解决深度** | ❌ 否 | 三族已经卡在 4 段零余量，再加层就越过上限；根段的判据本来就不该是 owner 前缀——这是把既有违反项继续固化 |
| B | **一根两 facet**（`DamageFlow.Key.<键>` / `DamageFlow.Template.<Id>`） | ❌ 否 | 仍是一根兼两角色（运行期读写值槽 vs 装配期按 id 查模板），两条代码路径挤一根；且只把 4 段压到 3 段，两个角色仍共享深度预算、无法独立演进 |
| C | **B5：受限 tag 强制"一处声明"** | ❌ 撤销 | re-root 后 `DamageFlowKey` 是共享根，既有口径直接成立，冲突自然消解；且受限机制物理上无法施加于原生词、只对显式祖先生效、仅是编辑器防呆——对症的强制层是归属规则 + 重复声明校验 |
| D | **45 词全量 re-root** | ❌ 否 | 逐条核验后：只有 29 词需要 re-root，**16 个 `Probe` 词整批退役删除**（验证词退役就是整根删），比全量轻得多；`Probe` 保留其独立根，不参与 re-root。**连带面（3 个 C# 探针消费文件）与自相矛盾处的修订见 D3「宿主侧 `Probe` 词」** |
| E | 把 `TcsStateParam` 改名为 `SkillStateParam` / `CastParam` 以避开 `TcsState` 模块名 | ❌ 否 | 技能只是该键空间的消费者之一（状态实例读同一键空间），用 `Skill`/`Cast` 限定会把"状态实例的参数"排除在外；改用注记把 "State" 的广义钉死，比换名更准确 |
| F | 让插件自带 redirect / 自带受限声明 | ❌ 否（机制不可行） | 原生词没有 TagList 源 ⇒ redirect 走 `UGameplayTagsSettings` 回落分支（项目侧）；每插件自动注册自己 `Config/Tags` 的引擎代码是注释掉的 TODO，唯一自动注册的搜索路径是 `<项目>/Config/Tags`。故 redirect 与受限声明**只能由宿主项目提供**，插件把义务写进契约与交办项 |
| G | 把宿主侧 5 根注册表也登在插件规格里 | ❌ 否 | 同一主题只允许一个载体：宿主侧根名的规范载体是 LAC 侧契约；插件规格只保留**示例文本**（用于说明宿主内容词形态），不复制条目 |
| H | 一次性把 `EffectChainRunVar` 与 `EffectChain` 合成一根（`EffectChain.RunVar.<键>`） | ❌ 否 | 两条不同消费路径（`SetRunVariable`/`TryGetRunVariable` vs `RegisterChain`/`FindChain`）= 两个角色 = 两个根；合成即回到"facet 段兼收两角色"的反例形态 |

## Risks / Trade-offs

| 风险 | 影响 | 缓解 |
|---|---|---|
| 21 个原生词的**文本改名**是破坏性变更 | 已序列化进资产/蓝图/DataTable 的旧 tag 引用会失联 | 29 条 `FGameplayTagRedirect`（宿主 ini 登记，LAC 提案交付）+ `WarnOnInvalidTags` 扫残留；重定向"直接写终态、不链式" |
| 跨模块消费者按常量名引用 | 常量改名会打断宿主 C++ 编译 | 导出声明与定义同步改；LAC 侧是唯一消费者且同批改；判据 = "设计意图是否供外部用"，MUST 手写带模块导出宏的声明 |
| 中文 `DevComment` 可能因字符集/宏包装出问题 | 编译失败 | 本批**实测一次**（检查点 1），失败则退回英文说明（不阻塞换根本体） |
| 宿主**自发布事件**归属规则放宽（`TcsEvent` 下允许宿主加词） | 与既有"事件 tag 一律不进项目 tag 表"口径相反 | 既有口径的理由（"宿主漏配 → 事件静默丢失"）只对**框架分发**的契约词成立；宿主自己的事件丢了是宿主自己的内容问题，且解析期有 ensure 兜底。规格里明确划界：**框架词汇**仍 MUST 原生声明 |
| 规格里宿主侧根名与 LAC 侧契约**双写**风险 | 两边措辞漂移 | 插件规格只写"示例文本 + 指针"（D7），规范性定义住 LAC 侧 |
| ~~**删 `FTcsDamageRecord::Element` 是记录形状的破坏性变更**（D9-3）~~ → **已移交**「伤害模块边界整肃」提案（2026-10-01 评审；理由见 `proposal.md`「明确不做」，本行保留作追溯） | 记录是**过网/载荷结构**（`TcsEvent.Damage.Recorded` 的载荷）——字段虽**零消费者**，但已序列化的蓝图/资产/DataTable 里若手工拼过该字段会在加载期失配；宿主若有按字段名读记录的脚本也会断 | ① 删声明 + 删唯一写入点（`TcsFlowStepsCore.cpp:240`）同批；② 全仓反查 `Element` 零残留（只允许步骤名/事件名）；③ 加载告警面 + 打开含该事件的蓝图/DataTable 资产做指向性检查并在实现记录留证；④ 记录里判元素的正确姿势改为**经事件载荷/触发条件侧的分类匹配**（`FTcsTriggerPayloadInfo::ClassificationTags` 已直通），不提供"取记录字段"的替代口 |
| 深度上限的"余量"观念被后续变更吃掉 | 3 段族再各加一层就满 | 规格明文"增根不增层"+ 校验脚本按 `-Roots` 清单检查（含段数） |

## Migration Plan

1. **契约先行（本提案）**：新增 `gameplay-tag-governance` + 11 个能力 MODIFIED delta + `project.md` 指针化；`openspec validate --strict --no-interactive` 通过 → 评审批准。
2. **实现（本仓，`tasks.md`）**：21 个原生词改名 + 常量名同步 + 21 处 `DevComment` → Development / Shipping 双配置编译 → 存档归档提案。
3. **重定向（LAC 仓交办）**：29 条 `FGameplayTagRedirect` 一次性登记进 LAC 的 `Config/DefaultGameplayTags.ini`（唯一且不链式）。
4. **宿主整改（LAC 仓提案）**：8 个正式词换根、16 个 `Probe` 词删除、硬编码串与 `uasset` 同步、宿主侧 5 根注册表；TGFS 仓提案同步改文本。
5. **回滚**：改写回旧 tag 文本 + 删除对应 redirect 即可（无数据迁移、无资产重存）；风险窗口仅限"改名已落地、redirect 未登记"这一步。**窗口消解方式 = 两仓交付窗口尽量贴近**（本仓**无 `Content/`、无 `Config/`**，已核实 ⇒ 零序列化资产承载这些 tag，风险完全在 LAC 内部），**不是**"两仓必须同批交付"——29 条 redirect 只能落 LAC 的 ini、改名只能落本仓，物理上不可能同批（见 `tasks.md` 第 5.4 项）。

## Open Questions

1. **`effect-chain-asset` 的主资产身份名**：需求写明"名取 `ChainId.GetTagName()`"，而 `GetTagName()` 返回**完整 tag 文本**（含根段）。原 scenario 写的是叶子形态 `[PrimaryAssetType, Chain_Whirlwind]`（与需求正文不一致——疑似 `FName ChainId` 时代的残留）。本提案按需求正文改写为 `[PrimaryAssetType, EffectChain.Whirlwind]` 并点明"名含根段"；若宿主资产库已按叶子形态落过 `PrimaryAssetId`，需按实际情况回退该措辞。
2. **`FGameplayTagRedirect` 的登记时机**：与 C++ 改名同批（同一次改动里"旧名解析不到"的窗口为零）还是分开（先登记 redirect 再改名）？本提案按"同批"写 `tasks.md`，但插件仓无法提交该 ini（属 LAC），故实际能否同批取决于两仓交付节奏。
3. **`ClassificationTags` 的"来源标签启动写入"是文档与代码的偏差（用户裁定：登记不拍板）**：`TcsDamageFlowContext.h:57` 与 `TcsDamageFlowContextView.h:73` 都写"分类 Tag 集（**来源标签启动写入** / 元素标签由 Element 步骤写入）"，但 `CollectStart`（`TcsFlowStepsCore.cpp:67-70`）**只发流程开始事件 + 重置黑板，不写分类集**——全库唯一写入点是 Element 步骤（`TcsFlowStepsRest.cpp:139` 的 `AddUnique`）。**两条路都待裁决**：① **补实现**——让"来源标签"（如状态的 `ClassificationTags`）真的在启动步骤写入；② **改注释**——把"启动写入"从两处注释里去掉，声明分类集**只有** Element 一步写入。**本提案两者都不做**（改行为或改契约都超出"换根"的范围），只登记在 `tasks.md`（见第 6.4 节）。该偏差还牵动一件事：若走 ①，则"来源标签"也需明确住 `DamageCategory` 根（否则会变成第三套分类词）。
4. **已决（从 Open Questions 移出，留档备查）**：`FormulaParams` 的键 → `TcsStateParam`（D8-2）；`EffectChainRunVar` 契约落点 → `effect-interpreter`（D8-4）；`DevComment` 需求 → 从规格删除（D8-1）；`FTcsParamSource_ParamRef` 存在性校验 → 运行期不校验 / 作者期归 M8（D8 连带效应）；**"三个无根角色" → 已定名两根**（D9-1 `EffectTriggerGate` / D9-2 `DamageCategory`），第三个（**实体 tag 匹配**：按 tag 匹配实体/目标的词空间）**在本轮清点中无对应槽位证据，故未立根**——将来自建时再立 + 登记（与"LAC 目前无自建事件机制，故不立 `LacEvent` 根"同款处理）；`Element` 字段 → **移交独立提案**（D9-3 改判，不再是"同根"问题，也不在本提案）。
