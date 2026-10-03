## MODIFIED Requirements
### Requirement: SelectTargets 步骤与执行器

`TcsTargeting` MUST 提供链步骤 `FTcsStepSelectTargets`（`Public/Chain/TcsStepSelectTargets.h`）：字段 `FInstancedStruct Selector`（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetSelectorStrategy")`）+ `TArray<FInstancedStruct> Filters`（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetFilterStrategy")`）——**2026-09-24 换型**（原为 `TInstancedStruct<T>`，在脚本层导出为空壳导致脚本拼不出本步骤）。

- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **跨模块自注册**（TcsTargeting → TcsEffect 注册表，机制层零改动）；
- 执行序：**清空** `Context.Targets` → `Selector->Resolve(Context, EntityQuery, Candidates)`（`EntityQuery` 取自 `UTcsEffectSubsystem::GetEntityQuery()`，经运行态 `Run.Owner`）→ 逐候选过 `Filters`（AND + 短路）→ **去重** → **排序** → **取前 K** → 写回 `Context.Targets`；
- **MUST NOT 做存活过滤**：候选的有效性由宿主 Filter 表达（框架不认识"存活"）；选择器产出的失效句柄由 Filter 淘汰，无 Filter 时原样传递；
- **即时步骤**：恒返回 `TSR_Completed`；
- **未配置 Selector**：保持 `Context.Targets` 原样 + Warning（不静默清空、不 ensure）；
- **取用纪律（换型连带）**：`Selector` / `Filters` 元素的取用 MUST 走 `GetPtr<T>()` + **判空**（裸载体的编译期限定已移除，运行期 `IsChildOf` 校验失败时返回 nullptr）。

**排序与收束（2026-10-03 新增，P-A 排序相位）**：

- 新增字段 `TArray<FTcsTargetSortItem> SortItems`——**有序比较项数组**（每个元素 = 一个「**评分器 + 方向**」组合，形状见下一条需求）；**空数组 = 保持既有"保序"行为**（见下方例外条件）；
- 新增字段 `int32 MaxCount = 0`——按**完整字典序**取前 K；`0` = 不限（负值 MUST 按 0 处理）。**MUST NOT** 理解成"按某一个排序项截断"（那会与严格字典序打架）；
- **排序项为空时 MUST 保持既有的"保序"行为**（通过者按选择器产出顺序写回，去重后）——本条是本需求改写"保序"措辞的**唯一例外条件**，其余情况（有排序项）一律按字典序重排；
- **候选去重（本轮补掉的无人防守缺口）**：通过过滤的候选集 MUST 按键 **= 实体句柄身份**去重，重复项只保留一份；去重 MUST 发生在**过滤之后、取前 K 之前**（放在取 K 之后会让结果数少于 K，那正是本条的防守目标）；
  - **去重是执行器的责任，MUST NOT 转嫁给选择器**——`FTcsTargetSelectorStrategy::Resolve` 的契约一字未改（它仍只需"只填充不清空"），宿主自定义选择器**不被要求**自己去重；
  - 去重键是**候选的完整身份** ⇒ 重复项彼此**不可区分** ⇒ "保留先出现的还是后出现的"无观测差异；与"去重要在资格与评分之后"这一外部经验的**等价性论证**见 `design.md`；
- **`MaxCount` 生效于排序之后**：结果集 = 去重后按字典序排序的前 `MaxCount` 个（`MaxCount` 为 0 时取全部）。

#### Scenario: 未配置选择器时目标集原样

- **WHEN** 步骤的 `Selector` 为空载体（未配置）
- **THEN** `Context.Targets` 保持原样，留 Warning（不静默清空）

#### Scenario: 过滤按 AND 且保序

- **WHEN** 配两个 Filter（都通过 / 一过一不过）与多个候选，且**未配任何排序项**
- **THEN** 只有全通过的候选留下，且保持选择器产出的顺序

#### Scenario: 配了排序项则按字典序重排

- **WHEN** 同一批候选配了排序项（如"距离升序"）
- **THEN** 写回顺序为排序结果，**MUST NOT** 再保持选择器产出顺序（"保序"仅在无排序项时成立）

#### Scenario: 严格字典序与逐项方向

- **WHEN** 配两个排序项（第一项升序、第二项降序）与三个候选：首个候选项上分出胜负
- **THEN** 第二排序项**只在第一项相等时**参与裁定；每项方向独立生效

#### Scenario: 全等后按稳定键决胜且可复现

- **WHEN** 两个候选在**全部**排序项上取值完全相等
- **THEN** 按实体句柄 `Id` **升序**决胜；同一输入连跑两次结果集**逐位一致**

#### Scenario: 去重后同一目标只出现一次

- **WHEN** 选择器产出含重复句柄的候选集（如宿主两个来源重叠）
- **THEN** 写回的目标集内该句柄只出现一次（下游步骤 MUST NOT 对同一目标动手两次）

#### Scenario: 取前 K

- **WHEN** `MaxCount = 3` 而通过过滤的候选有 5 个
- **THEN** 写回的是按完整字典序排在前 3 的候选（不是"按某个排序项取前 3"，也不是任意的 3 个）

#### Scenario: MaxCount 为 0 时不截断

- **WHEN** `MaxCount = 0`（默认）
- **THEN** 全部通过过滤的候选（去重、排序后）都写回 `Context.Targets`

## ADDED Requirements
### Requirement: 评分器策略契约与排序项

`TcsTargeting` MUST 提供评分器策略基类 `FTcsTargetScorerStrategy` 与排序项载体——**这是 TCS 目标选择从"单相"（谁合法）补成"两相"（谁合法 + 谁更优）的那一相**：

- `FTcsTargetScorerStrategy`（USTRUCT 反射基类，D3-7 v3）MUST 声明
  `virtual double Score(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery) const`——
  **抽象由约定达成，MUST NOT 使用纯虚**（`= 0` / `PURE_VIRTUAL`）；基类 MUST 提供中性默认实现 + `meta = (Hidden)`（口径与两个既有策略基类完全一致：UHT 为每个 USTRUCT 无条件生成 `TCppStructOps<T>`，抽象类报 C2259；`PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 下展开为 `= 0`，会让同一份代码 Development 编不过、Shipping 能过）。**中性默认实现 = 返回 0.0**（不区分候选；"未配置"不等于"淘汰全部"）；
