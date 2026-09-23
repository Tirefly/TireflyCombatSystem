## ADDED Requirements

### Requirement: 触发行数据形状

`TcsEffect` MUST 以**数据行**承载"事件 → 效果链"的映射（D4-1 终版 10 字段；行为由共享 Handler 承载，事件 struct 上不自绑 delegate——裁决 2a）：

- `FTcsTriggerRow`（USTRUCT）字段：`EventTag: FGameplayTag`（订阅哪个事件——Tag 路由）/ `EventPayloadFilter: FInstancedStruct`（载荷预筛，先于条件求值）/ `Conditions: TArray<FInstancedStruct>`（门禁条件）/ `Effects: FGameplayTag`（触发后执行的链 id，引用已登记链）/ `Priority: int32`（同 Tag 多行触发顺序，**大者先**）/ `ExecutionGate: ETcsExecutionGate`（执行闸——网络姿态挂点）/ `InterruptPriority: int32`（可打断哪些正在跑的链）/ `GateTags: TArray<FGameplayTag>`（行级开关 Tag，运行时点灯控制整行）/ `Cues: TArray<FGameplayTag>`（CueId 引用列表，帧末通道）/ `bConditionMissIsSilent: bool`（条件未过是否静默）；
- **`Source: FTcsSourceHandle`** 是**登记簿记**（不属 D4-1 的 10 个触发语义字段）：作为级联退订锚点，支撑"来源注销自动退订"（D4-1 明文）；
- 全部字段**独立可空**、零策略类（D4-1："12 字段各自独立可空，空 = 默认行为"）；`Scope` / `HandlerClass` **MUST NOT** 存在（D4-1 v2 砍除：目标归属由 `SelectTargets` 步骤或上下文默认目标表达；求值器本身就是全部行的共享 Handler）；
- `ETcsExecutionGate`（UENUM）MUST 至少含 `TEG_Always = 0`（恒通过）；**R4 只实现该值**，其余取值（如仅权威侧）随网络姿态轮——枚举值 MUST NOT 以"末位追加"以外的顺序变化（Custom 逃逸位规约：值 0 = 默认）。

#### Scenario: 触发行字段集完整且独立可空

- **WHEN** 构造一个仅填 `EventTag` 与 `Effects` 的 `FTcsTriggerRow`
- **THEN** 其余字段取默认值（空 tag / 空数组 / `Priority = 0` / `ExecutionGate = TEG_Always` / `bConditionMissIsSilent = true`），行合法

#### Scenario: 行携带级联退订锚点

- **WHEN** 同一来源登记多行触发行
- **THEN** 各行的 `Source` 相等，可按该句柄一次性全量摘除（与 M2 `RemoveBySource` 同款语义）

#### Scenario: 不含已砍除字段

- **WHEN** 检查 `FTcsTriggerRow` 的字段集
- **THEN** 不含 `Scope` 与 `HandlerClass`（D4-1 v2 砍除——目标归属归步骤，求值器即共享 Handler）

### Requirement: 触发条件最小集

`TcsEffect` MUST 以**纯数据谓词**承载触发条件（D4-5 条件最小集；与流程步骤侧同款形态——条件不是策略对象，不用策略基类）：

- `FTcsTriggerCondition_HasAllTags`：`Tags: TArray<FGameplayTag>`——触发上下文分类 Tag 集须含**全部**给定 Tag（空数组 = 无条件通过）；
- `FTcsTriggerCondition_Chance`：`Probability: double`（[0,1]）——**随机值由调用方注入**（D0-1 确定性纪律：求值内部 MUST NOT 取随机数，否则同输入不同输出、回放失效）；
- `FTcsTriggerContext`（USTRUCT，**触发期最小上下文**）：`EventTag` / `ClassificationTags` / `Caster`——只含条件求值真正需要的字段；
- `EvaluateTriggerConditions(Conditions, Context, RandomValue = 0.0) -> bool`：全部条件通过 → `true`；任一不过 → `false`；**未知条件类型 → 视为不过 + Warning 日志**（MUST NOT 静默通过——静默会让"条件写错"表现成"条件通过"）；
- 条件类型之间**无公共基类**（D4-16）：求值走"**同名字段 + 各求值点首行调用助手**"的纪律，与 `damage-step-library` 的 `ShouldRunFlowStep` 同款。

**与流程侧条件的分工（MUST NOT 混用）**：`TcsDamage` 的 `FTcsConditionHasAllTags` / `FTcsConditionChance` 服务于**流程步骤**（上下文 `FTcsDamageFlowContext`）；本能力的 `FTcsTriggerCondition_*` 服务于**触发行**（上下文 `FTcsTriggerContext`）。`TcsEffect` MUST NOT 依赖 `TcsDamage`（依赖铁律 `Core←Attribute←Effect←{Damage,…}`），故两侧各自持有条件类型。

#### Scenario: 全部条件通过才触发

- **WHEN** 一行的条件为 `[HasAllTags([A, B]), Chance(1.0)]`，上下文分类 Tag 集含 A 与 B，注入随机值 0.5
- **THEN** 求值返回 `true`

#### Scenario: 任一条件不过即不触发

- **WHEN** 条件为 `[HasAllTags([A, B])]` 而上下文只含 A
- **THEN** 求值返回 `false`（**不求值后续条件**——短路）

#### Scenario: 未知条件类型不静默通过

- **WHEN** 条件数组含一个未注册的 struct 类型
- **THEN** 求值返回 `false` + 留 Warning 日志（含类型名）

#### Scenario: 概率条件依赖注入的随机值

- **WHEN** `Chance(0.5)` 分别以注入随机值 0.4 与 0.6 求值
- **THEN** 前者通过、后者不过——**同一输入恒得同一结果**（求值内部不取随机数）
