# Change: GameplayTag 词表换根（re-root）

## Why

本插件的 tag 词表用**单一 owner 前缀** `Tcs` 当命名空间，把"事件"、"流程黑板键"、"流程模板 id"、"链 id"、"参数键"全部塞在一棵树下用 facet 段（`Tcs.Event.*` / `Tcs.Flow.Key.*` / `Tcs.Flow.Template.*` / `Tcs.Chain.*` / `Tcs.Param.*`）区分。这与既定规范冲突且已经付出代价：

1. **根段的判据不是"它属于哪个业务系统"**——判据是"这个词会被哪条代码路径解析/匹配"。`Tcs` 正是 owner 前缀，不是消费角色；现行词表**一直在违反该规则**，而校验脚本的 `-Namespace Tcs` 把这个形态固化成了检查项。
2. **三族词全部卡在 4 段零余量**：`Tcs.Event.Damage.<名>` / `Tcs.Flow.Key.<键>` / `Tcs.Flow.Template.<Id>` 都是 4 段，深度上限（4 段，第 4 段为余量）已被用满——后续任何细分都无处可加。
3. **同一机制的两个角色被一根兼收**：`Tcs.Flow.Key.*`（运行期读写值槽）与 `Tcs.Flow.Template.*`（装配期按 id 查模板）是两条不同的代码路径，却共用 `Tcs.Flow` 根。
4. **运行变量键在契约里无域**：`SetRunVariable` / `TryGetRunVariable` 的键被临时塞进 `Tcs.Param.*`（域误用），而参数表键本身也被 `Tcs.Param` 这个泛指根承载。

本变更把这批词的根**换掉**：根 = 消费角色，一角色一根，根名唯一指向一个消费场景，每个根各自拿回完整的深度预算。同时把插件侧的 tag 契约从 `openspec/project.md`（用户已声明其当前不作为规范来源、之后整体重写）迁到规格层，`project.md` 的 tag 段落缩为指针。

## What Changes

- **新增能力 `gameplay-tag-governance`（ADDED）**——承载**插件侧** tag 契约：插件拥有的 **8 个根**（`TcsEvent` / `TcsStateParam` / `EffectChainRunVar` / `DamageFlowKey` / `DamageFlowTemplate` / `EffectChain` / `EffectTriggerGate` / `DamageCategory`）的根段注册表（消费角色 / 声明方 / 形态段数）、归属规则（"谁拥有那个词，谁声明"）、共享根、一角色一根（正交化，含"多读取点 / 单角色"判据边界）、根名判据、深度上限（增根不增层）、`TcsStateParam` 的 "State" 语义边界。
- **21 个原生词换根**（**BREAKING**：tag 文本改变，跨模块消费者与宿主订阅面同步改名）：
  - `Tcs.Event.*` → `TcsEvent.*`（11 个：属性变更 1 + 伤害收集/记录 10）——`Tcs.Event.Damage.<名>` 由 4 段降为 3 段；
  - `Tcs.Flow.Key.*` → `DamageFlowKey.*`（9 个）——由 4 段降为 2 段；
  - `Tcs.Flow.Template.Default` → `DamageFlowTemplate.Default`（1 个）——由 4 段降为 2 段。
  - 常量名按"逐点换下划线"同步（如 `Tag_Tcs_Event_Damage_Hit` → `Tag_TcsEvent_Damage_Hit`、`Tag_Tcs_Flow_Key_BaseDamage` → `Tag_DamageFlowKey_BaseDamage`）；公开头里的跨模块导出声明一并改名。
