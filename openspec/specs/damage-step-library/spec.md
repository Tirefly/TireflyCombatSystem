# damage-step-library Specification

## Purpose
定义伤害流程的标准步骤库（十步）、步骤 Conditions 挂点、通用数据步骤，以及官方默认流程模板。

## Requirements

### Requirement: 标准步骤库（十步）

> **基准说明**：本条 delta 以 **`reroot-gameplay-tag-vocabulary` 换根后的文本**为基准撰写（契约键根一律用换根后的 `DamageFlowKey`），依赖该提案**先归档**——理由见 `proposal.md`「跨提案依赖」。

`TcsDamage` MUST 提供十阶段标准步骤库（09 §2.2；D7-5"流程 = 数据模板 + 标准件"），全部为**数据 struct**（无公共基类，D4-16），MUST 经 `UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册。

**执行期行为契约**：正路共用的黑板键名（`BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`）属**标准步骤库的契约**（M8 校验与 Explain 认识）——**2026-09-22 改造：这些契约键从"自由 FName"改为"插件原生声明的 GameplayTag"**；**2026-10-01 换根：根 = `DamageFlowKey`**，形态 `DamageFlowKey.<键>`，常量名逐点换下划线（如 `DamageFlowKey.BaseDamage` → `Tag_DamageFlowKey_BaseDamage`）；**项目自定义键仍自由**，但类型同为 `FGameplayTag`、由**宿主 ini** 声明，且与契约键**同根**（`DamageFlowKey` 是共享根——根段注册表与归属规则见 `gameplay-tag-governance` 能力）。步骤 MUST NOT 自行推导基础伤害（零公式纪律）。

**契约键原生声明的理由**（与事件 tag 同款判据"谁拥有那个词，谁声明"）：契约键是**步骤之间的接口**，属**框架词汇**——若让宿主声明，宿主漏配即导致上下游步骤对不上（**静默读到 0**），框架契约被宿主配置破坏。故 MUST 插件原生声明（随模块加载生效、零宿主配置依赖）。

**MUST NOT 再用裸字面量**：现状中 `TcsFlowStepsRest.cpp` 的 `FName(TEXT("Crit"))` / `FName(TEXT("Hit"))` 等内联字面量 MUST 改为引用原生 tag 常量（消除跨文件裸字面量耦合）；`TcsFlowStepsRest.cpp` 中声明后从未使用的死常量（`TcsFlowRestKey_HitRate` / `TcsFlowRestKey_CritRate`）MUST 一并清理或改为 tag 常量并实际使用。

**宿主判定语义的键（登记，MUST NOT 删除）**：`Hit` / `Crit` / `Kill` 三键虽以契约键形态原生声明，其**语义承载的是宿主玩法判定**（命中 / 暴击 / 致死）。它们仍是**步骤之间的接口**（框架词汇），故 **MUST NOT 因"名字是宿主概念"而删除**——删键即断掉步骤之间的接口。三键的归属说明 MUST 在契约文档中登记。

**值域纪律（`FTcsFlowExecute`，MUST）**：扣血步骤 **MUST NOT 自设值域下限**——写入属性时 MUST 直接写 `当前值 - 执行量`，**值域收口 MUST 由属性层的宿主配置承担**（`ETcsAttributeValueDomain` + 边界三态；`ABM_None` 是默认 = 该侧不设边界）。
**理由（判据 = 契约覆盖，比"框架内定"更重的一档）**：属性层只在宿主**显式配了边界**时才钳制，即"过量伤害记为负数"本是宿主可表达的语义；步骤内硬编码 `FMath::Max(0.0, ·)` 会**抢在宿主已有的配置点之前**下结论，形成"框架与宿主各有一套值域结论"的双真相。**迁移代价**：删除该钳制后，宿主 MUST 给生命属性配 `Min` 边界，否则过量伤害会把生命记为负值。

**吸收映射纪律（`FTcsFlowExecute`，MUST）**："吸收 → 执行量"的映射 **MUST NOT 硬编码在步骤内**，MUST 经 `ITcsDamageFlowDelegate::ResolveExecutedDamage` 取（中性默认 = `FMath::Max(0.0, 候选 - 吸收)`，即现状行为）。
**理由**：其中的**零下限**只引用数值、方向收敛到"不介入"，属合法**契约默认**；但**减法是框架内定的模型**——宿主只能供吸收量、不能换模型 ⇒ 缺少覆盖点即不满足契约默认定义（见 `damage-primitive`「流程委托契约（宿主实现）」）。

**致死判定纪律（`FTcsFlowExecute`，MUST）**：致死线 **MUST NOT 硬编码在步骤内**，MUST 经 `ITcsDamageFlowDelegate::IsLethal` 取（中性默认 = 当前值 `<= 0.0`）；框架只据此**记账** `Kill`，MUST NOT 杀实体。

**命中结果纪律（`FTcsFlowCompleted`，MUST）**：填充记录的 `bHit` **MUST** 从契约键 `DamageFlowKey.Hit` 读回（与 `bCrit` / `Absorbed` 同款写法），**MUST NOT** 硬编码为 `true`。框架不替宿主断言命中。

#### Scenario: 契约键跨文件一致

- **WHEN** 生产者步骤与消费者步骤分别在不同 cpp 里读写同一契约键
- **THEN** 二者引用**同一个原生 tag 常量**（编译期保证一致，不存在"两处字面量各写一遍"的失配可能）

#### Scenario: 项目自定义键仍自由

- **WHEN** 宿主侧步骤要用一个契约键之外的键
- **THEN** 该宿主 tag 由宿主 ini 在 `DamageFlowKey` 根下声明后即可用（插件不预设、不校验其存在性——归 M8 校验矩阵）

#### Scenario: 值域由宿主配置决定而非步骤硬编码

- **WHEN** 宿主给生命属性配了 `Min = 0` 边界，且本次执行量大于目标当前生命
- **THEN** 写入结果为 0（由**属性层**钳制）；**不配 `Min` 时写入结果可以为负**——两种结果都只由宿主配置决定，步骤内部无任何下限钳制

#### Scenario: 吸收映射可被宿主替换

- **WHEN** 宿主实现 `ResolveExecutedDamage` 返回非减法的映射结果
- **THEN** `Execute` 步采用该结果作为本次执行量，MUST NOT 回落为框架内置的减法

#### Scenario: 致死线可被宿主替换

- **WHEN** 宿主实现 `IsLethal` 返回自定义判定
- **THEN** `Kill` 键与记录里的 `bKill` 依据该判定；框架不杀实体

#### Scenario: 命中结果读契约键

- **WHEN** `FTcsFlowCompleted` 填充记录，而 `DamageFlowKey.Hit` 无提交
- **THEN** `bHit = false`（折叠初值 0）；MUST NOT 写入恒 `true`

#### Scenario: 宿主判定语义的键不被误删

- **WHEN** 审查 `Hit` / `Crit` / `Kill` 三键是否因"名字是宿主概念"被判越界
- **THEN** 三者**保留**——它们是步骤之间的接口（框架词汇），归属说明已登记；删除会断掉步骤间接口

### Requirement: 步骤 Conditions 挂点

流程步骤 MUST 支持"条件不过则跳过"（09 §2.2 的**每步骤可带 Conditions**）：

- 每个步骤 struct MUST 持**同名字段** `TArray<FInstancedStruct> Conditions`（元素为**纯数据谓词** struct——非策略对象，故不适用策略形态一元化）；
- 共享求值助手 `ShouldRunFlowStep(const TArray<FInstancedStruct>& Conditions, const FTcsDamageFlowContext& Context) -> bool`（住 `Public/Flow/TcsFlowStepConditions.h`）；**每个步骤执行器 MUST 首行调用**（步骤无公共基类（D4-16），故为纪律而非基类钩子）；
- 不过 → **跳过该步并继续流程**（返回 true，不是中止）；未知条件类型 → 视为不过 + Warning 日志（不静默通过）；
- R3 实现两个求值器：`FTcsConditionHasAllTags{ TArray<FGameplayTag> Tags }`（对 `ClassificationTags` 判定）与 `FTcsConditionChance{ double Probability }`（**概率型条件显式声明随机源**：宿主未注入随机源时退化为确定性判定，保证可复现——D0-1 的例外必须显式）；
- **分类集词汇的根（2026-10-01 换根）**：`FTcsConditionHasAllTags::Tags` 里的词 MUST 落在 **`DamageCategory`** 根下——消费角色 = **供条件匹配的伤害分类集**；声明方 = 宿主 `Config/DefaultGameplayTags.ini`（词由宿主的 `ITcsDamageFlowDelegate::Execute_ResolveElement` 实现决定）；形态 `DamageCategory.<词>`（2 段）。**该根由流程侧与触发侧两个匹配面共用**（触发侧 = `effect-trigger` 的 `FTcsTriggerCondition_HasAllTags`），MUST NOT 为两侧各立一根——两处只是**同一套词的两个读取点**，词的归属只有一份（写链与判据边界见 `gameplay-tag-governance` 的根段注册表备注与「一角色一根」）。插件 MUST NOT 声明任何具体分类词。

#### Scenario: 条件不过即跳过

- **WHEN** 某步骤的 `Conditions` 含 `FTcsConditionHasAllTags{Tags=[火]}` 而上下文的 `ClassificationTags` 不含火
- **THEN** 该步不执行、流程继续走下一步

#### Scenario: 未知名条件不静默通过

- **WHEN** `Conditions` 含没有求值器的 struct 类型
- **THEN** 该步被跳过 + Warning 日志（不静默通过、不崩溃）

#### Scenario: 分类词与触发侧匹配面同根

- **WHEN** 检查 `FTcsConditionHasAllTags{Tags=[…]}` 里的词与触发行条件 `FTcsTriggerCondition_HasAllTags` 里的词
- **THEN** 二者同住 `DamageCategory` 根（同一套宿主分类词的两个读取点）；插件 MUST NOT 为流程侧/触发侧各立一根，也不声明任何具体分类词

### Requirement: 通用数据步骤

`TcsDamage` MUST 提供两个**数据化**步骤（把"宿主挂点"变成可配置数据；09 §2.2 的两处）：

- `FTcsFlowModify{ FGameplayTag TargetKey; ETcsAttributeOp Op; FTcsParamValue Operand; TArray<FInstancedStruct> Conditions; }`——数据化黑板写入（"破甲阶段" = 一个数据步骤；**2026-09-22 改造：`TargetKey` 类型 `FName` → `FGameplayTag`**）；Operand 为 `FTcsParamValue`（黑板键引用保留为流程域自身的 Operand 选项，随其来源策略轮落地）。**MUST NOT 携带消耗策略**——**理由于 2026-09-30 改写**：原理由为"`FTcsConsumePolicy` 含 `TFunction OnConsumed` 回调（纯 C++、无 `USTRUCT` 宏、不可作 `UPROPERTY`）"，该技术事实已由本批的「消耗策略为纯数据可反射结构」需求消除（`OnConsumed` 改事件语义）；**禁令本身不变，理由改为职责划分**——数据步骤只做**纯数值写入**，消耗型提交属"修改器通道"语义（`ModifyFlow` 链原语与 C++ 步骤的职责；见 `damage-primitive` 的「ModifyFlow 链原语」需求）；
- `FTcsFlowDelegate{ FGameplayTag TargetKey; TScriptInterface<UTcsDamageFlowDelegate> Delegate; ... }`——数据化委托调用（轻量公式挂法；**2026-09-22 改造：`TargetKey` 类型改 tag**）。

#### Scenario: 数据步骤可配可跑

- **WHEN** 模板里放一个 `FTcsFlowModify`（TargetKey = 某 tag、Op = `TAO_Add`、Operand = Literal 5）
- **THEN** 该步执行后黑板对应键多出 5 的贡献（无需任何 C++ 步骤）

### Requirement: 官方默认模板

`TcsDamage` MUST 在门面初始化时组装并登记**官方默认模板**（D7-5：标准十阶段只是插件自带的标准件，**可整表替换**）：

- 内容：`CollectStart → BaseDamage → Execute → Completed`；
- 标识：以约定 `TemplateId`（`Default`）登记；`FTcsStepDamage` 的 `FlowTemplateId` 为空时 MUST 解析到它；
- 其余六步（`PreHit` / `Hit` / `Crit` / `Element` / `AfterDamage` / `PreExecute`）的 struct 与执行器同批提供，但 **R3 不组装进默认模板**；
- 模板为**数据**，宿主的自定义模板可整表替换默认模板的行为（登记同 id 时以宿主显式登记为准——门面 MUST NOT 拒绝宿主覆盖 `Default`？**否**：按既有口径重复登记一律拒绝；宿主以自己的 id 登记并在链步骤里指定）。

#### Scenario: 空模板 id 落到默认模板

- **WHEN** `FTcsStepDamage.FlowTemplateId` 为空
- **THEN** 执行器解析到官方默认模板并跑通四步

#### Scenario: 标准件可替换

- **WHEN** 宿主登记自己的 `TemplateId` 并在链步骤里指定
- **THEN** 流程按宿主的模板执行（官方默认模板不受影响，仍可被其他链使用）