- 分值类型 MUST 为 **`double`**（与全插件数值口径一致，MUST NOT 用 float）；
- **注入可空**：`EntityQuery` MAY 为 `nullptr`——策略 MUST 容忍（降级 + Warning），MUST NOT 解引用空指针；**降级值 MUST 为 NaN 而非 0**（0 是合法分数，用它会把"取不到世界能力"表现成"所有候选并列第一"）；
- 排序项形状 `FTcsTargetSortItem{ FInstancedStruct Scorer; ETcsTargetSortDirection Direction = ETSD_Ascending; }`，`ETcsTargetSortDirection{ ETSD_Ascending = 0, ETSD_Descending = 1 }`（值 0 = 内置默认，与全插件枚举规约一致）；
- **排序契约**：`SortItems` 是**有序比较项数组**，按**严格字典序**逐项比较（第 N 项只在第 1..N-1 项全部相等时才参与裁定），**每一项的方向独立生效**；
- **稳定键决胜**：全部排序项取值完全相等时，MUST 按 **`FTcsCombatEntityHandle::Id` 升序**决胜——这是"并列时谁先"的唯一裁定者；
  - **依据（含一处事实更正）**：既有调研稿写"用句柄的（Index, Generation）升序"，而 `FTcsCombatEntityHandle` **只有 `int64 Id` 一个字段**、头注释明文"**本句柄无代际段——Id 单调递增且永不复用**"；`Index`/`Generation` 属 `TTcsInstanceHandle<T>`。用 `Id` 不仅可行且**更强**：永不复用 ⇒ 无"回收槽误命中"问题；