- **A 组：21 处 `DevComment`（交付项，不是插件契约）**——`UE_DEFINE_GAMEPLAY_TAG` → `UE_DEFINE_GAMEPLAY_TAG_COMMENT(TagName, Tag, Comment)`，写清语义 / 谁声明谁消费（宏第三实参经 `TEXT(...)` 包裹，**中文说明能否过编译需实测一次**）。**该义务是通用规范，唯一载体是用户级 `unreal-gameplay-tags` 技能（`governance.md` 检查项 A1/A4）；本提案只在 `tasks.md` 第 3 节交付这一次的 21 处补齐，MUST NOT 在本插件规格里复述该通用义务。**
- **11 个既有能力 MODIFIED**（字面 tag 文本同步 + 各自点明新根）：`damage-flow`、`damage-step-library`、`damage-primitive`、`attribute-types`、`attribute-store`、`attribute-pipeline`、`effect-chain`、`effect-chain-asset`、`effect-interpreter`、`effect-trigger`、`param-value`。
- **新增 2 个根（按 `FGameplayTag` 槽位清点后定名，2026-10-01；6 根 → 8 根）**：
  - `EffectTriggerGate`（角色 = 触发行的行级开关；唯一消费者 = `SetTriggerGateTag` / `IsTriggerGateTagLit`，即四道门第三道；声明方 = 宿主 ini）；
  - `DamageCategory`（角色 = **供条件匹配的伤害分类集**；**两个匹配面共用一套词**——触发侧 `FTcsTriggerCondition_HasAllTags` + 流程侧 `FTcsConditionHasAllTags`；声明方 = 宿主 ini，词由宿主 `ResolveElement` 决定）。
- **两处"归回既有角色"的裁定（第二/三轮消费者清点，2026-10-01）**：`FormulaParams` 的键归 `TcsStateParam`（三层值空间的第 2 层由第 1 层传入，同词汇）；`EffectChainRunVar` 的命名契约落点移到 `effect-interpreter`（应用点跟消费者 API 走，`effect-chain` 只留指针）。
- **`openspec/project.md` 的 tag 段落缩为指针**——tag 命名/归属/常量名规则不再住 `project.md`（同一主题只允许一个载体），改为指向新能力 `gameplay-tag-governance`；`project.md` 保留与 tag **无关**的条目（identifier = `FGameplayTag` 标准、跨模块注册与反射可见性、字段默认值陷阱、模块词汇边界）。
- **`R-3` 死常量清理并入本批（2026-10-01 用户批准）**——`Source/TcsDamage/Private/Chain/TcsStepDamage.cpp:17-18` 的 `const FName TcsChainDamage_BaseKey(TEXT("BaseDamage"));`（零使用、`FName` → `FGameplayTag` 改造的残留）随本批一并删除。**并入理由**：与本批既有的 `TcsFlowStepsRest.cpp` 欠账是**同一类"`FName` 时代死常量"清理**，共用一次编译验证即可；且它住在键语义最敏感的文件里，留着会诱导后来者写裸字面量。落点见审计报告 `RSCH-boundary-audit` §4 的 `R-3`。（**注意**：审计报告的 `R-1` / `R-2` **不在本批**——`R-2` 已并入「伤害模块边界整肃」提案。）
- **明确不做（本提案边界）**：
  - **不改任何 C++ / ini**——本提案只产出提案与规格 delta，实现（含 21 处改名与 `DevComment`）是 `tasks.md` 的事；
  - **不删 `FTcsDamageRecord::Element` 字段**——该字段的删除，连同记录里 `bHit` / `bCrit` / `Absorbed` / `bKill` 的玩法字段族整肃，**移交**独立的「伤害模块边界整肃」提案（S1–S7）。本提案保持**纯词表变更**。**移交理由（机械故障，非风格问题）**：两条提案若都 MODIFY `damage-primitive` 的 `### Requirement: 伤害记录与事件`，后归档的那个会用自己的粘贴全文覆盖前一个，**静默回滚其中一侧的改动**；且 `Element` 与 S1 的目标是同一个 struct、同一行字段枚举（`openspec/specs/damage-primitive/spec.md:99`），拆开写必然重叠；
  - **不动 `openspec/specs/` 下的生效规格**（那由 `openspec archive` 合入）；
  - **不改宿主仓（LAC）与 TGFS 仓**——宿主侧 8 个正式词换根、16 个 `Probe` 词删除、redirect 登记，以及 TGFS 自己的根改名，都归各自仓的提案（见「跨仓交办」）；
  - 不引入受限 tag（原 B5 方案**整条撤销**，见 `design.md`）、不新增 `Probe` 根、不重排任何模块依赖。

## Impact

