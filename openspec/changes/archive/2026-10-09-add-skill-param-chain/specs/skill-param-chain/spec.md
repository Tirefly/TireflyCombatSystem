# skill-param-chain Specification（delta）

## ADDED Requirements

### Requirement: 技能参数修正器的双形状（定义侧 / 账本侧）

`TcsSkill` MUST 提供**技能参数修正器**，且 MUST 照 M2 属性侧的**双形状**（设计裁定 D2-13「Operand 双形状，B 方案」）分成两套类型——**MUST NOT 用单一形状同时充当定义侧行与账本侧条目**：

- **定义侧** MUST 为 `USTRUCT(BlueprintType) FTcsNumericParamModifier`，字段 = `{FGameplayTag ParamKey, ETcsAttributeOp Op, FTcsParamValue Operand, int32 SortKey, int32 OverridePriority, FGameplayTag CompeteGroup, ETcsValueConventionFlag ValueConvention}`——`Operand` 是可配参数源的多态载体（`Literal` / `ParamRef` / `AttributeScaled` 等），故它**可以**持有对象引用；
- **定义侧 MUST NOT 携带 `Source`**（2026-10-09 用户质疑后订正，**MUST NOT 加回**）：**来源因实例而异**——同一个 `ParamChainRows[0]`，甲玩家靠装备给、乙玩家靠天赋给 ⇒ 它属**实例**而非 Def。**三条判据**：① `FTcsSourceHandle` 是**非反射纯 C++ struct**（无 `USTRUCT`）⇒ **不能作 `UPROPERTY`** ⇒ 一个 Def 类型上的 `Source` **既进不了细节面板、也不会被资产序列化**（UHT 产物可证：本类型反射属性恰为上述七项）；② 定义侧物化路径**根本不读它**（盖的是 `FTcsCastRun.RunSource`）；③ M2 的既有形状即此——`FTcsAttrModDefTableRow`（定义侧行）**没有 `Source` 字段**，它是 `MakeFromDef(Row, Operand, InSource)` 的**形参**。⇒ **`Source` 只住账本侧**，由**施加方**给出；
- **账本侧** MUST 为**纯 C++ struct `FTcsNumericParamModInstance`（无 `USTRUCT`）**，字段 = `{FGameplayTag ParamKey, ETcsAttributeOp Op, double ResolvedValue, FTcsSourceHandle Source, int32 SortKey, int32 OverridePriority, FGameplayTag CompeteGroup}`——`ResolvedValue` 是**物化边界求值并转规范值后的结果**，故账本条目**不含任何对象引用**；
- **为什么必须双形状（判据，不是偏好）**：`FTcsLearnedSkillEntry` 住 `TArray`（非 `UPROPERTY` 容器、元素为非反射 struct）⇒ GC 看不见它。若账本侧持 `FTcsParamValue`（可持 `TScriptInterface`），那些对象会被**静默回收**（症状 = 参数链里的源变成空引用，**不崩溃**）。M2 的账本侧注释即此义：「账本只为聚合热路径服务，不进反射面」「Literal 恒为已解析规范值（物化器单点转换保证，账本不做二次猜测——零膨胀）」。
- `Op` MUST 复用 M2 的 `ETcsAttributeOp`（五带 `TAO_Add/Override/PercentAdd/Mul/FlatAdd`），**MUST NOT 另立同义枚举**——M2 属性聚合、TcsDamage 流程属性与 M5 参数链**三处共用同一套带序**；
- `SortKey` 与 `OverridePriority` MUST **并存且语义各归其位**：`OverridePriority` = **仅 `TAO_Override` 带读**的强弱排座次（折叠器读它）；`SortKey` = 同带内展示/审计位（折叠器**不读**它，带序唯一真相在 `Op`）。**MUST NOT 令 `OverridePriority = SortKey`**——那会让 M5 的 Override 胜负判据与 M2 **不同**（M2 是「优先级大者胜 → 打平才按策略比数值」的三级比较，不是「取最大值」）；
- **MUST NOT 新增修正器字段所需的任何 `UENUM`**：`Op` MUST 复用 M2 的 `ETcsAttributeOp`、`Operand` MUST 复用 `FTcsParamValue`、`ValueConvention` MUST 复用 `ETcsValueConventionFlag`（三者都来自既有模块）。**该口径的作用域仅限修正器的这三个字段类型**——`FTcsSkillEntrySelector` 的 `Mode`（见「条目选择器」需求）MUST 有自己的 `ETcsSkillEntrySelectorMode`：选择器是**另一件事**（"施加作用到哪些条目"），全仓无任何现成枚举可表达它，而用整型常量代替只会换来"编辑器无下拉、丢掉 `UMETA`"的代价。