- **浮点边界**：任一排序项对某候选返回 **NaN** ⇒ 该候选 MUST 被**排除**（MUST NOT 排到末尾、MUST NOT 取任意位置）；**±Inf MUST 保留**并按其方向正常参与排序；
- **"无法评分"MUST 用 NaN 表达**：取不到位置/依赖缺失等情形 MUST 返回 NaN（= 淘汰），**MUST NOT 返回 `-Inf`**——`-Inf` 在升序查询下会变成"排最前"，是外部参照实现里实测存在的陷阱，规格明文禁止照抄；
- **资格 MUST NOT 伪装成低分**：`Scorer` 只表达"谁更优"，**MUST NOT** 用极低分表达"死亡 / 满血 / 不可选中"这类资格判定——那些归 `FTcsTargetFilterStrategy`。这条纪律 MUST 写在类型注释与规格两处；
- **插件内置面 MUST 遵守"框架零宿主词汇"**：只有**只引用框架自身契约**的评分器可内置；引用宿主属性名 / Tag 词表 / 阵营关系的一律归宿主（判据 = 该策略引用了什么词汇）。本批据此只内置一个（见下一条需求）；
- **确定性的范围（如实收窄）**：排序结果 MUST 对**同进程内的同一输入**可复现（含稳定键决胜）；**MUST NOT** 承诺跨进程复现——稳定键 `Id` 由**进程内**原子发号器分配，两台机器对同一批实体的 `Id` 不保证同序。`SPEC-06-targeting` §3 原文"客户端重算一致"的措辞随之收窄为同进程口径（依据：选择本就是权威侧执行、结果不复制，见该文档 §4）；
- **MUST NOT 依赖超越函数的精确值**：评分与比较 MUST NOT 依赖 `FMath` 超越函数（如 `Sin`/`Exp`）的跨平台位一致结果——它们在平台间不保证逐位相同。

#### Scenario: 两相管线表达"先资格后偏好"

- **WHEN** 同一批候选先过 Filters（资格）、再按 `SortItems`（偏好）排序
- **THEN** 被资格淘汰的候选**不进入**评分（不是"评了低分再排后面"）——顺序 MUST 为先过滤后评分

#### Scenario: 严格字典序逐项裁定

- **WHEN** 排序项 = [距离升序, 威胁降序]，候选 A 距离更近、候选 B 威胁更高
- **THEN** A 排在 B 前（第一项先裁定）；只有距离相等时威胁才参与

#### Scenario: NaN 候选被排除而非排末尾

- **WHEN** 三个候选中一个的排序项返回 NaN，另两个返回有限值
- **THEN** 该候选不出现在结果集里；结果集只有两个候选（MUST NOT 出现"NaN 排在最后"）

#### Scenario: 正负无穷正常参与排序

- **WHEN** 某候选的排序项返回 `+Inf`（升序配置）
- **THEN** 该候选参与排序并排在有限值之后（MUST NOT 被当作 NaN 排除）

#### Scenario: 全等后按句柄 Id 升序决胜

- **WHEN** 两个候选在每个排序项上取值完全相等，其 `Id` 分别为 7 与 3
- **THEN** `Id = 3` 的候选排在前面（升序）；同一输入重复执行结果一致

#### Scenario: 缺实体查询能力时不是"并列第一"

- **WHEN** 未注入 `ITcsEntityQuery` 而链跑到含排序项的 `SelectTargets`
- **THEN** 依赖该能力的评分器返回 NaN（候选被排除）+ Warning 日志；MUST NOT 返回 0 把全部候选变成并列

### Requirement: 内置距离评分器

`TcsTargeting` MUST 提供**且只提供这一个**框架内置评分器 `FTcsScorerDistance`（`Public/Targeting/TcsScorerDistance.h`，非 `Hidden`——它是可选实现而非抽象基类，MUST 出现在编辑器类型 picker 中）：

- **依赖面 = 只依赖 `ITcsEntityQuery::GetLocation`**（`virtual bool GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation) = 0`）——**零宿主语义**：它引用的词汇只有"实体有位置"这一框架自身契约，不认识任何属性名 / Tag 词表 / 阵营关系；
- 分数 = 候选到**施法者**的**欧氏距离**（非负）；`ETSD_Ascending` 即"最近优先"（用户 2026-09-30 裁定"先只内置一个距离最近"）；
- **分数 MUST 是可解释的真实距离**：它是可观测值（后续的诊断/预览面会展示"首个排序项分数"），MUST NOT 改用它做未声明的变换（如返回平方距离）；
- **取不到任一端位置 ⇒ MUST 返回 NaN**（该候选被排除）——包括"施法者句柄无效"与"候选无定位"两种情形；MUST NOT 返回 `-Inf`、MUST NOT 返回 0；
- `EntityQuery == nullptr` ⇒ MUST 返回 NaN + Warning（不崩溃）；
- **本批 MUST NOT 内置任何其它评分器**（"血量最低"这类需要宿主词汇的条件，走下一条需求的宿主插槽或宿主 C++ 策略）。

#### Scenario: 升序即最近优先

- **WHEN** 施法者与两个候选都有定位，距离分别为 5 与 20，排序项为"距离升序"
- **THEN** 距离 5 的候选排在前面；分数分别是 5 与 20（真实距离，非平方值）

#### Scenario: 施法者无定位时候选被排除