- **Affected specs**：
  - `gameplay-tag-governance`（**ADDED**：7 条需求——根段注册表 / 词归属规则 / 同一根共享 / 一角色一根 / 根名判据 / 词表深度上限 / `TcsStateParam` 语义边界。**`DevComment` 义务不在其列**——通用规范住用户级技能，本批只交付 21 处补齐，见 `tasks.md` 第 3 节）
  - `damage-flow`（MODIFIED × 4：流程模板与登记表 / **流程上下文（三层值空间）** / 流程属性黑板 / 收集事件协议）
  - `damage-step-library`（MODIFIED × 2：标准步骤库（十步） / 步骤 Conditions 挂点——`DamageCategory` 的流程侧匹配面）
  - `damage-primitive`（MODIFIED × 5：Damage 链步骤与执行器 / **伤害记录与事件（仅同步 tag 文本与根归属）** / ModifyFlow 链原语 / 消耗策略为纯数据可反射结构 / 收集事件载荷读取器登记——`DamageCategory` 的两个匹配面之桥）
  - `effect-trigger`（MODIFIED × 2：触发条件最小集——`DamageCategory` 的触发侧匹配面 / 触发行登记表与订阅生命周期——`EffectTriggerGate` 的唯一消费者）
  - `effect-interpreter`（MODIFIED × 1：门面反射面（脚本层可达）——`EffectChainRunVar` 的命名契约落此）
  - `attribute-types`（MODIFIED × 4：边界三态与值域模式 / 属性值参数源 / 属性身份 = GameplayTag / 属性定义（双轨制 + tag 身份））
  - `attribute-store`（MODIFIED × 1：属性定义表与单位侧添加移除）
  - `attribute-pipeline`（MODIFIED × 1：变更广播）
  - `effect-chain`（MODIFIED × 3：链与步骤数据形状 / 链定义登记表 / 效果链上下文（黑板））
  - `effect-chain-asset`（MODIFIED × 1：效果链资产）
  - `param-value`（MODIFIED × 1：值来源策略基类与内置源）
- **Affected code**（本提案**不改**，实现归 `tasks.md`；`Source/` 内的字面 tag 面已核实为 3 个 `.cpp` 定义点 + 4 个公开头导出声明）：
  - `Source/TcsDamage/Private/TcsDamageSubsystem.cpp`（10 个事件词）
  - `Source/TcsDamage/Private/Flow/TcsFlowKeys.cpp`（9 个黑板键 + 1 个模板词）
  - `Source/TcsAttribute/Private/Attribute/TcsAttributeChangedEvent.cpp`（1 个事件词）
  - 公开头导出声明：`TcsDamageFlowCollectEvent.h` / `TcsDamageRecord.h` / `TcsFlowKeys.h` / `TcsAttributeChangedEvent.h`
  - **`R-3` 死常量清理面（2026-10-01 并入）**：`Source/TcsDamage/Private/Chain/TcsStepDamage.cpp:17-18`（删零使用的 `TcsChainDamage_BaseKey`，见「What Changes」末条）；同批的 `TcsFlowStepsRest.cpp` 既有欠账见 `tasks.md` 第 2.6 项
  - **`Element` 字段删除面（已移交，不在本提案）**：`Source/TcsDamage/Public/Flow/TcsDamageRecord.h:56`（声明）+ `Source/TcsDamage/Private/Flow/Steps/TcsFlowStepsCore.cpp:240`（唯一写入点，且是置空）——**两处动作归「伤害模块边界整肃」提案**，本提案只登记落点。其余 `Element` 命中全是**步骤名**（`FTcsFlowElement`）/ **委托方法**（`ResolveElement`）/ **事件名**（`TcsEvent.Damage.Element`），不受影响；宿主 LAC `Source/` + `Script/` 对该字段**零引用**（已核）