#### Scenario: 双形状分离且账本侧不含对象引用

- **WHEN** 检查 `FTcsNumericParamModifier` 与 `FTcsNumericParamModInstance` 两个类型
- **THEN** 前者是 `USTRUCT` 且 `Operand` 为 `FTcsParamValue`；**后者是纯 C++ struct、无 `USTRUCT`、无任何 `FInstancedStruct` / `TScriptInterface` 成员**（`ResolvedValue` 为 `double`）——账本侧**物理上不可能**持对象引用

#### Scenario: SortKey 与 OverridePriority 并存且各司其职

- **WHEN** 检查两个形状的字段表与折叠调用点
- **THEN** 两个字段**都存在**；折叠时进 `FTcsAttributeBandEntry::OverridePriority` 的是**本行的 `OverridePriority`**（MUST NOT 是 `SortKey`）；`SortKey` 不被折叠器读取

#### Scenario: 修正器三字段复用既有类型（零新枚举的作用域）
- **WHEN** 检查两个形状的字段类型清单与本能力新增的 `UENUM`
- **THEN** `Op` / `Operand` / `ValueConvention` 的类型都来自既有模块（`TcsAttribute` / `TcsCore` / `TcsNotation`），修正器族**没有**新增任何 `UENUM`；选择器的 `Mode` 类型不在本条判据范围内（它有专属枚举，见「条目选择器」需求）

### Requirement: 参数账本（每条目一套槽位）与 Apply / 级联摘除

`TcsSkill` MUST 把参数修正器做成**账本**——照 `FTcsAttributeInstance::AttrModInstances` 的既有形状，**每个已学条目持自己的参数修正器槽位**：

- `FTcsLearnedSkillEntry` MUST 增第七条字段 `TArray<FTcsNumericParamModInstance> NumericParamModInstances`——**账本侧类型（纯 C++、无对象引用）**，故账本**仍然**满足「MUST NOT 持任何 UObject 引用」的既有纪律，`UTcsSkillSubsystem::AddReferencedObjects` **MUST NOT** 为此新增遍历；
- **Apply MUST 形如 `ApplyParamModifiers(Unit, FTcsSkillEntrySelector, TArrayView<FTcsNumericParamModifier>, FTcsSourceHandle Source)`**：先按选择器定位目标条目，再把每条修正器**物化**（求值 + 转规范值，见下条需求）后追加进该条目的 `NumericParamModInstances`；单位未注册 / 无匹配条目 ⇒ **忽略并留日志、返回 false（MUST NOT ensure**——同 M2 `ApplyModifier` 的 D2-14 口径）；
  - **`Source` MUST 是形参**（见上条需求对定义侧的订正）：来源属**这一次施加**，由调用方声明"这次是谁给的"；
  - **`Source` 无效 ⇒ MUST 拒绝且零写入**：`Source` 是"按来源摘除"的锚点，无锚点的条目**摘不掉**（表现为"参数改不回去"，静默且远离根因）⇒ 宁可拒绝这次施加。**判据 MUST 是"条目 `NumericParamModInstances` 未被改动"**，MUST NOT 只查"返回了 false"（"先写后判"的实现也能返回 false）；