- **WHEN** `Context.Caster` 无效或其位置取不到
- **THEN** 全部候选的该排序项为 NaN ⇒ 结果集为空 + Warning（MUST NOT 产出"距离 0"的全并列结果）

#### Scenario: 候选无定位被淘汰

- **WHEN** 三个候选中一个取不到定位（`GetLocation` 返回 false）
- **THEN** 该候选被排除，另两个按距离升序排列

#### Scenario: 反向配置表达最远优先

- **WHEN** 同一批候选改用 `ETSD_Descending`
- **THEN** 距离最远的候选排在最前（同一评分器，方向由排序项给定）

### Requirement: 宿主脚本评分器插槽

`TcsTargeting` MUST 提供宿主评分器插槽——**让宿主用任意 UE 脚本语言（C#/AS/Luau/TS/蓝图）实现"谁更优"，零 C++ 改动**（补齐第三个插槽家族；选择器族与过滤器族已在 SCRIPT-8 落地）：

- 新增 `ITcsTargetScorerHost`（`UINTERFACE(MinimalAPI, Blueprintable)`，住 `Public/Host/TcsTargetScorerHost.h`），MUST 提供：
  - `UFUNCTION(BlueprintNativeEvent) double ScoreTarget(FTcsCombatEntityHandle Candidate, FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator)`——**纯评分**（MUST NOT 改写候选或上下文）；形参与既有过滤器插槽同款（**传句柄、不传上下文**）；
- 新增转发策略 `FTcsScorerHostDelegate : FTcsTargetScorerStrategy`（USTRUCT，可被编辑器 picker 选中）：持 `UPROPERTY TScriptInterface<ITcsTargetScorerHost> Host`，`Score` 覆写为**纯转发**；
- **转发是必需的，不是选择**：`FTcsTargetScorerStrategy` 走 **C++ 虚分派**，而脚本定义的 struct **没有 C++ 类型**（`CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即野函数指针）——引擎层面无解。故 MUST 经"USTRUCT 转发器 → UObject 反射接口"两层；
- **空 `Host` 的语义 = 中性分（0.0）+ Warning**——与基类中性默认实现同口径（照过滤器插槽"空 Host = 通过"的既有裁定理由：配置遗漏 SHOULD 表现成"这一项不区分候选"，而不是"淘汰全部候选"；后者会让配置残缺表现成"打不到人"，比中性更难排查）。**MUST NOT 静默**：Warning 让配置残缺可见；
- **脚本返回 NaN 的语义 = 该候选被排除**（与 C++ 评分器同一条浮点边界，MUST NOT 特判成其它语义）；
- 接口 MUST 加 `Blueprintable`；形参 MUST 全反射（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验）；
- **性能纪律（SHOULD，写进类型注释与规格两处）**：评分器的调用频次是"**逐候选 × 逐排序项**"，高于过滤器；宿主插槽走跨语言反射（`UFunction::Invoke`），**热路径上不便宜** ⇒ 规格 SHOULD 建议"**排序评分优先 C++ 策略；脚本插槽用于低频 / 非关键排序**"，并给出依据，不让宿主自己撞上去；
- **双轨并存**：内置评分器与 C++ 策略子类 MUST 保持原样（内置走虚分派快路径，插槽只给宿主扩展）。

#### Scenario: 脚本层实现评分器并生效

- **WHEN** 宿主脚本层实现 `ScoreTarget` 返回自定义分，配进 `SortItems`（经 `FTcsScorerHostDelegate` 转发器），链跑到该步
- **THEN** 候选按该脚本分与给定方向排序（同分时落到稳定键决胜）

#### Scenario: 未配 Host 的转发器不淘汰候选

- **WHEN** `FTcsScorerHostDelegate.Host` 为空而该转发器被配进 `SortItems`
- **THEN** 该排序项对所有候选返回 0.0（并列）+ Warning 日志——候选**不因这一项被淘汰**（最终顺序退化为稳定键决胜）

#### Scenario: 脚本返回 NaN 时该候选被排除

- **WHEN** 脚本实现对某个候选返回 NaN
- **THEN** 该候选被排除出结果集（与 C++ 评分器同一条边界，不特殊处理）

#### Scenario: 内置评分器与插槽可混用

- **WHEN** `SortItems` = [内置距离升序, 脚本插槽降序]
- **THEN** 第一项先裁定、第二项仅在第一项相等时参与；两条路径（虚分派 / 反射转发）结果正确合并