- **Affected docs**：`openspec/project.md`（tag 段落 → 指针）；`Documents/combat-system-design/` 下 15 份含字面 tag 的设计/计划/日志文档（**连带面已登记，本提案不改**，见 `tasks.md` 第 6 节）；`damage-record-inspection` 规格已扫——其记录字段清单**不含 `Element`**，与本次换根**无交集**（该字段的删除移交「伤害模块边界整肃」后，该规格仍无需改动）；文档/代码偏差（"来源标签启动写入"）登记为 `tasks.md` 第 6.4 节待裁决项
- **跨仓交办**（不在本仓交付）：
  - **LAC 仓**：8 个正式 ini 词换根 + 16 个 `Probe` 词删除 + `Source/TcsDev/`、`Script/LegendAutoChessCS/TcsProbe/` 硬编码串 + 2 个 `Content/TcsDev/*.uasset` + 宿主侧根段注册表（`Attribute` / `Probe` / `TgfsEvent` / `GameFlow*`）；**29 条 `FGameplayTagRedirect` 全部登记在 LAC 的 `Config/DefaultGameplayTags.ini`**（本仓无该文件，插件也无法自带——原生词没有 TagList 源，redirect 走 `UGameplayTagsSettings` 回落分支）。
  - **TGFS 仓**：改其提案的 spec 文本（`Flow.Phase.*` → `GameFlowPhase.*`、`Flow.Completer.*` → `GameFlowCompleter.*`、`Flow.Event.*` → `TgfsEvent.*`、`Tgfs.FlowEvent.Phase.*` → `TgfsEvent.Phase.*`）。
  - 顺序：**插件 → TGFS → LAC**（契约修订便宜且唯一决定 LAC 那批改动的目标形态；TGFS 提案未开工，纯文本改动）。

## 关键决策点

| 决策 | 结论 | 依据摘要 |
|---|---|---|
| 根段判据 | **根 = 消费角色**（哪条代码路径解析它），不是业务系统、不是 owner 前缀 | 规范本体；现行 `Tcs` 前缀形态是既有违反项 |
| 一角色一根 | `DamageFlowKey` 与 `DamageFlowTemplate` 各自成根，MUST NOT 用 facet 段兼收 | 两条不同代码路径 = 两个角色 |
| 深度 | 上限仍 4 段，**第 4 段作余量**；细分走**增根不增层** | 换根后 8 根全部 ≤ 3 段，三族零余量问题消失 |
| `TcsStateParam` 命名 | "State" 取广义（含技能激活运行态），**不是 `TcsState` 模块** | 技能运行态归状态；`TcsState`（`FStateInstance`）与 `TcsSkill`（`FCastRun`）是两个运行身份，但同属"参数表读取"这一个消费角色 |
| 原 B5（受限 tag 强制"一处声明"） | **整条撤销** | re-root 后 `DamageFlowKey` 是共享根，既有口径"契约键原生声明；项目自定义键自由"直接成立，原冲突自然消解 |
| 载体与拆分 | 三仓各开一个提案；**根段注册表两边各登自己那一半** | 插件仓登 6 个 `Tcs*` 系根；宿主侧 5 根（`Attribute` / `Probe` / `TgfsEvent` / `GameFlowPhase` / `GameFlowCompleter`）登 LAC 侧 |
| 引擎机制规则 | **不复制进本规格** | "声明位置四分类 / 改名与重定向 / 段命名风格 / `DevComment` 写法细节"住跨项目的 `unreal-gameplay-tags` 技能；本能力只承载**插件侧**契约 |

## 检查点（人工验证点，实现阶段执行）

1. **编译**：Development 与 Shipping 双配置零错误；**中文 `DevComment` 过编译**（宏第三实参经 `TEXT(...)` 包裹，本批首次实测）。
2. **tag 树形态**：编辑器 Project Settings → GameplayTags 下 **8 个根**就位，`TcsEvent.*` / `DamageFlowKey.*` / `DamageFlowTemplate.*` 三族**无 4 段词**（逐族数段数）。
3. **重定向生效**：把旧名 `Tcs.Event.Damage.Hit` 配进一个 `FGameplayTag` 属性/资产字段后加载，解析到新名 `TcsEvent.Damage.Hit`（redirect 由 LAC 侧登记后验证；同一 `OldTagName` 只出现一条、直接写终态不链式）。
4. **宿主侧竖切跑通**：LAC 侧 R3 竖切（`L_TcsDev_Slice` 地图）在重定向 + 宿主词换根后行为与改造前逐项一致。
5. **校验脚本**：`-Roots` 清单模式（多根白名单）跑通，`-Namespace Tcs` 的旧形态检查不再适用。
6. **提案校验**：`openspec validate reroot-gameplay-tag-vocabulary --strict --no-interactive` 通过。