- **级联摘除 MUST 提供 `RemoveParamModifiersBySource(Unit, Source)`**：扫描该单位**全部条目的 `NumericParamModInstances`**，摘除 `Source` 匹配者，**返回摘除条数**；`0` 条命中 = **正常路径，MUST NOT ensure**（来源可能只挂过已撤销的修正器）；
- **Apply 与 Removal MUST 成对**：凡写入 `NumericParamModInstances` 的条目 MUST 带一个 `Source`，且该 `Source` MUST 能被 `RemoveParamModifiersBySource` 摘除（判据 = 不存在"摘不掉的条目"）。

#### Scenario: Apply 落在目标条目的槽位

- **WHEN** 以 `FTcsSkillEntrySelector` 选中某条目并施加两条参数修正器
- **THEN** 该条目的 `NumericParamModInstances` 恰增 2 条、每条带正确的 `Source`；**未被选中的条目的槽位不变**

#### Scenario: 按 Source 级联摘除恰摘该来源

- **WHEN** 同一单位的两个条目各挂一条不同 `Source` 的修正器，按其中一个 `Source` 调 `RemoveParamModifiersBySource`
- **THEN** 恰摘掉该来源的那 1 条（返回 1），另一来源的条目**原样保留**；再次调用同一 `Source` ⇒ 返回 **0 且不 ensure**

#### Scenario: 账本侧不持对象引用（GC 纪律自证）

- **WHEN** 检查 `FTcsLearnedSkillEntry::NumericParamModInstances` 的元素类型与 `UTcsSkillSubsystem::AddReferencedObjects` 的覆盖范围
- **THEN** 元素是纯 C++ `FTcsNumericParamModInstance`（无对象引用）⇒ ARO **无需**覆盖账本；该纪律句「账本条目不持任何 UObject 引用」**保持成立、未被修改**

#### Scenario: 定义侧行不含 Source，来源只由施加方给出

- **WHEN** 检查 `FTcsNumericParamModifier` 的字段表与它的反射属性清单，并检查两个施加路径各自盖的来源
- **THEN** 该类型**没有** `Source` 字段（反射属性不含它）；定义侧物化的条目标来源 = 本次 `FTcsCastRun.RunSource`，外部施加的条目标来源 = `ApplyParamModifiers` 的 `Source` 形参——**两条路径的来源都不来自定义侧行**

#### Scenario: Source 无效时拒绝施加且零写入

- **WHEN** 以**无效** `Source`（`Id == 0`）调用 `ApplyParamModifiers`，且选择器确实命中条目
- **THEN** 返回 false（留具名 `Warning`，不 ensure），且**被命中的条目的 `NumericParamModInstances` 逐字未变**——判据 MUST 是"槽位未被改动"，**MUST NOT** 只查"返回 false"（"先写后判"的实现也能返回 false）

### Requirement: 参数链求值（摊平后调共享折叠器）

`TcsSkill` MUST 提供**参数链求值**，把某键的账本槽位折叠成最终值，且 **MUST 复用共享折叠器**——**这是「与 AttributeModifier 同样的聚合流程」的落点**：

- 求值流水 MUST 为：① 取该条目的 `NumericParamModInstances` 中 `ParamKey` 匹配的条目；② `CompeteGroup` 分桶、**组内解析值最大者**进折叠；③ 摊平成 `FTcsAttributeBandEntry{Op, Value = ResolvedValue, OverridePriority = 该条的 OverridePriority}`；④ 调 **`FoldTcsAttributeBands(初值, Entries)`**；
- **初值 MUST = 该键参数行的求值结果；该键无参数行则取 0**；
- **MUST NOT 自建第二份折叠**：`Source/TcsSkill/` 内 `FoldTcsAttributeBands` 调用**仅此一处**——M2 属性聚合、TcsDamage 流程属性与 M5 参数链三处一律调用它（`STAT-1` 收束）；
- **顺序无关**：由折叠器纯函数性保证（任意排列结果一致）⇒ **MUST NOT** 在调用点加"按带序排序"的补丁（那会掩盖而非修复顺序依赖）；
- 求值 MUST **只读账本侧已解析值**，**MUST NOT** 在读路径上重新求值参数源（那会让读数值漂移，并绕开物化边界的"值约定只转一次"）。

