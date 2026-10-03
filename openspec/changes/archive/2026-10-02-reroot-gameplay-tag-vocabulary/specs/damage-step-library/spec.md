## MODIFIED Requirements

### Requirement: 标准步骤库（十步）

`TcsDamage` MUST 提供十阶段标准步骤库（09 §2.2；D7-5"流程 = 数据模板 + 标准件"），全部为**数据 struct**（无公共基类，D4-16），MUST 经 `UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册。

**执行期行为契约**：正路共用的黑板键名（`BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`）属**标准步骤库的契约**（M8 校验与 Explain 认识）——**2026-09-22 改造：这些契约键从"自由 FName"改为"插件原生声明的 GameplayTag"**；**2026-10-01 换根：根 = `DamageFlowKey`**，形态 `DamageFlowKey.<键>`，常量名逐点换下划线（如 `DamageFlowKey.BaseDamage` → `Tag_DamageFlowKey_BaseDamage`）；**项目自定义键仍自由**，但类型同为 `FGameplayTag`、由**宿主 ini** 声明，且与契约键**同根**（`DamageFlowKey` 是共享根——根段注册表与归属规则见 `gameplay-tag-governance` 能力）。步骤 MUST NOT 自行推导基础伤害（零公式纪律）。

**契约键原生声明的理由**（与事件 tag 同款判据"谁拥有那个词，谁声明"）：契约键是**步骤之间的接口**，属**框架词汇**——若让宿主声明，宿主漏配即导致上下游步骤对不上（**静默读到 0**），框架契约被宿主配置破坏。故 MUST 插件原生声明（随模块加载生效、零宿主配置依赖）。

**MUST NOT 再用裸字面量**：现状中 `TcsFlowStepsRest.cpp` 的 `FName(TEXT("Crit"))` / `FName(TEXT("Hit"))` 等内联字面量 MUST 改为引用原生 tag 常量（消除跨文件裸字面量耦合）；`TcsFlowStepsRest.cpp` 中声明后从未使用的死常量（`TcsFlowRestKey_HitRate` / `TcsFlowRestKey_CritRate`）MUST 一并清理或改为 tag 常量并实际使用。

#### Scenario: 契约键跨文件一致

- **WHEN** 生产者步骤与消费者步骤分别在不同 cpp 里读写同一契约键
- **THEN** 二者引用**同一个原生 tag 常量**（编译期保证一致，不存在"两处字面量各写一遍"的失配可能）

#### Scenario: 项目自定义键仍自由

- **WHEN** 宿主侧步骤要用一个契约键之外的键
- **THEN** 该宿主 tag 由宿主 ini 在 `DamageFlowKey` 根下声明后即可用（插件不预设、不校验其存在性——归 M8 校验矩阵）

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