#### Scenario: 五带折叠一次到位

- **WHEN** 某键挂 `Add` +10、`PercentAdd` +0.5、`Mul` 2 三条修正（初值取该键参数行 = 100）
- **THEN** 结果为 `((100 + 10) × (1 + 0.5)) × 2 = 330`——**不是**按书写顺序逐步施加的结果

#### Scenario: 书写顺序不影响结果

- **WHEN** 把同一组修正器**打乱书写顺序**再求值一次
- **THEN** 两次结果**逐字相同**

#### Scenario: Override 带按 OverridePriority 三级比较

- **WHEN** 同键有多条 `Override`、`OverridePriority` 不同
- **THEN** **优先级大者**生效（**MUST NOT 是"取数值最大者"**，也 MUST NOT 是"后写入者胜"）；`Override` 存在时其余带（**含 `FlatAdd`**）一并被覆盖

#### Scenario: 优先级打平时按同优先级策略比较数值

- **WHEN** 同键多条 `Override` 且 `OverridePriority` **打平**（例：`OTB_Max` 策略下的 `+5` 与 `+3`）
- **THEN** 按**该策略**比较数值选出赢家（`OTB_Max` ⇒ `+5` 胜）；**框架约定 = 调用方无此概念时取折叠器的默认策略** `OTB_Max`（先例：同族第三处消费者 `TcsDamage` 的 `TcsFlowAttributes.cpp:45` 即以默认实参调用 `FoldTcsAttributeBands(0.0, Entries)`）。**判据**：本任务 MUST NOT 为 M5 另造一个 tie-break 字段——策略在 M2 住在**属性定义**上（`TcsAttributeDef.h:79`），而技能参数**没有"属性定义"这一层** ⇒ 取默认值即正确处置；**设计文本那句"Override 组取最大值"是简化表述**（准确说法 = 「优先级大者胜 → 打平才按策略比数值」，默认策略恰好是 `OTB_Max`）。

#### Scenario: 无参数行时初值为 0

- **WHEN** 对某键只挂修正器、该键**没有任何参数行**
- **THEN** 初值取 0，折叠照常进行

#### Scenario: 组内取最大且落选者撤销后自动递补

- **WHEN** 同一 `CompeteGroup` 内挂 `+10` 与 `+30`，随后撤销 `+30` 的来源
- **THEN** 先只有 `+30` 进折叠（结果体现 30，**不是 40**）；撤销后 `+10` **自动生效**（竞争是求值时按账本全量重算的 ⇒ **MUST NOT 依赖任何休眠池 / 唤醒调用**）

### Requirement: 物化边界（求值 + 值约定只转一次）

参数修正器从**定义侧**进**账本侧** MUST 经过一个**单点物化边界**——它是"哪个值进账本"的唯一收口处：

- 物化 MUST 求值 `Operand`（经 `FTcsParamEvaluateContext`）后，按该行 `ValueConvention` 经 `FTcsValueConvention::ConvertToCanonical` **转一次**规范值，写入 `ResolvedValue`；
- **能力位为假的源不转**（判据由源自身 `AllowsValueConvention()` 声明，**MUST NOT** 建"源类型 × 可配约定"的中心名单）；
- 账本**MUST NOT** 二次转换 / 二次猜测（`ResolvedValue` 进账本即规范值）。

#### Scenario: 值约定在物化边界只转一次

- **WHEN** 某行配 `VCF_Percent`、书写值 85，物化该行
- **THEN** 账本条目的 `ResolvedValue` = **0.85**（转换发生一次）；**读账本时 MUST NOT 再转**

#### Scenario: 能力位为假的源原样进账本

- **WHEN** 某行的源 `AllowsValueConvention()` 返回 false（引用源 / 属性换算源），且该行配了值约定
- **THEN** `ResolvedValue` = **原样求值结果**（不转）——判据由源自身声明，不由物化器枚举源类型

### Requirement: 定义侧参数链行的激活期物化

`FTcsSkillDefData` MUST 提供 `TArray<FTcsNumericParamModifier> ParamChainRows`，并在**激活时**物化进该条目的 `NumericParamModInstances`：

- 物化时 `Source` MUST = **本施法运行态来源句柄**（`FTcsCastRun.RunSource`）⇒ 随施法终结**级联摘除**，生命周期语义与基类 `ModifierRows` 同款；
- **施法终结 MUST 真的执行该摘除**（判据 = 同一 Entry 反复施法后槽位**不累积**）：物化是"每次激活都追加"，而终结例程 MUST 按 `RunSource` 摘掉本次那一批——漏掉它会让槽位逐次累积，表现为"放两次技能、参数翻倍"（静默错误且远离根因）。**边界（MUST 分清，防张冠李戴）**：本条摘的是**技能自己定义侧物化的行**（落 M5 技能账本）；台账 `CHAIN-7` 的"链挂条目回收"（链步骤经 `Context.RunSource` 挂到 **M2 属性账本**的修正器）归 `PLN-R6` Task 5 Step 3 的三路统一回收例程——两者来源句柄同源但**账本不同**，互不代劳；
- **声明作用域恒为本条目自身**，**MUST NOT** 接受 `FTcsSkillEntrySelector`（该选择器保持专属"外部施加"场景——同一件事不许两个入口）；
- **只落内联形态**：**MUST NOT** 引用 `UTcsSkillModDef`（实测该类型在全仓 `Source/` **零命中**）——模板引用形态整体归 `R6.5-f`。

#### Scenario: 激活期物化且来源为施法运行句柄

- **WHEN** 某技能定义配了 `ParamChainRows`，激活该技能
- **THEN** 该条目的 `NumericParamModInstances` 含物化出的条目，且每条 `Source` = **本次施法运行态的来源句柄**

#### Scenario: 施法终结按来源摘除物化行且槽位不累积

- **WHEN** 同一技能定义**连续激活两次**（各自终结），每次读该条目 `NumericParamModInstances` 的条数
- **THEN** 两次读数**相同**（每次终结都按各自的 `RunSource` 摘净本次物化行）——若终结不摘，第二次读数会翻倍

#### Scenario: 落内联形态、无模板引用

- **WHEN** 检查 `ParamChainRows` 的元素类型与引用形态
- **THEN** 元素是**内联的** `FTcsNumericParamModifier`；**不含**任何 `UTcsSkillModDef` / `TemplateTag` 引用

### Requirement: 条目选择器（外部施加的作用域）

`TcsSkill` MUST 提供 `FTcsSkillEntrySelector`（**名字 MUST 带 `Skill` 段**——它只服务技能账本条目；"看起来通用"的名字会在第二个域真出现时逼出改名，那时改动面是整个公开面），用于表达"外部施加的修正器作用到**哪些已学条目**"：

- 形状 MUST 为 `{EMode: ETcsSkillEntrySelectorMode, TArray<FGameplayTag> DefTagFilter, FGameplayTagQuery CategoryTags, FInstancedStruct CustomFragment}`——**每一档都有字段载体**，MUST NOT 出现"枚举有档、字段缺失"；
- 档位 MUST 恰好四档：`ESS_All`（无需筛选）/ **`ESS_ByDefTag`**（读 `DefTagFilter`，**层级匹配**——填父 tag 命中其全部子技能）/ **`ESS_ByCategoryTags`**（读 `CategoryTags`，**预留空壳**）/ `ESS_Custom`（读 `CustomFragment`）；
- **`ByTag` MUST NOT 作为档名**（2026-10-09 用户裁定改名）：它读的是条目的 `DefTag`（内容身份），而"Tag"一词在 TCS 里同时指属性词/状态词/事件词/参数键等**七八种**东西 ⇒ 档名 MUST 与字段名 `DefTag` 对齐，读代码即可知"按哪个 tag"；
- **原 `ById` 档 MUST NOT 存在**（2026-10-09 用户裁定删除，**判据三条**）：① **它不可被作者配置**——`FTcsSkillEntryHandle` 是**运行时产物**（桶内槽位下标 + 进程内发号代际），编辑器里无法预先填出合法值 ⇒ 对"以配置为入口的外部施加"而言**等于没有这一档**；② **它的"Id"是历史遗留**——所有 Def 资产身份原为 `FName DefId`，2026-09-22 起**已全量迁移到 `GameplayTag`（`DefTag`）** ⇒ "按 Id 筛"的正当形态就是**按 tag 筛**，已被 `ByDefTag` 覆盖（同一份内容身份不需要两档）；③ 留一个**带代际的句柄过滤位**会诱导后人用它做长期引用的精确命中，而代际会让跨帧引用自然失效（释放即 +1 置偶）⇒ 形状**看起来能用、实际易碎**；
- **`ESS_ByCategoryTags` 是本轮唯一新增的"预留空壳"档**（承接上述删除的位置）：其目标形态 = 按定义上的**类别标识容器**（`FGameplayTagContainer`，如一个 Buff 同时具"火属性伤害 + 异常状态"，一个技能同时具"左手/右手/双手释放（三选一）+ 投掷类 + 引导类"）筛选，规则**倾向复用引擎 `FGameplayTagQuery`**（`HasAll` / `HasAny` 等表达式，自带编辑器定制）。**本轮 MUST 只留空壳、MUST NOT 落机制**——判据三条：① **载体不存在**（`StateDef` 侧的类别标识容器字段尚未新增，属独立变更）；② **规则与档名都未拍板**（用户明示"后续改名为 `ByCategoryTag` 或 `ByCategoryTags`，**还要讨论**"）；③ **可能另立模块**（GameplayTag 筛选机制在 TCS 里将是重要角色、**甚至可能单独开 `TcsGameplayTag` 模块**）⇒ 现在写进 `TcsSkill` 内部将来极可能要搬家。**台账登记 = `STAT-10`**；
- **两个未实现档（`ByCategoryTags` / `Custom`）的行为 MUST 同款**：被命中时 MUST **具名报出"该档未实现"**且**不施加任何修正**（`MUST NOT` 静默当 `All`、`MUST NOT` 静默无操作——静默会让宿主以为已施加/已筛选）；
- 选择器**只服务外部施加**：定义侧 `ParamChainRows` MUST NOT 接受它。

#### Scenario: 四档各有字段载体

- **WHEN** 检查 `EMode` 的四个枚举值与 `FTcsSkillEntrySelector` 的字段表
- **THEN** `All` 无需筛选、`ByDefTag` 读 `DefTagFilter`、`ByCategoryTags` 读 `CategoryTags`、`Custom` 读 `CustomFragment`——四档都有对应字段；`EMode` 类型是**本能力自有的** `ETcsSkillEntrySelectorMode`，且**不存在** `ById` 档与任何句柄过滤字段（`TArray<FTcsSkillEntryHandle>` 一类）

#### Scenario: ByDefTag 支持完整与层级两种粒度

- **WHEN** 分别以**完整 `DefTag`**（如 `SkillDef.Check.ParamChainCompete`）与**父 tag**（如 `SkillDef.Check`）作 `ByDefTag` 筛选
- **THEN** 前者只命中那一条（其余条目的槽位不变），后者命中其全部子技能条目——**两次筛选的命中条数不同**（读数可分辨）

#### Scenario: ByCategoryTags 档具名报出且零修正被施加（预留空壳）

- **WHEN** 以 `EMode = ByCategoryTags` 并给出一个"若被误实现就会命中"的查询（如 `HasAny [SkillDef.Check]`）调用外部施加入口
- **THEN** 留**具名**未实现提示（提示里带档名），且**零修正被施加**——读数 MUST 断言"**没有条目的 `NumericParamModInstances` 被改动**"，**MUST NOT** 只查"有 Warning"（warning 照样可能发；只查它无法区分"报了未实现且没施加"与"报了未实现却仍然施加了"）

#### Scenario: Custom 档具名报出而未实现

- **WHEN** 以 `EMode = Custom` 调用外部施加入口
- **THEN** 留下**具名**未实现提示，且**零修正被施加**（判据同 `ByCategoryTags`：断言槽位未被改动）

### Requirement: `Level` 修正键的接线

`FTcsNumericParamModifier::ParamKey` 取**特殊键 `Level`** 时，其语义 MUST 是**修正生效等级**而非普通参数：

- **该键的词 MUST 由插件模块原生声明，且声明方 = 拥有等级语义的 `TcsState`**（2026-10-09 用户提出后核实订正）：`Level` 是**框架自己解释的契约词**（`clamp(0, LevelBase + Σ)` 这条公式由等级域求值），而非宿主内容词 ⇒ 按 `gameplay-tag-governance`「框架契约词 MUST 由插件模块原生声明」落为原生 tag `TcsStateParam.Level`（`TcsState/Public/Param/TcsStateParamKeys.h`，带模块导出宏——`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 是裸 `extern` ⇒ 否则跨模块引用会 `LNK2001`）。**声明方 MUST 是 `TcsState` 而不是 `TcsSkill`，判据三条**：① **语义归属**——`LevelBase` / `MaxLevel` 声明在 `FTcsStateDefBase`（技能 Def **继承**它），D3-11「Level 终定」的家是状态侧规格；② **依赖方向（决定性）**——`TcsSkill` 已依赖 `TcsState` ⇒ 声明在上游时两侧消费都零成本、不成环；若声明在 `TcsSkill`，则状态侧将来消费（状态等级修正）就要**反向依赖 TcsSkill = 成环**；③ 同一个词被多域消费是常规情形（M2/M5/状态三域共用同一套五带即先例）。**判据**：宿主漏配会让 `Level` 修正**静默失效**（数值照进账本、等级永不动、零报错）——那正是"契约词不可下放"要防的静默破坏。`TcsStateParam` 根的**一般键**（具体参数键）仍归宿主 ini；同一根下"契约词 + 内容词"并存是既有约定（拆的只是**声明方**）；
- 它 MUST 接入账本的生效等级求值（`EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`），且 MUST **经同一条五带折叠**（初值 = `LevelBase`）——**MUST NOT** 为它另立一套算术（那会造出第二份折叠语义，正是 `STAT-1` 要防的）；
- **`EffectiveLevel` 的快照时点 = 激活时**（运行中升级不追溯）；
- **该键 MUST NOT 出现在普通参数的折叠结果里**：等级不进参数表、参数表不动等级（`GetNumericParam(Level)` 是另一条独立路径，两边各自按同键槽位折叠、互不写回）；
- `Level` 之外的一切键 MUST 走普通参数链折叠，**MUST NOT** 影响生效等级。

#### Scenario: Level 键进等级求值而非参数表

- **WHEN** 某条目定义 `LevelBase = 1`，且有一条 `ParamKey = Level`、`Add` +2 的修正
- **THEN** 生效等级为 3；且该键**不出现在**普通参数的折叠结果里（对未配参数行的普通键读取得 miss）

#### Scenario: Level 键由插件原生声明且住拥有语义的模块

- **WHEN** 检查 `TcsStateParam.Level` 的声明方与宿主 ini
- **THEN** 它由**插件模块**原生声明（带模块导出宏），**不需要**宿主 `Config/DefaultGameplayTags.ini` 配置即生效；且声明处住 **`TcsState`**（拥有等级语义、且是 `TcsSkill` 的依赖上游）——**MUST NOT** 声明在 `TcsSkill`（那会让状态侧消费时反向依赖、成环）；`TcsStateParam` 根下的普通参数键仍由宿主声明

#### Scenario: 激活后升级不追溯

- **WHEN** 某次激活已冻结 `EffectiveLevel`，随后该条目的 `Level` 被改变
- **THEN** 本次运行态读到的等级**仍是激活时的值**
