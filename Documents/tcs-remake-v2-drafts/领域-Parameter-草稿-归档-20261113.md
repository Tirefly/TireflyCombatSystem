# 领域草稿 · Parameter（参数域）—— **已归档**

> **本文件已归档，不是 V2 设计依据。** 2026-11-17 用户裁定：Parameter 域「完全刨除 V1 的原有内容和代码」，从零重做；设计内容另立新档 [`领域-Parameter-草稿.md`](领域-Parameter-草稿.md)。
>
> **为什么整份保留（正文一字未改）**：
>
> 1. **§11 裁决流水 D1–D50**（含用户原话）是**裁决记录**，不是设计推导；`D42–D46` 虽已作废，也必须在案。
> 2. **全部 `文件:行号` 实证**（§0 现状清点、§7A 折叠器归属、§12/§13/§15）是**事实**，不是主张。事实不会因为设计换了就失效。
> 3. 新档引用旧裁决时，指向本文件。
>
> ⚠️ **MUST NOT** 把本文件的设计段落（尤其 §8.8 / §9 / §10，以及 §6 的 `Live`/`Snapshot` 归属）当作 V2 设计依据。那些形态是从 V1 的 `Mode` 位倒推出来的，已被 2026-11-13 的「基底出生求值 + 写入门 + 账本折叠」方向、以及 2026-11-17 的「从零重做」整体覆盖。
>
> ⚠️ **本文件里的【V1 实证】只能用来判断「V2 要解决什么问题」，MUST NOT 用来论证「V2 该长什么样」。**

---

> 以下为原文（原名 `领域-Parameter-草稿.md`，962 行），归档时仅在本段之前加归档头，正文自下一行起一字未动。
# 领域草稿 · Parameter（参数域）

> **族外草稿，不是实现依据。** 参数域正式文档尚未开始；本文只收录**调研实证 + 待确认点**。未列入本文的不算已裁，已裁的也不等于已进参数域文档。
>
> **推进顺序（2026-11-06 用户裁定）**：「先全面调研完成后，再说，可以先起一个草稿文档，逐步记录，逐步确认，最后再确定这三个选项」⇒ **调研 → 记录 → 逐条确认 → 最后裁架构**。

---

## 本版说明（2026-11-13 精简）

**触发**：用户 2026-11-13 指出两点——① 「Parameter 域整体问题很大，我想先从头捋一遍」；② 「现在你根本无法避免参考 V1 的实现，这就更加重了我的疑虑」。

### 三类内容的标记约定（本版新增，直接回应疑虑②）

| 标记 | 含义 | 允许怎么用 |
|---|---|---|
| **【V1 实证】** | V1 代码里读出来的事实，带 `文件:行号` | 只能用来**判断 V2 要解决什么问题**；MUST NOT 用来论证 V2 该长什么样 |
| **【V2 推导】** | 从 V2 已裁约束推出来的结论 | 可当设计依据，用户确认前不算已裁 |
| **【已裁 Dxx】** | 用户明确裁过 | 不许重开；要改 MUST 先说明依据变了 |

### 本版动作

| 动作 | 内容 | 原因 |
|---|---|---|
| 删 | **原** §8.8.7 的全部落地伪代码（约 460 行）与原 §8.8.1–8.8.6 的逐条论证 | 那套 `FTcsParamBinding` 形态是**照 V1 的 `Mode` 位长出来的**，已被 2026-11-13 用户所批的「基底出生求值 + 写入门 + 账本折叠」方向覆盖 |
| 压 | §8.8 `671→162`、§7A `153→49`、§12 `168→27`、§15 `124→70`、§8.1–8.4 `38→27`（行数实测） | 已裁且已落地 / 已结案 / 不属本域；保留结论 + 指针。**全文 1794 → 963 行** |
| 压 | §0 的「V1 里参数散在六个模块」考古表 → 「今天代码里有什么」清点 | 删掉以 V1 为骨架的叙述方式 |
| 留 | §11 裁决流水 D1–D50 全表（D42–D46 标作废）、§10 确认清单、所有 `文件:行号` 实证 | 授权记录与可复核事实，一个都不能丢 |

> **原文备份**：`%TEMP%\Parameter-draft-backup-20261113.md`（1794 行，本版改写前）。

---

## 目录

| § | 主题 | 性质 |
|---|---|---|
| 0 | 现状清点：今天代码里的「参数」由什么组成 | 实证 |
| 1 | 值来源族 | 实证 |
| 2 | `AttributeScaled` 的读取侧 | 已裁 D12/D13/D14 |
| 3 | `Composite` 与 `ParamRef` | 已裁 D36/D37 |
| 4 | 与 Attribute 域的接口 | 已裁 |
| 5 | 值载体与源族的命名（P2 / P3） | ⚠️ 待议 |
| 6 | `Live` / `Snapshot` 的归属 | 已裁 D1；语义待重定，见 6.1 |
| 7 | 参数域自己的修改器（耦合点登记） | 登记 |
| 7A | 折叠器归属 | 已裁 D2–D5，已落地 |
| 8 | 调研实证 | 实证 |
| 8.8 | 冻结机制：一个值绑在哪个时刻 | **本轮重点**：D47–D50 已裁 + U1–U7 待确认 |
| 9 | 架构三选一 | ⚠️ 最后裁 |
| 10 | 逐条确认清单（进度表） | 索引 |
| 11 | 裁决流水 D1–D50（含作废标注） | 台账 |
| 12 | 弗拉米尔案例（已压缩为指针） | 已裁 D25–D29 |
| 13 | 遗留点结案 | 已结 |
| 15 | `InstigatorTags` / `TargetTags`（不引入） | 已裁 D33 |
| 方法 | 调研方法与边界 | 元 |

> 原 §14 已并入 §3（编号留空，不重排）。

---

## 0. 现状清点：今天的代码里，「参数」由什么组成

> **【V1 实证】** 全仓 `TcsParamSubsystem` = **0 命中**，V1 没有任何参数域门面。今天的「参数」是一组散件。V2 的 Parameter 域要从这些散件里**重新划**出来，不是给某个 V1 模块改名。

### 0.1 载体系（无领域词汇、被多域共享）

| 类型 | 住哪 | 消费者 | 含领域词汇 |
|---|---|---|---|
| `FTcsParamValue`（载体，内含 `FInstancedStruct Source`） | TcsCore | 48 处 / 跨 7 模块 | 否 |
| `FTcsParamValueSource`（抽象基类）+ 6 个内置派生 | TcsCore | — | 否 |
| `FTcsParamEvaluateContext`（四成员） | TcsCore | 48 处 | 否 |
| `FTcsParamEnumerableSource`（可枚举分支基类） | TcsCore | 4 个派生 | 否 |
| `FTcsParamSource_ParamRef`（唯一读 `ParamTable` 的源） | TcsCore | 见 §8.5 | 否 |
| `ITcsParamTableReader`（读口契约） | TcsCore | 1 个实现（`UTcsStateParamTableReader`） | 否 |
| `ETcsValueConventionFlag` + `ConvertToCanonical` | TcsNotation | 12 处 / 跨 5 模块 | 否 |
| `FoldTcsAttributeBands`（折叠器） | TcsAttribute | 3 处 | 否 |

### 0.2 领域侧（含领域词汇）

| 类型 | 住哪 | 领域词汇是什么 |
|---|---|---|
| `FTcsNumericParamRow`（参数行） | TcsState | 「参数表的行」 |
| `ETcsParamMode` | TcsState | 「施法/施加那一瞬冻住」（**D47 拟从参数行移除**，见 §8.8.3） |
| `FTcsParamSnapshot` / `FTcsParamSnapshotEntry` | TcsState | 「施加瞬间冻结」（**D47 后由实例出生值表取代**，见 §8.8.3） |
| `FTcsBoolSwitchRow`（布尔开关行） | TcsSkill | 「技能开关」 |
| `FTcsNumericParamModifier` / `FTcsNumericParamModInstance` | TcsSkill | 「改技能参数表的键」 |
| 三个契约键（见 0.3） | TcsState / TcsSkill | Level / 冷却 / 消耗 |
| `FTcsParamSource_AttributeScaled` | TcsAttribute | 「属性」 |
| 四个等级源 + `ITcsEntityLevelProvider` | TcsBuff | 「等级」 |

### 0.3 三个契约键（全仓仅三个 `FNativeGameplayTag` 参数键）

| 键 | 声明 | 基数住哪 | 求值路径 | 冻结点 |
|---|---|---|---|---|
| `Tag_TcsStateParam_Level` | `TcsState/Public/Param/TcsStateParamKeys.h:42` | `FTcsStateDefBase::LevelBase`（`TcsStateDefBase.h:163`） | `EvaluateEffectiveLevel`（`TcsParamChain_Evaluate.cpp:189-206`） | **实例建立那一刻**（`TcsCastOps.cpp:568-571`） |
| `Tag_TcsSkillParam_CooldownPct` | `TcsSkill/Public/Param/TcsSkillParamKeys.h:38` | 无（纯系数，语义是 `1 − CDR`） | `EvaluateParamKey`（`TcsCooldownOps.cpp:860`） | **起冷那一刻**（`TcsCooldownOps.cpp:129-131`，全场只取一次） |
| `Tag_TcsSkillParam_CostAmount` | `TcsSkill/Public/Param/TcsSkillParamKeys.h:67` | `Amount` 自己的求值结果 | `FoldParamKeyWithBase`（`TcsCostOps.cpp:73-74`） | **每次现算，且刻意算两次**（门禁 `:215-216` / 提交 `:338-339`） |

**三个键的冻结点各不相同，而且这个不同是刻意的**：

- CDR — `TcsCooldownOps.cpp:129-131`：「★ CDR 只在此刻取一次（D5-15），取多次会让同一批轨道拿到不同系数」；
- Cost — `TcsCostOps.cpp:336-337`：「本行 MUST NOT 被优化成沿用门禁那次的结果：门禁与提交之间隔着建 run / 快照 / 物化等步骤，期间修正器可能已变」（TOCTOU）；
- Level — `TcsCastOps.cpp:562-563`：「等级定了之后本次施法不变（运行中升级不追溯）」。

⇒ **【V2 推导】** 这三个值的冻结点**由代码定、不该由作者配**。参数行的门（§8.8）管不到它们——这是 2026-11-13 用户裁定「门只管 `Params` 行」的直接依据。

### 0.4 无 Key 的值（不是键值对）

| 值 | 声明 | 求值点 |
|---|---|---|
| 时段时长 `FTcsPhaseSpan::Duration` | `TcsSkill/Public/Def/TcsPhaseSpan.h:24` | `TcsCastOps_Phase.cpp:57` |
| 冷却时长 `FTcsCooldownTrack::Duration` | `TcsSkill/Public/Def/TcsCooldownTypes.h:92` | `TcsCooldownOps.cpp:190` |
| 消耗 `Amount` | 消耗策略 | `TcsCostOps.cpp:44` |
| Buff 时长 `DurationTime` | TcsBuff | `TcsBuffOps_Snapshot.cpp:107` |
| 链步骤内联值 | `FTcsStepDamage` / `FTcsStepSetVar` | `TcsStepDamage.cpp:52`、`TcsStepSetVar.cpp:35`（★ 空上下文） |

**【V1 实证】** 这批值没有 `ParamKey`，因而没有账本写入通道——`FTcsNumericParamModInstance` 必带 `ParamKey`（`TcsNumericParamModifier.h:262`）。`TcsBuffOps_Snapshot.cpp:91-96` 已把「把时值塞进快照会引入一个『时值的参数键』——那是插件自造参数表键」记成边界。

⇒ **【V2 推导】** 它们的冻结语义**由源类型决定**，不由门决定：`Literal` 天然冻结；`ParamRef` 间接冻结（读的是实例值表，出生一次）；`AttributeScaled` 天然实时（但今天恒落 `Fallback`，见 §8.1）。**归属仍未裁**，见 §8.8.7。

### 0.5 模块依赖现状

```text
TcsCore        ← 无 Tcs 依赖（禁依赖任何兄弟 / 上层 Tcs 模块）
TcsNotation    ← Core, CoreUObject（与 TcsCore 平级，互不依赖）
TcsAttribute   ← TcsCore, TcsNotation
TcsState       ← TcsCore, TcsNotation, TcsAttribute     （看不见 TcsSkill / TcsBuff）
TcsBuff        ← + TcsEffect, TcsState
TcsSkill       ← + TcsEffect, TcsState
TcsDamage      ← TcsCore, TcsAttribute, TcsEffect
```

**【V1 实证】** `TcsSkill` 与 `TcsDamage` 都反向依赖 `TcsAttribute`，只为拿一个五带枚举与一个纯函数（`TcsNumericParamModifier.h:11-12` 直接 `#include` 属性域头）。

---

## 1. 值来源族

`FTcsParamValue { FInstancedStruct Source }` 今天已有一组源：

| 源 | 语义 | 住哪 | 状态 |
|---|---|---|---|
| `FTcsParamSource_Literal` | 配置写死的数 | TcsCore | 可用 |
| `FTcsParamSource_ParamRef` | 读同参数表的另一个键 | TcsCore | **只在部分路径可用**，见 §8.5 |
| `FTcsParamSource_Composite` | 多个值组合 | 未建 | **本轮新造概念**，V1 零命中 |
| `FTcsParamSource_AttributeScaled` | 系数 × 另一属性当前值 | TcsAttribute | **今天恒落 `Fallback`**，见 §8.1 |
| `FTcsParamSource_StateLevel{Array,Map}` | 按生效等级查表 / 查档 | TcsBuff | 可用（读 `Context.EffectiveLevel`，纯字段） |
| `FTcsParamSource_InstigatorLevel{Array,Map}` | 按施加者等级查表 / 查档 | TcsBuff | **恒落 `Fallback`**，见 §8.2 |
| `FTcsParamSource_HostDelegate` | 宿主自定义求值 | TcsCore | **已裁删除（D11）** |

**【V1 实证】** 全仓 `Source/` 扫 `Curve` = 0 命中——没有 Curve 版等级源，只有上面四种。

---

## 2. `AttributeScaled` 的读取侧（**【已裁 D12/D13/D14】**）

**背景**：「读某实体某属性 × 系数」必须能区分读**哪个**实体。

```cpp
enum class ETcsAttrReadSide : uint8
{
	Instigator,   // 施加者（施法者的属性）
	Target,       // 被加成的实体（修正器宿主的属性）
};
```

**【V2 推导】** 不在形态里区分 `Self`：`Instigator` / `Target` 已覆盖，「自身」即 `Target`。

**命名坑（必须写进参数域正式文档）**：`Target` 在伤害流程里指「被命中的敌人」，在 Buff 里指「被加成的友军」——两者可以不是同一个实体（治疗技能：施法者 A、受害 B、治疗目标 C）。这个歧义只允许存在于参数源内部，**MUST NOT 外溢成跨域通用词**。

**【已裁 D13】** `ITcsAttributeProvider` 删除 ⇒ `AttributeScaled` 在 V2 必须**直接走属性子系统**，不再经宿主接口。**【已裁 D14】** 给 `FTcsParamEvaluateContext` 加 `TObjectPtr<UWorld> World` 字段，不建新上下文类型、不建新接口。⇒ 本节最终形态在参数域开工时重定。

---

## 3. 多属性组合：`Composite` 与 `ParamRef`（**【已裁 D36/D37】**）

**背景**：GAS 用 `UGameplayEffectExecutionCalculation` 支持「一个值由多个属性组合」；TCS 的聚合器**无代码插点**（《数值聚合器》§2.2「计算在上游求值并传入终值」），所以组合能力必须落在参数域。

### 3.0 两套 `Op` MUST NOT 混淆

| | `ETcsParamCombineOp` | `ETcsAggregateOp` |
|---|---|---|
| 谁的 | `FTcsParamSource_Composite`（值来源） | 数值聚合器 |
| 几个 | **4**：`Add` / `Mul` / `Max` / `Min` | **7**：`Add` / `PercentAdd` / `Mul` / `FlatAdd` / `OverrideBase` / `OverrideSegment` / `OverrideGlobal` |
| 干什么 | 把**多个值**合成**一个值** | 把**多条修正**折叠进**一个属性** |
| 何时 | **求值那一刻** | 属性被写之后 |
| 输入 | `TArray<FTcsParamValue> Terms` | `TArray<FTcsAggregateEntry>` |

> **唯一的接触点**：Composite 算完交出一个 `double`，这个 `double` 可能成为聚合器的基值或某个条目的 `Value`。仅此而已。
> **`Add` 在两处含义不同**：Composite 里是「两个数相加」，聚合器里是「同带求和 `ΣAdd`」。MUST NOT 以为能互通。

### 3.1 两个源的形态

```cpp
// 引用同参数表的另一个键（可链式 A→B→C，环由配置期校验拦）
struct FTcsParamSource_ParamRef : FTcsParamValueSource
{
	FGameplayTag Key;        // 同域参数表的键
	double       Fallback;   // 键 miss / 无参数表时取用（必填）
};

// 组合多个值（可嵌套）
struct FTcsParamSource_Composite : FTcsParamValueSource
{
	ETcsParamCombineOp     CombineOp;   // 四选一，见 3.2
	TArray<FTcsParamValue> Terms;       // 每个 Term 自己又是一个完整的值载体
};
```

> **成员名叫 `CombineOp` 而不是 `Op`**（D37）：属性域有 `ETcsAttributeOp Op`、聚合器有 `ETcsAggregateOp`、流程步骤也有 `Op`——全仓到处都是 `Op`。写成 `CombineOp`，一眼知道「这是组合运算，不是属性带」。

**配置形态（细节面板）**：`Source` 选 `Composite` → `CombineOp` 四选一 → `Terms` 里逐项添加，每项再选它自己的 `Source`。**零自定义编辑器代码**：`Terms` 元素是嵌套的 `FTcsParamValue`，引擎的 `FInstancedStruct` 细节面板自动递归展开，插件不写任何 `IDetailCustomization`。需要先加后乘时套一层（`(A+B)×2` ⇒ 外层 `Mul`，其 `Terms[0]` 是 `Add`）。

### 3.2 运算集：四则

| 运算 | 保留 | 判据 |
|---|---|---|
| `Add` | ✅ | 刚需 |
| `Mul` | ✅ | 刚需 |
| `Max` | ✅ | 「攻击力和法强取高的那个」这类自适应加成；形态与 Add/Mul 完全同构 |
| `Min` | ✅ | 同上，对称保留 |

**四条的共同点**：都是 **n 元结合运算**（`(A⊕B)⊕C = A⊕(B⊕C)`）⇒ 与 `Terms: TArray` 这个**无序同质容器**形态自洽。

**为什么不加 `Sub`**：

```text
a − b   ≡   Add { a,  Mul { b, Literal(-1) } }
```

⇒ 不配 `Sub` 也能算。**真加进去反而破坏形态**：n 元减法**不满足结合律**（`(A−B)−C ≠ A−(B−C)`），一个 `TArray` 装不下「谁减谁」的顺序；要真做必须另立二元节点 `{Left, Right}`——那是第二种节点形态。

**为什么不加 `Div`**：除法用现有四则**表达不出来**（需要 `1/b`，而 `b` 是运行期值，配置期算不出）。这是真缺能力，但按铁律①：**【V1 实证】V1 全仓零处配置级除法**——所有除法都是 C++ 硬编码（`TcsCooldownOps.cpp:193` 的 `BaseDuration * (1.0 - CooldownPct)`、`TcsParamChain_Evaluate.cpp:205` 的取整），策划侧配的是**一个数**，不是**一条公式**。⇒ **现在不建**。将来要用**一元 `Reciprocal` 节点**加（保持 n 元形态），MUST NOT 往枚举里塞一个 `Div`。

**为什么不加 `If` / 比较 / `Clamp`**：

| 候选 | 否决理由 |
|---|---|
| `If` / 比较 | **那已经是脚本**。同一能力链路径能做（`Branch` + `SetVar`），在参数源里再造一套是 D11 已否决的方向 |
| `Clamp` | **值域收口归属性域**（`Attribute.md` §4），参数域再做一次就是两处收口、两个真相 |

### 3.3 空 `Terms` 一律是配置错误

| 运算 | 空集若给值 | 后果 |
|---|---|---|
| `Add` | `0` | 静默出一个 0，看不出配错了 |
| `Mul` | `1` | 同上 |
| `Max` / `Min` | 无自然值 | 必须发明哨兵值 |

⇒ **不按运算分别定义单位元，统一按「空 `Terms` = 配置错误」处置**：由 `IsDataValid` 在保存期报 `Error`。若给 Add/Mul 单位元、只让 Max/Min 报错，则「空 Terms 是否合法」取决于运算——同一件事两个答案。

> **与聚合器的空集语义不冲突**：聚合器的「流程属性基值恒 0.0（空集语义）」说的是**没有修正器**时属性等于基值，那是**聚合器的正常状态**；`Composite` 的空 `Terms` 是**配置者漏填**。两者性质相反。

### 3.4 连带后果（参数域自己要还的债）

| 引入的东西 | 必须配套 |
|---|---|
| 表达式树（可嵌套） | **配置期环检测**（`ParamRef` 成环 = 无限递归） |
| 递归求值 | **求值深度上限** + `ensure` |
| 子树里的 `AttributeScaled` | **依赖边收集要遍历子树**，不是看顶层一个字段（`Attribute.md` §5.2） |
| 参数域体量 | 从「值载体」变成**表达式求值器**——这是参数域正式文档必须正面写清的定位变化 |

> **与聚合器的边界不变**：参数域算出的是一个 `double`，聚合器仍然只做七带折叠、仍然封闭。表达式能力**不构成「聚合器开插点」**。
> **【V1 实证】** `FTcsParamSource_Composite` 全仓 **0 命中**——它是本轮新造的概念，不是 V1 遗产。

---

## 4. 与 Attribute 域的接口（**【已裁】**）

- Attribute 域的 `FTcsAttrModInstance` 持有 `FTcsParamValue`（值载体），`FTcsAttrModDef` **不持有**（保泛用性）。
- 参数源通过 `FTcsParamEvaluateContext` 拿**属性读口**——V1 是接口 `ITcsAttributeProvider`，**V2 已裁删除**（D13），改为直接走属性子系统。
- 求值上下文带「是否冻结」的位：为真时 `AttributeScaled` **MUST NOT** 登记依赖边（冻结不跟随）。
- `Attribute.md` §7.2 已把修正器的模式字段定为 `ETcsParamMode Mode`（枚举归参数域），本域必须提供它的**定义与语义**。

> ⚠️ **本节第 3 条的落点已被 §8.8 的新方向改写**：旧写法假设「求值上下文带一个 `bSnapshot` 位」，D47 把「要不要跟随」做成**参数行上的门**（§8.8.3/§8.8.5），门落在出生求值时一次性写进实例。两处冲突**以 §8.8 为准**；本节第 3 条要等 §8.8.7 的 U2/U3（出生求值读什么、挂哪一层）确认后才能重写。

---

## 5. 值载体与源族的命名（P2 / P3）

**触发（2026-11-06 用户质询）**：用户印象是「Def 里配的叫 `ParamSource`、运行时真正起作用的叫 `ParamValue`」，问这是否不符合 TCS 统一命名规范。

**事实澄清**：这个对应关系不成立——真实关系是**包含**，不是前后两态。

```text
FTcsParamValue                    ← 载体（外壳）。Def 里配的是它，实例里拿的也是它
└── Source : FInstancedStruct
    └── FTcsParamValueSource      ← 抽象基类，meta=(Hidden)
        ├── FTcsParamSource_Literal / _ParamRef / _Composite / _AttributeScaled / _HostDelegate
        ├── FTcsParamSource_{State,Instigator}Level{Array,Map}
        └── FTcsParamEnumerableSource    ← 可枚举分支基类
```

**已裁（Attribute 域范围内）**：`FTcsAttrModInstance` 的值成员由 `Value` 改为 `Operand`——`FTcsParamValue` 自身内含 `Source`（值从哪来），而该结构另有 `Source`（谁施加的），同结构体一词两义。见 `Attribute.md` §7.2 的 Why Not。

**待裁两题**：

| # | 问题 | 我的倾向 |
|---|---|---|
| **P2** | 族根名 `FTcsParamValueSource` 与具体类前缀 `FTcsParamSource_` 差一个 `Value`——6+ 个具体类派生自它，名字里都不含基类名 | **基类改 `FTcsParamSourceBase`**（与 StateTree `FStateTreeConditionBase` 同构）；6+ 个具体类零改动 |
| **P3** | `FTcsParamValue` 装的是「怎么算出这个数」而不是「一个数」，且在配置态也出现 | **不改**——它是外观，外观名按用途取、不按实现取（同类先例：GAS `FGameplayEffectModifierMagnitude` 内部也是策略分派）。若否决此理由，则成套改 `FTcsParamExpr` 族 |

> **关于 P2 的现状形状**：族内现有两套规律——基类驼峰无下划线（`FTcsParamValueSource` / `FTcsParamEnumerableSource`）、具体类下划线（`FTcsParamSource_*`）。所以「派生名不含基类名」本身不算硬伤；P2 真正裁的是**要不要让基类名自解释**。
> **改名成本为零**：V1 代码与资产整体删除（`03-词汇与命名.md` §5），⇒ P2/P3 只看「新名是否更准」，不看「改动波及多大」。

---

## 6. `Live` / `Snapshot` 的归属（**【已裁 D1】**；语义正在重定）

**触发（2026-11-06 用户质询）**：用户追问「`ParamSource` 直接在运行时使用，意味着参数在运行时不会被修改或更新，即便 `StateLevelArray` 这一类也不会吗」。

**该质询揭出的真问题**：源作为「怎么算」的纯函数，与「算完之后要不要冻住」是**两件事**。前者不可变、两态同一份；后者才是真正有分歧的地方。

### 6.1 【已裁 D1】模式与载体正交，模式归持有者

**D1 原文裁决**：`ETcsParamMode` 与 `FTcsParamValue` **正交**；模式**归持有者**，不归载体。

**【已裁 D38】D1 的业务依据作废，结论保留且更强。** D1 当初记的理由是「`SkillParam`-`SkillModifier` 这条线上确实没有使用 `Snapshot` 的需求」——该理由已被 **D39** 推翻（用户 2026-11-12：「或许 Skill侧 Buff侧 真的也需要 Snapshot 机制」）。

**为什么结论反而更强**：

| 若模式绑在载体上 | 后果 |
|---|---|
| 同一份 `FTcsParamValue` 被 Skill / Buff / Attribute 三处引用 | 三处对「要不要冻」的需求不同，绑在载体上只能有一个答案 |
| §3 的 `Composite` 有 3 个 `Terms` | 3 个载体各带一个模式，求值时**听谁的**？一个表达式不可能有三个求值时机 |
| 布尔开关根本没有载体（存的是 `bool Base`） | 模式绑载体 ⇒ 布尔永久无门，而门 MUST 管布尔（§8.8.5） |

⇒ **D1 不是「因为技能侧不需要冻所以模式别绑载体」，而是「因为每个持有者都要独立决定，所以模式 MUST NOT 绑载体」。** 落地时须订正 §11 台账里 D1 的理由栏。

### 6.2 V1 的实情（**【V1 实证】**，只用于说明 V2 要解决什么）

| 事实 | 证据 |
|---|---|
| 源对象本身不可变 | 四个等级源的 `Evaluate` 全 const；`Values` 是 `EditAnywhere, BlueprintReadOnly`（`TcsParamSource_StateLevel.h:34-35, :68`） |
| 参数值在运行时**有状态** | `FTcsParamSnapshot`（`TcsStateSnapshot.h:47-63`）：施加瞬间求值冻结，实例生命周期内不重算 |
| V1 自称冻结语义归参数域 | `TcsStateSnapshot.h:60-61`：「它是**参数域**的概念（修正器物化、技能侧参数行都要读它），不是状态模块专属」 |
| 模式枚举定义在 | `ETcsParamMode`（`TcsParamRow.h:17-21`），两个值 `Snapshot` / `Live` |
| 真正按它分流的处数 | **半条**：只有 `TcsCastOps_Snapshot.cpp:42-45` 一处读，且是「跳过 Live 行」；`TcsBuffOps_Snapshot.cpp` 完全不读 |
| 修正器路径 | 该文件注释自承：「`EPM_Live` / `EPM_Snapshot` 在本路径上同解」 |
| 快照里预留过 Live 位 | `TcsStateSnapshot.h:38-39` `FInstancedStruct SourceRef`——「Debug 可见取值来源 / **Live 化预留**；无消费者」 |

**【V1 实证】** V1 没有 Curve 版等级源：全仓 `Source/` 扫 `Curve` = 0 命中。只有 `{State,Instigator}Level{Array,Map}` 四种。

### 6.3 V2 的落地要求（D1 的直接推论，仍有效）

| # | 事项 |
|---|---|
| 1 | `ETcsParamMode` 的定义与语义住参数域（`TcsCore/Public/Parameter/`）。**⚠️ 它的持有者已被 D47 改动**：参数行 `FTcsNumericParamRow` **不再带 `Mode`**（改用「出生后是否跟随」的门），今天仍持 `Mode` 的只剩属性域的 `FTcsAttrModInstance`（`Attribute.md` §7.3）与布尔行的现有字段（见 §8.8.7 U1）⇒ 本条的落点须随 D47 重写 |
| 2 | 「此刻该不该求值、求完要不要冻」的判定归**持有者**（D1），参数域只提供语义与类型 |
| 3 | 冻结值的**存储**归持有者（属性实例的 `SnapshotValue`；状态 / Skill 实例的出生值表 `BirthParams`，见 §8.8.3） |
| 4 | 属性域默认 `Live`（修正器本来就实时折叠）；参数域「跟随」的默认值未裁，见 §8.8.7 的 U6 |
| 5 | 求值上下文里的「不要跟随」位控制 `AttributeScaled` 要不要登记依赖边——**落点已变**，见 §4 的 ⚠️ |

**已同步到 Attribute 域**：`04-领域/Attribute.md` §7.2 的 `ETcsValueMode ValueMode` 已删除，改用 `ETcsParamMode Mode`；§7.3 改写为「模式归参数域，冻结位归本域」并补分工边界表与 Why Not。

> **与 §8.8 的措辞关系（已澄清）**：本节第 2 条说「判定归**持有者**」，D49 把「要不要跟随」做成**参数行上的配置项**。两者不矛盾——**门是配置（住在参数行上），执行者是持有者（实例在出生求值时把门值拷进自己身上，此后不可变）**；D1 反对的是「模式绑在载体 `FTcsParamValue` 上」，不是「配置写在 Def 里」。

---

## 7. 参数域自己的修改器（耦合点登记）

**用户原话意旨（2026-11-06）**：「Parameter 也会有属于自己的修改器（比如 Skill 模块的技能修改器），所以可能会影响到 Attribute 域，不过最终应该会在 Parameter 域裁定完再说」。

| 东西 | 改的是什么 | 住哪 |
|---|---|---|
| `FTcsAttrModInstance`（属性域） | 属性的值 | `Attribute.md` §7.2 |
| `FTcsNumericParamModifier` / `FTcsNumericParamModInstance`（V1 技能侧） | **参数的值** | `TcsSkill/Public/Skill/TcsNumericParamModifier.h` |
| `FBoolSwitchModifier` | 布尔开关的值 | **V1 未建**（`R6.5-e` 未开工） |

### 7.1 真正的耦合点 = 折叠器归谁（已由 §7A 结案）

**V1 明文**（`SPEC-05-skill` D5-5 v3，原文见 `dec-02-fold-display.md:33`）：「**折叠器单份住 TcsAttribute**……归属理由：三个消费者的编译边都已含 TcsAttribute（零新边），且不把 Op 枚举与带序词汇搬进零语义的 TcsCore。」

**【V1 实证】** `TcsNumericParamModifier.h:11-12` 直接 include 属性域头 `Attribute/TcsAttrModInstance.h`，注释写「`ETcsAttributeOp`（封闭五带）住 M2——**M5 复用同一套带序**，MUST NOT 另立同义枚举」。⇒ **产品模块反向依赖属性域，只为一个纯函数 + 一个枚举。**

**【V2 推导】** 该理由在 V2 已失效：它是「编译边现状」的描述，不是正确性证明。V2 的模块边界重划后这条边方向可以重选——**§7A 已经选了**（V1 折叠器删除不搬，V2 聚合器即其完整版）。

**与 `Attribute.md` 的接触面**：

| 接触点 | 影响 |
|---|---|
| §3 聚合接线 | 若折叠器上移或迁走，§3.4 的收集 / 求值要跟着改 |
| §7.2 `FTcsAttrModInstance::Operand` | 若参数域消费属性修正器的折叠结果，`Operand` 可能不再是 `FTcsParamValue` |
| §9.2「它不做什么」 | 现在写着「折叠公式 → 《数值聚合器》」，需与 §7A 结论对齐 |

**还剩一问未答**：参数修改器的「作用域」（`CompeteGroup` / `Source` 撤销级联）与属性修改器的「落点」（`ETcsAttrLayer {Basic, Extra, Global}`）形状相似但语义不同，**MUST NOT 强行统一成一个枚举**（同 `ETcsValueMode` 的教训）。这一问属 §10 清单外登记项 A。

---

## 7A. 折叠器归属（**【已裁 D2–D5】**，2026-11-11 已全部落地）

> **原节约 150 行，此处压缩为结论 + 指针。** 完整论证见备份 `Parameter-draft-backup-20261113.md` 的 §7A.1–7A.6。

**触发**：用户直觉「感觉这个折叠器说的很像是我在 V2 一开始提到的核心基线的【数值聚合器】」——**直觉是对的，且比「很像」更强：两者是同一个东西，V1 那份是它的弱化前身。**

### 7A.1 三条实证（压缩）

| # | 实证 |
|---|---|
| 一 | V1 折叠器 3 个调用点（`TcsAttributePipeline.cpp:251` 属性聚合 / `TcsParamChain_Evaluate.cpp:98` 技能参数链 / `TcsFlowAttributes.cpp:45` 伤害流程属性）与《数值聚合器》§4.0 总表的三列（属性 / 参数 / 流程属性）**一一对应，不多不少** |
| 二 | V2 已把 V1 调用形态写成「**V2 初版四带公式的退化形态**」；两边签名同形同语义：V1 `FoldTcsAttributeBands(double, TConstArrayView<FTcsAttributeBandEntry>, ETcsAttrOverrideTieBreak)` vs V2 `EvaluateAggregatedValue(double, TConstArrayView<FTcsAggregateEntry>, ETcsOverrideStrategy)` |
| 三 | V1 的带权助手 `GetTcsAttributeBandWeight`（`TcsAttrModInstance.h:41`）全仓 **1 处命中 = 它自己的定义**，零消费者；《数值聚合器》§5.3 标题即「不提供带权数字」⇒ **V2 早已预判并否掉了它，只是当时不知道 V1 里存在** |

### 7A.2 【已裁 D2/D3/D4】

用户原话：「按照你给的倾向建议执行，至于 SourceHandle，**聚合器本身只执行计算，确实不需要保存 SourceHandle**，但是调用聚合器的几个域，却需要保留 SourceHandle」。

| # | 结论 |
|---|---|
| D2 | **V1 的 `FoldTcsAttributeBands` 删除，不搬**——V2 聚合器严格更强（N 段 / 3 覆盖 / 7 策略 vs 单层 / 1 覆盖 / 固定三级）；「搬到核心基线」这个选项不存在，聚合器**本来就是**核心基线（五项之一） |
| D3 | **覆盖比较维持单级**（《数值聚合器》§3.1 原裁决不变）：7 个策略互斥、无隐藏更高优先级规则。V1 那套「优先级 → TieBreak → 有符号值」三级比较**正是** §3.1 要否掉的「隐藏规则」 |
| D4 | **聚合器条目不持有 `FTcsSourceHandle`**，只持 `uint64 OrderSeq`（施加序） |

### 7A.3 两层分工（D4/D5 的形态）

| 层 | 持有什么 | 为什么 |
|---|---|---|
| **聚合器**（纯函数） | `uint64 OrderSeq` | 它只做**比较与累加**；比较需要的是序关系，不需要句柄的身份与有效性 |
| **域**（属性 / 参数 / 流程） | `FTcsSourceHandle Source` | 域要的是**级联撤销锚点**（`RemoveBySource`）与 **UI 归因**（「被【破甲】覆盖了」）。那是句柄的完整语义 |

**降型三条判据**：① 耦合——聚合器零领域词汇，`FTcsSourceHandle` 的 `IsValid()` / `operator==` / 发号器语义它一个都不用，只用 `.Id` 却持整个句柄是过度耦合；② 反射友好——`FTcsSourceHandle` 是非反射纯 C++ struct（`TcsSourceHandle.h:20` 裸 struct），它的存在直接禁止聚合器成为 `USTRUCT`；换成 `uint64` 后其余字段都是反射类型；③ 铁律④ 的镜像——字段名叫 `Source` 而只读 `.Id`，读代码的人会以为聚合器在管身份。

**【已裁 D5】** 明细的 `OverrideWinner` 降为 `OverrideWinnerSeq`（序号），域拿序号回查自己的账本。判据 = 《数值聚合器》§3.3 自己定的「明细只放调用方拿不到的量」，而来源句柄恰恰是调用方给的（`Entries` 是它递进来的）。域反查是 O(n)，而明细是 UI 路径（§3.3 已裁「MUST NOT 进热路径」）⇒ 成本可接受。

### 7A.4 一处硬矛盾已解决（2026-11-11 落地）

《数值聚合器》§3.1 与 §4.3 曾互相打架：§3 的条目结构必须有 `Source` 字段，而 §4.3 明令「流程域 MUST NOT 有 `Source` 字段」。**【V1 实证】** V1 没有这个矛盾，因为 V1 的条目结构里根本没有 Source（`TcsFlowAttributes.cpp:34-41` 只填 `Op / Value / OverridePriority`）⇒ 引入 `Source` 时那条裁决从未被重新检查过。

**降成 `uint64 OrderSeq` 顺带解决它**：流程域填 `OrderSeq = 0` 不是「有 Source」，而是「**不排施加序**」——`LatestApplied` / `EarliestApplied` 对全 `0` 的集合恒打平，落到按值比较，这正是流程域该有的行为。五处已全部对齐：§3 条目结构 / §3.2 标题与正文 / §3.3 明细 / §4.3 流程域 / §8.2 零依赖句。

### 7A.5 连带：同源刷新的时机（已统一为「写时重算」）

《数值聚合器》§4.1 原定「惰性」、`Attribute.md` §2.2 已裁「写时重算」⇒ 统一为**写时重算**，三条理由：① **链步骤是同步的**，`ApplyAttributeSet` → 行求值 → 行为链再入起链全在同一帧同一调用栈，「先写后读」必须读到新值；② **读侧要保持纯内存返回**，`AttributeScaled` 跨实体读，读时重算会连锁唤醒另一侧，且宿主源回调里再读属性会**求值中途重入**；③ **批已经能表达「这一批算一次」**，惰性想省的「多次写只算一遍」由 `BeginBatch` / `CommitBatch` 解决。

> **一处必须分清**：批内读到的是批开始前的值，这**不是惰性的残留**。批的语义是「要么一起生效、要么都还没生效」。**批内不读，与「写时不重算」是两件事。**

---

## 8. 调研实证：能力已建、契约悬空

> 每一节都是全仓扫描的直接产物，**每条都可复核**。共同形态：类型在册、有默认降级路径、**没有实现者** ⇒ 配了它不报错、只是静默落兜底。
> ⚠️ **统计口径**：本节「0 命中」类结论**只扫了 `*.h` / `*.cpp`，未扫 `*.cs`**——这个错我犯过一次（8.3），V1 也犯过同一次（`plan-r6-skill-layer.md:1024` 自记）。引用这类结论时 MUST 说明扫了什么。

### 8.1 `ITcsAttributeProvider` —— 0 实现者 ⇒ `AttributeScaled` 恒落兜底

- 声明 `TcsAttribute/Public/Attribute/TcsAttributeProvider.h:27`；唯一消费者 `TcsParamSource_AttributeScaled.h:105`。全仓 `public ITcsAttributeProvider` = **0 命中**。
- **后果**：`FTcsParamSource_AttributeScaled::Evaluate` 恒走 `Provider.GetObject()` 判空 ⇒ **恒返回 `Fallback`**。代码自承：`TcsSkillDefData.h:108`「全仓无实现者……配了它只会落兜底值」。
- **另有一层更硬的阻塞**：`Evaluate`（`TcsParamSource_AttributeScaled.h:93-96`）要求 `Context.GetScriptStruct()->IsChildOf(FTcsAttributeEvaluateContext::StaticStruct())`，而 **`FTcsAttributeEvaluateContext` 全仓只有声明、没有构造点**（`TcsBuff/Public/Host/TcsEntityLevelProvider.h:70` 只在注释里把它当先例提了一次）⇒ 即使补上 Provider，上下文类型不匹配仍然走 `Fallback`。
- **【已裁 D13/D14】** 删除 Provider；`FTcsParamEvaluateContext` 增 `TObjectPtr<UWorld> World`，源自己调 `UTcsAttributeSubsystem::Resolve(Context.World)`。

### 8.2 `ITcsEntityLevelProvider` —— 0 实现者 ⇒ 四个等级源只有两个能用

- 声明 `TcsBuff/Public/Host/TcsEntityLevelProvider.h:40`；消费者 `FTcsParamSource_InstigatorLevel{Array,Map}`。全仓 `public ITcsEntityLevelProvider` = **0 命中**。
- **后果**：`StateLevel{Array,Map}` 读 `Context.EffectiveLevel`（纯字段）⇒ 可用；`InstigatorLevel{Array,Map}` 读 `LevelProvider`（空）⇒ **恒落 `Fallback`**。
- 这是一条**只能靠读代码发现**的结论：四个源在同一文件里、签名对称、注释也对称，但**其中两个从未工作过**。
- **【已裁 D8/D9/D10】** 四个源 + 本接口**全保留**，住 State 域。接口是 (a) 类宿主契约，**MUST NOT 因零实现而砍**（用户原话：「TCS 本身是战斗系统插件，开发的很多功能就应该只有宿主项目侧才会使用到」），也**不补实现**。
- **附带（技能侧不可用）**：`TcsSkill` 内 `FTcsStateEvaluateContext` / `LevelProvider` 只有三条注释，`TcsCastOps.h:360` 明写「`ITcsEntityLevelProvider` 住 TcsState，技能侧参数源本轮不消费它」⇒ **四个等级源在技能侧全部不可用**。

### 8.3 `ITcsParamSourceHost` —— **我的统计错了，它有 C# 实现者**

- ❌ 我最初写「0 实现者」，**错**。V1 验收记录 `evidence/2026-10-04-state-param-snapshot-and-level-sources.md:124` 明写：宿主脚本夹具 **`UTcsDevGcParamSourceHost`**（C# / UnrealSharp）实现过它，且做过真实原生 GC 后的存活验证。**错因 = 只 grep 了 `Source/**/*.h|*.cpp`，没扫 `*.cs`。**
- 夹具随 V1 一起删除（当前 `Content/` 与 `Script/` 都没有），但**它证明了这条路走得通**。
- **【已裁 D11】仍然删除**，理由不是「零实现者」，而是三条：① 它把两件事塞进一个源——「条件」归触发/流程（`WaitEvent` + `Branch`），「值计算」归参数源；② C++ 宿主派生 `FTcsParamValueSource` 已覆盖同需求（`meta=(BaseStruct)` 让宿主的源自动出现在 picker 里，零插件改动）；③ 脚本宿主有别的路。
- **【V1 实证】** 用户举的「低血量时移速 +30%」在 V1 里每一环都已存在：`FTcsStepWaitEvent`（`TcsStepWaitEvent.h:39`）+ `FTcsChainEventWaitRegistry`（同 tag 的 N 条等待只订阅一条）+ `FTcsStepBranch` + `FTcsStepApplyState`。**三个步骤，零新机制。**

### 8.4 `ITcsSkillCostPolicyHost` —— 0 实现者，宿主自定义消耗策略从未被用过

- 声明 `TcsSkill/Public/Skill/TcsCostPolicyHost.h:66`；后果 `FTcsSkillCostPolicy_HostDelegate` **fail-closed**（`CanAfford` / `CommitCost` 均返回 false）。
- **V2 尚未就此条裁决**，登记在 §10 清单外。⚠️ 统计口径同 8.1。

### 8.5 `FTcsParamSource_ParamRef` —— 只在少数路径真能取到值

参数表 `Context.ParamTable` 的各处填法（实证）：

| 调用点 | `ParamTable` | 后果 |
|---|---|---|
| `TcsBuffModifierMaterializer.cpp:35` | ✅ 绑该实例值表 | 可用 |
| `TcsCastOps_Phase.cpp:55` | ✅ 绑 run 值表 | 可用 |
| `TcsCastOps.cpp:554` | ✅ 绑 run 值表（物化 `ParamChainRows`） | 可用 |
| `TcsCastOps.cpp:39`（`MakeContext`） | ❌ 恒空（`:37-39` 注释自承） | `ParamRef` 落兜底 |
| `TcsCooldownOps.cpp:150` | ❌ 恒空 | 同上 |
| `TcsCostOps.cpp:42` | ❌ 恒空 | 同上 |
| `TcsParamChain.cpp:128`（外部施加） | ❌ 恒空（`:106-107`「账本外的一次写入，没有本次求值的参数表可言」） | 同上 |
| `TcsParamChain_Evaluate.cpp:114` | ❌ 恒空 | 同上 |
| `TcsStepApplyState.cpp:77` | ❌ 恒空（注释自承「链侧参数表载体不存在」） | 同上 |

⇒ **`ParamRef` 不是一个「到处能用」的源**，这是一条**文档从未写清、只能靠逐点追踪才能发现**的口径。**出生求值阶段（`MakeContext`）恒空**这一点尤其要紧：它意味着今天所有 `ParamRef` 参数行在出生时都读不到任何东西。

### 8.6 `ETcsParamMode` —— 配置面三处，真正按它分流的只有半条（**【V1 实证】**）

> 本节 2026-11-07 全面改写。初稿写「1 个真消费者 + 2 处零读取者」，**错了**——见实证。

#### 实证 A：`Mode` 只影响值表，值表又不是唯一读路径

| 地方 | 读的是什么 | `Mode` 生效吗 |
|---|---|---|
| `TcsCastOps_Snapshot.cpp:42-45` | 构建值表 | ✅ **Live 行被 `continue` 跳过、不进表** |
| `TcsParamChain_Evaluate.cpp:145-153`（`EvaluateParamKey` 的初值） | **直接遍历 `Def->Params` 求值** | ❌ 完全不看 `Mode` |
| `TcsBuffOps_Snapshot.cpp:52-80`（Buff 侧构建） | 构建值表 | ❌ 完全不看 `Mode`（同一件事，另一处实现） |

**⇒ 关键矛盾**：`Mode = Live` 的行**不进表**，可 `EvaluateParamKey` 又**不看 `Mode`、直接求值**。两者凑一起的结果是：同一个键，两条读路径给出两个不同的数——命中参数链折叠时 `Live` 与 `Snapshot` **差别为零**；命中参数表读取时 `Live` 行的键**查不到、落 `Fallback`**。

#### 实证 B：Buff 侧与技能侧对同一字段处理不一致

| | 技能侧 `BuildSkillSnapshot` | Buff 侧 `BuildSnapshot` |
|---|---|---|
| 检查 `Row.Mode` | ✅ `:42` | ❌ **没有这一行** |
| 其余逻辑（`SourceRef` / `Override` / 值约定 / `AllowsValueConvention`） | 逐字相同 | 逐字相同 |

两个函数体是**复制关系**，只在 `Mode` 这一处分叉。这不是疏忽——`TcsParamRow.h:88-95` 与 `TcsBoolSwitchRow.h:30-33` 都在注释里**如实承认**了缺口（「状态/buff 侧与布尔开关表上的本列目前零读取者 ⇒ 在那两处填实时，行为与快照相同、且不会有任何提示」）。

#### 实证 C：`Mode` 与载体的字段分布**强负相关**

| 统计项 | 数字 |
|---|---|
| `FTcsParamValue` 作为字段 | **20 处，跨 6 模块** |
| 这 20 处里所在结构体带 `Mode` 的 | **1 处**（`FTcsNumericParamRow`） |
| `ETcsParamMode` 作为字段 | **2 处** |
| 这 2 处里所在结构体含载体的 | **1 处**（同上） |
| 剩下那处 | `FTcsBoolSwitchRow::Mode`——所在结构体**连载体都没有**，存的是 `bool Base` |

**19 / 20 处载体没有模式列**，而有模式的那一处（布尔开关）连载体都没有。

#### 实证 D：代码自己写明「在本路径上同解」

`TcsCastOps_Snapshot.cpp` 文件注释原文：「`EPM_Live` / `EPM_Snapshot` 在本路径上同解：两者都走实时折叠（快照只影响快照面）」⇒ 模式的差异**被抹平**。只有「模式不是载体属性、而是某条路径要不要冻结的决策」才可能产生这句话。

#### 实证 E：§3 的 `Composite` 把「模式属于载体」推成荒谬

`Attack = Strength×2 + Agility×0.5` 的表达式树里有 **3 个载体**，各带一个 `Mode`。求值时**听谁的？一个表达式不可能有多个求值时机** ⇒ 模式必然在载体之外。

#### 结论（**【已裁 D1】**）

**模式与载体正交，模式归持有者。** 详见 §6.1（含 D38 的订正：原业务依据已被 D39 推翻，结论反而更强）。

### 8.7 `TcsNotation` 模块只有一个内容

`Source/TcsNotation/Public/` 全部内容 = `TcsValueConvention.h`（57 行）+ 日志通道。它是一个**独立编译单元 + 一条模块依赖边**，换一个 57 行的枚举与静态助手。机制 14 的 V2 处置已写：「**机制保留，模块取消**——值约定并入『数值与参数』域，不再独立成模块」。

> **这是 §9 的一个重要输入**：如果「值约定」并入参数域，参数域就是**第一个有真实跨模块内容物的域**；如果它留作核心基线，「并入」这个词就没有落点。

---

### 8.8 冻结机制：一个值绑在哪个时刻（**方向已批，细节逐条确认中**）

**触发**：§8.6 记的一条前提被推翻。原文记的是 2026-11-07 用户对 D1 的业务确认「在 `SkillParam`-`SkillModifier` 这条线上确实没有使用 `Snapshot` 的需求」。2026-11-12 用户推翻它（**D39**）：

> 「不对劲，或许 Skill侧 Buff侧 真的也需要 Snapshot 机制。比如一个 SkillEntry 已经因为玩家操作激活了 一次 SkillInstance，其内部有一个 BoolSwitch 或者 影响前摇时间 的 Numeric，开着可能允许 一个 SkillModifier 在这个 SkillInstance 过程中影响，也可能不希望。」

⇒ 问法随之更换（**D40**）：**不是「`FTcsParamSnapshot` 该搬到哪个模块」（那个问法预设了「要搬」），是「一个值绑在哪个时刻」。**

#### 8.8.1 方法论纠正：本节前一版有三处 V1 锚点（**已认账，本轮删除**）

用户 2026-11-13 两次纠正：「不要过度参考 V1，不然为什么要重新设计V2」/「V2 现在是要 重新设计」。据此复查本节前一版，有三处**从 V1 的类型形状倒推**，不是从 V2 需求推导：

| # | V1 锚点 | 它其实是 |
|---|---|---|
| 1 | `ETcsParamMode { Snapshot, Live }` | 就是 V1 的 `FTcsNumericParamRow::Mode`（`TcsParamRow.h:101`）。**一个 `Mode` 位把三个互不蕴含的问题压成一个**（见 8.8.2） |
| 2 | 「`Live` 行在实例上存**源副本**」 | 就是 V1 的 `FTcsParamSnapshotEntry::SourceRef`（`TcsStateSnapshot.h:38`）——一个**全仓零真读者**的字段（13 处命中 = 写入 2 + GC 追踪 2 + 注释 9），只换了名字 |
| 3 | 「读口**按行分派**」 | V1「值表兼任参数表」的遗留。**只有先有那张冻结表，才会出现「`Live` 行不在表里」，才需要按行分派**。不建那张表，这个缺陷不会诞生，也就不需要修 |

⇒ **前一版一直在回答「V1 那张表该怎么修」，而不是「V2 需要什么」。** 原 §8.8.7（约 460 行）的 `FTcsParamBinding` / 源副本 / 按行分派 / `FTcsParamBindingScope` 改造已整体删除，D42–D46 相应作废（见 §11）。

#### 8.8.2 关键拆解：一个 `Mode` 位把三个互不蕴含的问题压成一个

| # | 真问题 | 由谁回答 |
|---|---|---|
| ① | **基底表达式何时求值**：出生一次，还是每次读都回源？（决定「世界中途变了，这一发跟不跟」） | 参数域 |
| ② | **中途到来的修改器能不能写进这个键**（用户原话的「开着可能允许…也可能不希望」——原话是**权限**，不是副本） | 参数域 |
| ③ | **已物化进属性账本的那条修正器，跟不跟参数走** | Attribute 域（`Attribute.md` §7.3 的 `SnapshotValue`，已落地已裁） |

**【V1 实证】** V1 用一个 `Mode` 位硬捆 ①②，且两个域各实现半条（§8.6）。**V2 照抄等于继承这个捆法。**

#### 8.8.3 已批方向（**D47**）：基底出生求值 + 写入门 + 账本折叠

用户 2026-11-13 原话：「基本上，是按照『基底出生求值 + 写入门 + 账本折叠』重做，只不过思路本身仍然有可以讨论和确认的内容」⇒ **方向已批，细节 MUST 逐条确认，不许直接当定稿。**

```cpp
// 参数行：去掉 Mode，只剩「我叫什么、我出生时值多少、我怎么换算」
struct FTcsNumericParamRow
{
	FGameplayTag       Key;
	FTcsParamValue     Base;             // 出生求值的输入
	ETcsValueConventionFlag ValueConvention = VCF_None;
};

// 实例侧（取代 FTcsParamSnapshot 的角色）：出生那一刻求值一次，此后不回源
struct FTcsParamBirthEntry
{
	FGameplayTag Key;
	double       Value   = 0.0;   // 出生那一瞬的规范值（已含条目账本修正）
	bool         bFollow = false; // 出生之后是否继续跟随账本；默认固定
};
```

**★ 与 V1 的分界（本节最关键的一条）**：**值表恒装全部键**，门只决定「出生之后是否继续跟随」，**不决定「是否进表」**。V1 的 `TcsCastOps_Snapshot.cpp:42-45` `if (Row.Mode == EPM_Live) { continue; }` 把 `Live` 行**排除在表外**，才生产出「`ParamRef` 引用 `Live` 键永远落 `Fallback`」；新骨架里 `Live` 键同样有出生值，`ParamRef` 在出生求值阶段读到的是同一时刻的值，与 `Snapshot` 键**完全对称** ⇒ 该缺陷的成因不存在。

#### 8.8.4 已裁（**D48**）：**译法 B**——门管读取，不管写入

用户 2026-11-13：「我个人也倾向于 译法 B」。

| | 译法 A（管写入） | **译法 B（管读取）** |
|---|---|---|
| 门在哪 | 修正器到达时判 `TryGetGate(Key)==Closed` ⇒ 仍写条目账本但不进本次施法 | 读口里若 `bFollow==true` 走实时通道，否则只认 `BirthParams` |
| 实例是否要自己的账本 | **要**（折叠变成 `Fold(Instance.Ledger, Key, Base)`） | **不要**（账本仍只有条目那一份） |

**三条倾向 B 的判据**：① A 让「摘除完整性」由一处变两处（`TcsParamChain.cpp:176-178` 已明写「不存在摘不掉的条目」是成对性判据；实例账本要能被按来源摘除，且实例随时终结，两生命周期不同步的缺陷形态与 §8.6 同类）；② A 让折叠多一条路径（`FoldParamChainInstances` 现在只有 `TcsParamChain_Evaluate.cpp:67` 一个定义、`:173`/`:186`/`:198` 三个调用点全在同一文件，是模块内唯一折叠收束点）；③ B 不改现状存储形状。

```cpp
bool UTcsSkillSubsystem::TryGetInstanceParam(FTcsSkillInstanceHandle Run, FGameplayTag Key, double& Out)
{
	const FTcsSkillInstance* Inst = FindRun(Run);
	if (!Inst) return false;

	for (const FTcsParamBirthEntry& E : Inst->BirthParams)
	{
		if (E.Key != Key) continue;
		if (!E.bFollow) { Out = E.Value; return true; }                 // 门关：只认出生值

		const FTcsLearnedSkillEntry* Entry = GetEntry(Inst->Entity, Inst->EntryHandle);
		const FTcsSkillDefData*      Def   = FindDef(Inst->DefTag);
		return FTcsParamChainOps::EvaluateParamKey(*this, *Entry, Def, Key, Out);  // 门开：实时通道
	}
	return false;   // 键不在值表 = miss（同 EvaluateParamKey 语义，MUST NOT 静默返回 0 当命中）
}
```

**实现便利**：`EvaluateParamKey`（`TcsParamChain_Evaluate.cpp:132-175`）已经天然是「条目通道」（回查 `Def->Params` 拿初值 + 折叠 `Entry.NumericParamModInstances`）⇒ `bFollow` 分支**不需要新写求值逻辑**。

**B 的两个甩不掉的后果（用户未反对）**：

| # | 后果 |
|---|---|
| ① | **中间态修正器永远进条目账本**：`ApplyParamModifiers`（`TcsParamChain.cpp:113-143`）完全不动，照旧写 `Entry.NumericParamModInstances`。门只让**这一发看不见**，不让它不写入 ⇒ 门关的那一发里 +30% 对**下一发**仍有效 |
| ② | **悖论就地消解为两层各管一件事**：参数层的门管「这一发是否继续跟随条目账本」（问题 ①②）；属性层 `FTcsAttrModInstance::SnapshotValue` 管「已物化进属性账本的那条修正器是否跟随参数走」（问题 ③）。两者**不重叠** |

#### 8.8.5 已裁（**D49**）：门 = **甲**（配置定、出生拷贝、此后不可变），且 **MUST 管布尔**

用户 2026-11-13：「我也倾向用甲」+「门管不管 布尔参数？管的，必须要管的」。

| 候选 | 做法 | 判 |
|---|---|---|
| **甲** | 配置定（参数行上一个「出生后是否跟随」的字段），出生求值时读一次写进 `bFollow`，此后**不可变** | ✅ **已裁** |
| 乙 | 运行时可变：实例上留可写位 + `SetInstanceParamGate(...)`，配置只给初值 | ❌ 谁调、何时调未定 |
| 丙 | 不做门：一律跟随（取消冻结）或一律固定（取消实时） | ❌ 两端的真实需求都被删掉 |

**倾向甲的三条判据**：① 用户原话「开着可能允许…也可能不希望」表达的是**作者意图**，不是运行时事件；② 乙今天连写入源都没有——唯一像运行时开关的 `FTcsBoolSwitchRow` 自述（`TcsBoolSwitchRow.h:30-33`）「`Mode` 列零读取者、`IsSwitchSet` 只读 `Base`、布尔修正器（`FBoolSwitchModifier`）整体归 `R6.5-e`，今天没有『改开关』的机制」⇒ 选乙必须同时建布尔修正器机制作前置；③ 本仓纪律「零消费者不预建」（`TcsStateInstance.h:51-53` 原文用来否掉 `Phase` 字段）。

**额外好处**：门值与 `BirthParams` 在同一时刻、同一次求值落地 ⇒ **门本身也是被冻结的值**。

#### 8.8.6 已裁（**D50**）：门只管 `Params` 行；无 Key 的值由代码定冻结点

用户 2026-11-13 选「甲：门只管 `Params` 行」。据此把「非参数行的值」分成三类，**各归各位**：

**(a) 不是键值对的字段** —— 不进参数账本、根本没有 Key：`FTcsPhaseSpan::Duration`（`TcsPhaseSpan.h:24`）、`FTcsCooldownTrack::Duration`（`TcsCooldownTypes.h:92`）、消耗 `Amount`、链步骤内联值。
⇒ **不需要门也不需要 Key**：冻结语义由源类型决定（`Literal` 天然冻结；`ParamRef` 间接冻结，因它读的是实例值表、出生一次；`AttributeScaled` 天然实时，但今天恒落 `Fallback` 不可达，见 §8.1）。给它们造 Key 就等于塞进参数账本，而 `TcsBuffOps_Snapshot.cpp:91-96` 已把「把时值塞进快照会引入一个『时值的参数键』——那是插件自造参数表键」记成边界。**另外**：`FTcsNumericParamModInstance` 必带 `ParamKey`（`TcsNumericParamModifier.h:262`）⇒ 无 Key 值本来就没有账本写入通道。

**(b) 基数在 C++ 字段、修正挂插件原生契约键**（即用户 m01683 指出的 `Cooldown` / `Level` 这类）——**全仓只有三个**：

| 键 | 冻结点 | 证据 |
|---|---|---|
| `Tag_TcsStateParam_Level` | **实例建立那一刻** | `TcsCastOps.cpp:568-571`：`LevelRun->Level = EvaluateEffectiveLevel(...)`；`:562-563`「等级定了之后本次施法不变」 |
| `Tag_TcsSkillParam_CooldownPct` | **起冷那一刻，全场只取一次** | `TcsCooldownOps.cpp:129-131`：「★ CDR 只在此刻取一次（D5-15），取多次会让同一批轨道拿到不同系数」 |
| `Tag_TcsSkillParam_CostAmount` | **每次现算，且刻意算两次** | 门禁 `TcsCostOps.cpp:215-216` / 提交 `:338-339`；`:336-337`「本行 MUST NOT 被优化成沿用门禁那次的结果：门禁与提交之间隔着建 run / 快照 / 物化等步骤，期间修正器可能已变」（TOCTOU） |

⇒ **三个冻结点各不相同，且这个不同是刻意的** ⇒ **D50 不是遗漏：这些值的冻结时刻本来就该由代码定、不该由作者配。**

**(c) 布尔开关** —— 有 Key，但今天零修正器通道、实例零存储：`IsSwitchSet`（`TcsSkillOps_ParamRead.cpp:128-134`）只返回 `Row.Base`，`:137-138`「两表互不兜底」。门 MUST 管布尔（D49），但落地形态未裁，见 8.8.7。

**⇒ 顺带发现三处静默错误**（作者往 `Params` 写契约键的后果，各自不同；`TcsState/Private/Def` 内对 `Params` / `BoolSwitches` 的校验 **grep 零命中**，今天没有拦截）：

| 写哪个键 | 后果 | 依据 |
|---|---|---|
| `Level` | **两条路径结果不一致** | `EvaluateEffectiveLevel` 初值取 `Def->LevelBase`（`TcsParamChain_Evaluate.cpp:194`），而 `EvaluateParamKey(Level)` 初值取 `Params` 里那行的求值（`:145-153`），两者用同一折叠器但初值不同。`:203-204` 注释「`Level` 键不出现在普通参数折叠结果里」只是**假设**作者不会配 |
| `CooldownPct` | **有效配置**（配行 = 配 CDR 基数） | `EvaluateParamKey` 会查 `Def->Params`。三者中唯一「作者写契约键有意义」的 |
| `CostAmount` | **配了被静默忽略** | `FoldParamKeyWithBase` 不回查 `Def->Params`（`:182-183`） |

⇒ 建议在 `FTcsSkillDef::IsDataValid` 补校验：`Level` 与 `CostAmount` 进 `Params` 报错，`CooldownPct` 允许。

#### 8.8.7 未裁 / 待确认清单（**本节是当前讨论前沿，不是定稿**）

| # | 待裁 / 待确认 | 说明 |
|---|---|---|
| **U1** | 布尔门的落地形态 | **甲-1**：布尔进实例、与数值对称；但今天 `bFollow==true` 分支读 `IsSwitchSet` 仍返回 `Row.Base` ⇒ **门在布尔上空转、不可验证**，与「零消费者不预建」紧张。**甲-2**：`FTcsBoolSwitchRow` 上先配出门位，实例侧拷贝推迟到 `R6.5-e` 与布尔修正器同批落地；代价 = 短暂「配置了但没生效」，正是 `TcsCastOps_Snapshot.cpp:37-41` 自己骂过的形态 |
| **U2** | **出生求值的输入是什么** | MUST 是**条目通道的读数**（`GetNumericParam` = `Def` 初值 + 条目账本），不是 `Def` 初值。走查：「面板写 120、这一发打 100」玩家会认为是 bug。**现状不符**：`BuildSkillSnapshot`（`TcsCastOps_Snapshot.cpp` 全文）**完全不读** `Entry.NumericParamModInstances` ⇒ 现状出生值 = 纯 `Def` 初值 |
| **U3** | **条目通道多 run 并存时没有唯一答案** | `TcsSkillOps_ParamRead.cpp:82-100` 自述：`GetNumericParam(Entity, EntryHandle, Key)` 没有 run 段，而 `CI_InstancePerExecution` 下同一 `Entry` 可有两个 run 并存 ⇒「读哪一次施法的值」无唯一答案。**`BirthParams` 若挂 run 上，UI 读哪个 run？若挂条目上，又与「这一发期间冻结」矛盾。** 这是最重的一条 |
| **U4** | 两层结构的层级差 | **Skill 有两层**（跨施法的 `FTcsLearnedSkillEntry` 与一次施法的 `FTcsSkillInstance`）；**Buff/State 只有一层**（`FTcsBuffInstance` 直接派生 `FTcsStateInstance`，无「学会」概念，施加即建档）⇒ 出生值表（`BirthParams`）挂哪一层，两域形态不同 |
| **U5** | 时段时长与 `TcsSkillDefData.h:123` 的注释矛盾 | 注释写「时段时长在激活时快照化」，实际 `TcsCastOps_Phase.cpp:191` **每次进入时段现算**（用 `Run->ParamSnapshot` + 当前 `Level`）。按 8.8.6(a)，它应按源类型决定语义 ⇒ 注释须改，代码待议 |
| **U6** | 「跟随」的默认值 | 数值行默认固定还是默认跟随？门若默认固定，则今天所有实时行为都会变；若默认跟随，则「作者要显式关」。**未裁** |
| **U7** | 旧 `FTcsParamSnapshot` / `UTcsStateParamTableReader` / `FTcsStateSnapshotScope` 的处置 | 若值表改成 `BirthParams` 且门在读取侧，则「值表兼参数表」的前提消失 ⇒ 这套读取壳（`TcsStateParamTableReader.h` 全文）**要不要留、改成什么**未裁 |

> ⚠️ **以上任一 U 项落进族内文档前，MUST 先经用户逐条确认。** 本节方向已批（D47），细节未确认。

#### 8.8.8 原 §8.8.1–8.8.6 的结论 R1–R5（**部分已作废，只保留仍然成立的部分**）

| # | 原结论 | 现在 |
|---|---|---|
| R1 | 冻结是机制、不是类型 | ✅ 成立（`BirthParams` 就是把它做成数据而不是「一张表的类型」） |
| R2 | 宿主三种：State 实例（含 Buff）/ Skill 实例 / 有自己参数行的 Area 实例；Area 的「每跳」是读取频率问题 | ⚠️ **推断**（V1 零 `Area`，`grep Area Source` = 0 命中；`TcsDamage` 侧零 `ParamSnapshot`） |
| R3 | V1 的错不在「冻」，在把「一次求值」做成「一张按键可查的表」 | ✅ 成立，是本轮删掉源副本与按行分派的依据 |
| R4 | 两层冻结不等价、参数级不可省 | ✅ 成立（8.8.4 后果②） |
| R5 | `ParamRef` 不需要自己的 `Mode`，但需要按行分派的读口 | ❌ **作废**：新骨架里值表恒装全部键，不需要按行分派 |

---

## 9. 三选一：参数各部分该住哪（**待 §10 走完后才裁**）

> **本节不是裁决，是把选项摆出来备查。** 用户已明确「最后再确定这三个选项」。

### 9.1 判据来自哪里

`INDEX.md` §2.2 的原文判据：

> 一个设施**不含任何领域词汇**、**被 ≥2 个领域依赖** ⇒ **核心基线**。含领域词汇 ⇒ 属于那个领域。

而 `INDEX.md` 已把 Parameter 列为**七个领域**之一。**这里存在一个未解决的张力**：按 §0 的调研，这个「域」的大部分内容不含领域词汇、且被 7 个模块依赖。

### 9.2 三个选项

| | **A：全部并入核心基线** | **B：拆成基线 + 领域** | **C：维持一个域** |
|---|---|---|---|
| 形态 | 载体系 + 模式 + 修改器全进 `TcsCore`（或新增核心基线「数值源」） | `02-核心基线/数值源.md`（载体 + 源族 + 上下文 + 值约定）+ `04-领域/Parameter.md`（门 + 修改器 + 参数表 + 出生求值） | 全部进 `04-领域/Parameter.md`，属性/伤害反向依赖它 |
| 符合 §2.2 判据 | 前半（载体）符合，后半（门/修改器）不符合 | ✅ 逐件符合 | ❌ 让 Attribute 域依赖一个含领域词汇的域 |
| `Attribute.md` 要改吗 | §7.2 / §7.3 大改 | §7.3 只需指向基线文档 | §3.4 与 §7.2 都要改 |
| 折叠器（§7.1 三问） | 随载体系进基线 | **落基线**（它无领域词汇） | 落参数域，属性域反向依赖 |
| 值约定（§8.7） | 进核心基线 | 进 `数值源` 基线 | 进参数域 |
| 风险 | 模式枚举被无差别塞进基线；`TcsCore` 变成杂物间 | 一个逻辑概念横跨两份文档，读者要跳 | 依赖图出现「属性 → 参数 → 属性」的隐式环 |

### 9.3 我倾向 B，但**判据要等 §10 确认**

理由是「拿消费者数量划边界，比拿名字划边界可靠」：`FTcsParamValue` 今天有 **7 个模块**的真实消费者 ⇒ 基线；门（`bool bFollow` 那个位）今天只有**技能 / 状态类宿主** ⇒ 领域；`FTcsNumericParamModifier` 只有技能域 ⇒ 领域。

**D1 裁完后这个理由变强了**：模式与载体已被确认正交 ⇒ **B 的拆分不会劈开一个概念**，原本支撑 A 的唯一前提消失了。

但**这不等于 B 已经成立**：正交只证明「可以拆」，没证明「应该拆」。剩下要答：

| 待答 | 落点 |
|---|---|
| 拆分后「参数域」还剩下什么？ | §10 第 8、9 条 |
| 折叠器归属会连带决定载体系归属吗？ | §10 第 2 条 |
| 「域」与「模块」是两个轴，文档该怎么写？ | §10 第 3 条 |

> **A 并未被排除**：如果拆分后「参数域」只剩两三个类型，那它也许不该独立成一个域，而是并进别的域（如 Attribute 或 State）。这要等 §10 走完才能判断。

---

## 10. 逐条确认清单

> **走法**：一次确认一条，确认结果就地改写本节状态。§9 的三选一在全部条目走完后裁。

| # | 要确认的问题 | 我的当前判断 | 状态 |
|---|---|---|---|
| 1 | **`ETcsParamMode` 与 `FTcsParamValue` 是不是同一个概念的两面？** | 不是——模式描述「**什么时候求值**」，载体描述「**怎么算**」，两者正交 | ✅ **已裁 2026-11-07（D1）：正交 + 模式归持有者**；业务依据已由 D38/D39 订正。连带定下第 3 条 |
| 2 | **折叠器归谁？** | 不是「归谁」，而是**V1 那份直接删除**——V2《数值聚合器》就是它的完整版，且已是核心基线 | ✅ **已裁 2026-11-08（D2–D5），三条遗留已于 2026-11-11 全部落地**（详见 §7A） |
| 3 | **「住 TcsCore」与「属于 Parameter 域」是不是同一件事？** | **不是**——两个轴 | ✅ **已裁 2026-11-09（D6）**。连带：参数表归参数域（D7） |
| 4 | **源族保留集** | 保留 | ✅ **已裁 2026-11-09（D8/D10）**：四个等级源 + `ITcsEntityLevelProvider` 全保留、住 State 域。我原提议的「改走 `ITcsEntityQuery`」**已证伪**（该接口没有 `GetLevel`，且是 C++ 专用面） |
| 5 | **`HostDelegate` 保留吗？** | **删除** | ✅ **已裁 2026-11-09（D11）**。我此前的「保留」判断与「0 实现者」统计都是错的（后者只扫 C++，漏了 C# 夹具） |
| 6 | **`AttributeScaled` 读取侧形态** | 保留 | ✅ **已裁 2026-11-10（D12/D13/D14）**：源保留、住 TcsAttribute；`ITcsAttributeProvider` 删除；给上下文加 `TObjectPtr<UWorld> World`，不建新上下文类型 |
| 7 | **`Composite` 的运算集** | 够 | ✅ **已裁 2026-11-11（D36/D37）**：四则；不加 `Sub`、现在不加 `Div`；空 `Terms` 一律配置错误；名字 `Composite`、成员 `CombineOp`。详见 §3 |
| 8 | **冻结机制怎么重做？** | ⚠️ **问法已换**：不是「`FTcsParamSnapshot` 搬到哪」，是「一个值绑在哪个时刻」 | ✅ **方向已裁 2026-11-13（D47–D50）**：基底出生求值 + 写入门 + 账本折叠；**门 = 读取侧（译法 B）**、**配置定且不可变（甲）**、**MUST 管布尔**、**只管 `Params` 行**。**⚠️ 细节未确认：U1–U7 见 §8.8.7。原 D42–D46 作废** |
| 9 | **值约定（§8.7）并入参数域**，`TcsNotation` 模块取消 | 确认（机制 14 已有此裁决，只是还没落） | 未确认 |
| 10 | **P2 命名**：`FTcsParamValueSource` → `FTcsParamSourceBase`？ | 改 | 未确认 |
| 11 | **P3 命名**：`FTcsParamValue` 是否改 `FTcsParamExpr`？ | 不改 | 未确认 |
| 12 | **架构三选一**（§9） | 倾向 B | **等 1–11 走完** |

**清单之外的登记项**：

| # | 事项 | 落点 |
|---|---|---|
| A | §7.1 三问的第 3 问：属性侧 `ETcsAttrLayer` 与参数侧 `CompeteGroup` 是不是同一个概念 | **不并入 §10**——它属「作用域概念是否统一」，是第 12 条的前置观察，不是独立裁决项 |
| ~~B~~ | ~~§7A.4 聚合器内部矛盾~~ | ✅ **2026-11-11 已落地**：`Source` → `uint64 OrderSeq`，流程域填 `0` |
| ~~C~~ | ~~§7A.5 惰性与写时重算的时机冲突~~ | ✅ **2026-11-11 已落地**：统一为写时重算 |
| D | 伤害记录形状 / **事件 Tag 命名公约** | **不并入本表**——两者都不属参数域。正文在族外 [`未决-核心基线.md`](未决-核心基线.md) §8–§9 |
| E | `ITcsSkillCostPolicyHost` 零实现（§8.4） | **未裁**：V1 的「宿主可自定义消耗策略」今天完全没被使用过。登记待议 |

---

## 11. 裁决流水（逐条可回溯）

> **用途**：把对话里**用户明确裁过**的每一条按时间顺序记下来，供写正式文档时逐条摘用。**每条都有用户原话或明确确认**，不含我的推断。本节只记「裁了什么」，不记「怎么论证的」。

| # | 日期 | 裁决 | 用户原话 / 确认方式 |
|---|---|---|---|
| D1 | 11-07 | **`ETcsParamMode` 与 `FTcsParamValue` 正交；模式归持有者**（业务依据已由 D38 订正） | 「在 SkillParam-SkillModifier 这条线上，确实没有使用 Snapshot 的需求，所以我也同意你的【模式与载体正交，模式归持有者】的设计」 |
| D2 | 11-08 | **V1 折叠器删除、不搬**（V2 数值聚合器即其完整版） | 「按照你给的倾向建议执行」 |
| D3 | 11-08 | **覆盖比较维持单级**（7 策略互斥，无隐藏三级） | 同上 |
| D4 | 11-08 | **聚合器条目不持 `FTcsSourceHandle`**，只持施加序；**域侧保留句柄** | 「聚合器本身只执行计算，确实不需要保存 SourceHandle，但是调用聚合器的几个域，却需要保留 SourceHandle」 |
| D5 | 11-09 | **明细赢家降为序号、域自己回查账本** | 「你给的方案我不反对」 |
| D6 | 11-09 | **文档层的「域」与实现层的「模块」是两个轴** | 「确实是两个轴」 |
| D7 | 11-09 | **参数表（`FTcsNumericParamRow`）归参数域**（Def 只是持有者） | 「参数表虽然是被 StateDef 引用，但完全可以在 Parameter 域内确定完所有逻辑后，再被 StateDef 引用」 |
| D8 | 11-09 | **「按施法者等级放大效果」能力需要保留** | 「LAC / 另外两款都需要」 |
| D9 | 11-09 | **`ITcsEntityLevelProvider` 是 (a) 类宿主契约，MUST NOT 因零实现而砍；不补实现** | 「TCS 本身是战斗系统插件，开发的很多功能就应该只有宿主项目侧才会使用到，不然 AddAttributeSet、LearnSkill 这种，岂不是也会在 TCS 内部完全没有消费者」 |
| D10 | 11-09 | **四个等级源住 State 域**（含 `ITcsEntityLevelProvider`） | 「可以把 StateLevelArray/Map 放在引用 Parameter 域的 State 域实现」「`ITcsEntityLevelProvider` + 两个 InstigatorLevel 源我觉得也应该放在 State 域」 |
| D11 | 11-09 | **`ITcsParamSourceHost` + `FTcsParamSource_HostDelegate` 删除** | 「我觉得不应该保留……这些功能就应该宿主项目自己实现」+「【低血量时移速 +30%】就应该是一个被动 Buff，EffectChain 里 WaitEvent，给 Owner 上一个移速加成 Buff」 |
| D12 | 11-10 | **`FTcsParamSource_AttributeScaled` 保留、住 TcsAttribute** | 「AttributeScaled 保留、住 TcsAttribute，没问题」 |
| D13 | 11-10 | **`ITcsAttributeProvider` 删除** | 「我其实也一直觉得这个 Provider 的意义不大」 |
| D14 | 11-10 | **给 `FTcsParamEvaluateContext` 加 `TObjectPtr<UWorld> World` 字段**，不建新上下文类型、不建新接口 | 「确认」 |
| D15 | 11-10 | **`Live + Instigator` 走事件订阅**，属性域只提供变更事件（不入依赖图） | 「确实没必要做成依赖图的形式，感觉可以做成【订阅 Instigator 属性变更事件，触发时 Buff 内部修改自己的 AttributeModifier】」 |
| D16 | 11-10 | **依赖图重建要覆盖 `AttachMod` / `DetachMod`**，走 B2（批归零点全图重建） | 「我认可这个结构和 B2的思路」 |
| D17 | 11-10 | **原建图时机与派生属性无关**；真因是 `Dependents` 必须与 `Instances` 下标对齐 | 「你的结论没问题」 |
| D18 | 11-10 | **拆出 `FTcsAttributeGraph`**（只含 `Dependents` + `EvalOrder` + 脏位） | 「拆出 FTcsAttributeGraph 我是认可的」 |
| D19 | 11-10 | **建图签名改 `Rebuild(int32 SlotCount, TConstArrayView<FTcsAttrEdge> Edges)`** + 提边时报缺失目标 | 「我也确认」 |
| D20 | 11-10 | **`FTcsAttributeInstance` 不改名**；代码落地时注释写明「槽位 = 下标」 | 「FTcsAttributeInstance 不改名没问题，不过具体代码落地时要在注释里写明，槽位对应的是 Instance 在数组中的索引下标」 |
| D21 | 11-10 | **`Rebuild` 由 `CommitBatch` 调用**，其余写入口只置 `bDirty` | 「三条我都认可」 |
| D22 | 11-10 | **环检测搬去 `UTcsDefValidationSubsystem`（editor-only）**，运行期不做 | 「环检测住 UTcsDefValidationSubsystem（editor-only），确认，我把 DefRegistry 记错了」 |
| D23 | 11-10 | **不引入 `CustomOperator` / `OperandEvaluatorClass`**（master 那套） | 「确认」 |
| D24 | 11-10 | **`ETcsAttrReadScope { Current, BaseOnly }` 引入**（后经 D32 取代） | 「倒是也没什么问题」 |
| D25 | 11-10 | **弗拉米尔这类「成对修正器」用批，配置者保证成对**；不建 `AttrModGroupDef` | 「我觉得 AttrModGroupDef 没什么必要」 |
| D26 | 11-10 | **不借鉴「层序替代依赖图」** | 「我也同意」 |
| D27 | 11-10 | **借鉴「来源排除」机制** | 「借鉴「来源排除」，我同意」 |
| D28 | 11-10 | **自建 `FTcsTagRequirements`**（不依赖 GAS 模块）；`TagQuery` 不建 | 「TCS 不准备依赖 GAS 模组，所以要在 TCS 内部建立一个同样形状的结构体」+「TagQuery 位不建 —— 确认」 |
| D29 | 11-10 | 过滤字段名 **`ModifierTagRequirements`**；语义照搬 GAS | 「名字就用 ModifierTagRequirements」+「我觉得可以照搬 GAS 的语义」 |
| D30 | 11-10 | **现在建 `02-核心基线/GameplayTag.md`**（第六项基线） | 「现在就创建文档，文档名就叫 GameplayTag，可以引用 V1 的裁决」 |
| D31 | 11-10 | **`InstigatorTags` / `TargetTags` 列入未来计划**（**已被 D33 取代**） | 「我倒是觉得 InstigatorTags（GAS原SourceTag）、TargetTags 也可以引入，只不过可以放到未来计划中」 |
| D32 | 11-10 | **`ETcsAttrReadScope` 整体删除**，由 `ETcsAttrReadPart`（`Total`/`Basic`/`Extra`）接替；`ETcsAttrReadSide` 保留 | 讨论中确认「段值级 scope」正是 `Part` 在做的事 |
| D33 | 11-11 | **`InstigatorTags` / `TargetTags` 当前不引入**；条件生效走 D11 的链路径。重启需同时满足三条进入条件（§15） | 「我接受你的提议」 |
| D34 | 11-11 | **`FTcsSourceHandle` 保持纯数字，不承载因果**（撤销锚点有 5 处 MUST 级读点；V1 的 `CausalityChain` 塞进句柄已被证伪） | 「SourceHandle 如果是个纯数字，真的可以吗」——实测后确认保持 |
| D34b | 11-11 | **因果链 / 溯源边：裁为「不建」**。判据：V1 落地了字段 + 两个读数口，至今**零逻辑读取方**（全仓 `CausedBy` 命中 4 处 = 1 定义 + 2 写入 + 1 读数口；`GetRunCausedBy` 零调用） | 「暂时全部裁掉，就算后面要加，也不是不能重新裁决」 |
| D34c | 11-11 | **Explain 调试面板：裁为「不建」**。判据：V1 规格有定义、**无任何 Task 认领**，11 个模块无编辑器模块 | 同上 |
| D35 | 11-11 | **「游戏结算统计」= 插件提供实现框架方案**（事件 + 记录形状 + 读取路径），宿主利用它实现自己的结算；**统计口径本身裁为「不建」** | 「插件提供实现框架方案，宿主项目利用框架实现自己的结算统计」 |
| D36 | 11-11 | **`Composite` 维持四则 `Add`/`Mul`/`Max`/`Min`**；不加 `Sub`（可表达且破坏结合律）、现在不加 `Div`（零消费者）；空 `Terms` = 配置错误 | 「确认这三条」 |
| D37 | 11-11 | **命名：源叫 `Composite` 不叫 `Formula`；成员 `Op` 改 `CombineOp`**。判据：`Formula` 在 V1 已被 `FormulaParams` 占用且该词超出本机制能力；`Op` 在全仓已有三处不同所指 | 「你觉得这个 ParamSource 到底叫 Composite 还是 Formula」+「确认这三条」 |
| D38 | 11-12 | **D1 的结论保留、业务依据作废并订正**。D1 原记理由已被 D39 推翻；D1 结论**不但保留且更强**——若 Skill 也要冻、State 也要冻，模式更不能绑载体，否则 `Composite` 的三个 `Term` 各带一个 `Mode`，求值时听谁的 | 订正系调研结论，**待用户确认后生效** |
| D39 | 11-12 | **Skill / Buff 侧确有 Snapshot 需求**——推翻 §8.6 记的 D1 业务依据 | 「不对劲，或许 Skill侧 Buff侧 真的也需要 Snapshot 机制。比如一个 SkillEntry 已经因为玩家操作激活了 一次 SkillInstance，其内部有一个 BoolSwitch 或者 影响前摇时间 的 Numeric，开着可能允许 一个 SkillModifier 在这个 SkillInstance 过程中影响，也可能不希望。」 |
| D40 | 11-12 | **问法纠正**：「`FTcsParamSnapshot` 该归谁 / 搬到哪」是错问法（预设了「要搬」）；正确问法是「**一个值绑在哪个时刻**」 | 「参数快照该归谁」→ 用户否定该问法 |
| D41 | 11-12 | **冻结是同一个机制落在三种宿主上**（State 实例 / Skill 实例 / 有自己参数行的 Area 实例）；**Area 的「每跳」不是绑定时刻问题，是读取频率问题** | 本次调研结论，**待用户确认** |
| ~~D42~~ | 11-13 | ~~**T1 裁：`Live` 行的源用【实例存源副本】**（路线乙）~~ ⇒ **已作废**，见 D47 说明 | ~~用户在 T1 三选项中选定「实例存源副本」~~ |
| ~~D43~~ | 11-13 | ~~**T2 裁：`Live` 行 `ParamRef` 成环的闸门用【配置期环检测】**~~ ⇒ **该结论对 V2 仍可能成立，但所依附的「`Live` 行需要源副本」前提已作废；若要保留，须作为独立项重裁** | ~~用户在 T2 两选项中选定「配置期环检测」~~ |
| ~~D44~~ | 11-13 | ~~**T3 裁：冻结存储【`USTRUCT` 化】**~~ ⇒ **已作废**。与 V1 类型形状绑定；新骨架的值表形状尚未定（见 U4/U7），届时若仍住池内 `TArray` 且需 GC 可见，本问题会以另一种形式重现 | ~~用户在 T3 上选「当然要 USTRUCT 化」~~ |
| ~~D45~~ | 11-13 | ~~**T4 裁：类型名 `FTcsParamBinding` / `FTcsParamBindingEntry`，字段 `ParamBinding`，住 `TcsState/Public/State/`**~~ ⇒ **已作废**。该命名回答的是「值绑在哪个时刻」这一**已被 D40 保留的问题**，但类型形状（「一张可查的表 + 源副本」）是 V1 的翻版（§8.8.1 三处锚点之②③） | ~~用户在 T4-a/b 选项中选定~~ |
| ~~D46~~ | 11-13 | ~~**T4-c 裁：该类型兼作参数表——读口按行分派，壳改绑「实例」**~~ ⇒ **已作废**。它存在的唯一理由就是「`Live` 行不在表里」；新骨架里**值表恒装全部键**，该缺陷的成因不存在（§8.8.3 的分界） | ~~用户在 T4-c 选项中选定~~ |
| **D47** | 11-13 | **冻结机制按「基底出生求值 + 写入门 + 账本折叠」重做**：参数行去掉 `Mode`，只剩 `Key` / `Base` / `ValueConvention`；实例出生时把每个键求值一次存进出生值表（`BirthParams`）；门只决定「出生之后是否继续跟随账本」，**不决定「是否进表」**；条目账本（`FTcsLearnedSkillEntry::NumericParamModInstances`）只保留一份 | 「基本上，是按照『基底出生求值 + 写入门 + 账本折叠』重做，只不过思路本身仍然有可以讨论和确认的内容」⇒ **方向已批，细节逐条确认** |
| **D48** | 11-13 | **门 = 译法 B（管读取，不管写入）**：读口里若「跟随」则走条目实时通道，否则只认出生值。实例**不需要自己的账本** | 「我个人也倾向于 译法 B」 |
| **D49** | 11-13 | **门 = 甲（配置定、出生拷贝进实例、此后不可变）**，且 **MUST 管布尔开关**（不只管数值） | 「我也倾向用甲」+「门管不管 布尔参数？管的，必须要管的」 |
| **D50** | 11-13 | **门只管 `Params` 行**；无 `Key` 的值（时段时长 / 冷却时长 / 消耗 / 链步骤内联值）与三个插件契约键（`Level` / `CooldownPct` / `CostAmount`）的冻结点**继续由代码定**，不加门 | ask_user_question 答复选「甲：门只管 Params 行」 |

> **D42–D46 作废的依据**：用户 2026-11-13 两次纠正「不要过度参考 V1，不然为什么要重新设计V2」（m01497）与「V2 现在是要 重新设计」（m01701）。复查发现那五条裁的是「V1 那张表该怎么修」（三处 V1 锚点见 §8.8.1），不是「V2 需要什么」⇒ 记录在案、不再作为设计依据。

---

## 12. 弗拉米尔案例（D24–D29 的落地形态）——**已移出本域，此处只留索引**

> **本版动作：168 行 → 本节 27 行（实测）。** 原文主体讲的是**属性域**的依赖图与来源排除机制，不是参数域的内容；族内 `04-领域/Attribute.md`（依赖图、`ETcsAttrReadPart`、过滤读）与 `02-核心基线/GameplayTag.md`（`FTcsTagRequirements`）已有落点，此处若留全文就是同一件事的第三份副本。

### 12.1 结论摘要（完整机制见 `Attribute.md` §3.5 / §5.2 与 `GameplayTag.md` §1）

| 项 | 结论 | 出处 |
|---|---|---|
| 问题 | LoL 弗拉米尔【血色契约】：①每 40 额外生命→+1 法强 ②每 1 法强→+1.4 额外生命 ③不自循环。①②互相引用 = **依赖环**，按「环即失败」根本配不出来 | 本节原 12.1 |
| **否决的做法** | 「层序替代依赖图」（用户伪代码的 `layers = [BASE, FLAT, PERCENT, ...]` 与 GAS 的 `EGameplayModEvaluationChannel` 是同一思路）⇒ **不借鉴**（D26）。理由：层序 = 人工提前算好的拓扑序，靠人保证层序与语义一致，**配错即静默错值**；TCS 的 `EvalOrder` 每次从实际依赖边算，能自动检环 | 原 12.2 |
| **采纳的做法** | 来源排除：修正器带 `Tags`，读取源带 `ModifierTagRequirements{RequireTags, IgnoreTags}`，读取方按标签筛掉不该计入的修正器（D27–D29） | 原 12.3 |
| **消环的真机制** | **不是标签，是图的节点粒度**：节点不是属性，而是求值单元（`SegmentBase`/`Modifier`/`Segment`/`Total`）⇒ 边可以精确到「某一条修正器」。过滤读**跳过段节点**，直连「段基值 + 每条被计入的修正器」⇒ 被排除的修正器**不在依赖链上**。标签只是「挑哪些修正器」的手段 | 原 12.4 |
| 弗拉米尔的两个配置项 | `Part = Extra` **且** `IgnoreTags = {Pact}`，**缺一不可**：读总值（`Part = Total`）就只能连 `Total(HP)` → `Segment(HP.Extra)` → 反向修正器 ⇒ 环；过滤为空则连 `Segment` 同样闭环 | 原 12.4 |
| 成本 | 不配过滤 = 零成本（多出的边只在 `RebuildGraph` 时付一次，`Attribute.md` §5.2）；过滤读**读 `NodeValues` 的节点值、不读 `CachedCurrent`、不存第二条缓存** | 原 12.5 |

### 12.2 两条语义边界（与 `GameplayTag.md` §1.2 一致，实现时须照此判定）

| 情形 | 结果 |
|---|---|
| 无标签修正器 + `IgnoreTags` 非空 | **计入**（`HasAny(空集)` = false）← 弗拉米尔成立的前提 |
| 无标签修正器 + `RequireTags` 非空 | **被排除**（`HasAll(空集)` = false）← 危险配置，建议数据校验期加警告 |
| 两个集合都空 | 恒通过（走快路径） |

> **与参数域的关系**：`FTcsParamSource_AttributeScaled` 是参数源的七族之一（§1），它的 `ModifierTagRequirements` 字段属于属性域的读取侧语义，参数域只负责「它是可被引用的一个源」。

---

## 13. 遗留点：逐条结案

> 本节原为「尚未裁决的遗留点」。2026-11-10 逐条复核后，**六条全部有结论**（结论不是「裁了」就是「问题本身不成立」），另发现一条已修。

| # | 原问题 | 结论 | 依据 |
|---|---|---|---|
| R1 | 带过滤的读取是「第二条读取路径」，与 `CachedCurrent` 的关系要不要收窄？ | ✅ **已消解，无需改 `Attribute.md` §2.1** | 过滤读读的是 `NodeValues` 里的节点值，不是另一份缓存。`CachedCurrent` 只是 `Total` 节点值的视图 |
| R2 | `ETcsAttrReadScope` 最终几档？ | ✅ **已删除，问题消失** | D32：整个 `ETcsAttrReadScope` 取消，由 `ETcsAttrReadPart`（`Total`/`Basic`/`Extra`）接替 |
| R3 | `BaseOnly` 是否需要段参数？ | ✅ **同上，已删除** | 「读 `Extra` 段的基值」= `Part = Extra` + 读 `SegmentBase` 节点 |
| R4 | `ShouldBuildEdge` 在「过滤非空但暂无修正器」时的行为 | ✅ **问题不成立 + 挖出真缺陷并已修** | ① `ShouldBuildEdge` 已随槽位级建边删除，改为修正器级 `BuildModifierOutEdges`；② 真缺陷是 `BatchWrittenNodes` 存节点下标、`RebuildGraph` 后失效 ⇒ 已裁「重建即丢弃、全量重算」 |
| R5 | `Graph::Rebuild` 检出环后返回 `false`，`CommitBatch` 要不要中止？ | ✅ **不中止；原子性口径已写进 `Attribute.md`** | §5.4 + §7.1：`ApplyAttributeSet` 的 `true` 只承诺「改动已应用、值已结算」，**不含「图无环」** |
| R6 | `FTcsTagRequirements` 的「≥2 域依赖」不满足 | ✅ **已满足，非勉强论证** | 消费者四个：Attribute 的 `ModifierTagRequirements` + State/Buff/Skill 三域类目筛选 |
| R7 | `Attribute.md` §3.4 仍是按实例整体求值、§5.3 已是按节点求值，两套实现 | ✅ **已修** | §3.4 重写为「只讲怎么交给聚合器，不讲什么时候算」，并新增「缓存与节点值的关系」表 |

---

## 15. 修正器条件生效（`InstigatorTags` / `TargetTags`）：当前不引入

> **对应 D33（2026-11-11，「我接受你的提议」）**。原 D31 的「列入未来计划」已被 D33 取代。**本版动作：124 行 → 本节 70 行（实测）**，删掉 GAS 源码考古的中间论证，只留判据与重启条件。

### 15.1 问题与 GAS 的形态

> 是否应该引入 GAS 的 Modifier 配置中的 `SourceTags` 与 `TargetTags`？

GAS 把它们放在 `FGameplayModifierInfo` 上（`GameplayEffect.h:584-594`），语义是「施加者 / 目标身上的标签满足这个要求，这条修正器才计入」。

### 15.2 GAS 实证：这套机制本身不是好样板【V1 参照，仅用于说明风险】

| 路径 | 判定时机 | 查哪个标签集 | 证据 |
|---|---|---|---|
| **Duration**（有聚合器） | 每次 `Evaluate` 全量重扫 | `GetAggregatedTags()` = ActorTags **+ SpecTags** | `GameplayEffectAggregator.cpp:391-395` |
| **Instant / Periodic** | 施加时逐条 `continue` | `GetActorTags()` = **只有** ActorTags | `GameplayEffect.cpp:3113-3125` |

两条路径查的**不是同一组标签**，而且 Instant 那条是**后补的补丁**——源码里还留着 `AbilitySystem.Fix.UseModTagReqsOnAllGE` 这个 CVar，说明文字是「**Fix an issue where MustHave/MustNotHave tags did not apply to Instant and Periodic Gameplay Effects**」⇒ 它长期不生效、后来才补上、补丁还停在 CVar 后面。**这是历史债，不是设计。** 另外标签在施加瞬间被捕获（`CapturedSourceTags`），之后不跟随变化。

> **结论**：照搬会把缺陷一起带进来。（GAS 源码因此不属 TCS 的 V1，只作外部参考。）

### 15.3 TCS 现在缺前置条件

`SourceTags` / `TargetTags` 要查的是「**实体身上现在有哪些标签**」。而 **TCS V2 没有「实体标签容器」这个概念**——`02-核心基线/战斗实体.md` 全文 `Tag` 出现 **0 次**。

V2 现有三个标签机制，**方向各不相同**：

| 机制 | 方向 | 位置 |
|---|---|---|
| `FTcsAttrModInstance::Tags` | 修正器**自报**「我是什么」 | `Attribute.md` §7.2 |
| `ModifierTagRequirements` | 读取方**筛选**「计入哪些修正器」 | `Attribute.md` §5.2 |
| **GAS `SourceTags`/`TargetTags`** | 修正器**要求**「别人有什么」 | ← **方向相反** |

引入后 `FTcsAttrModInstance` 上会同时出现三个标签字段（我携带的 / 施加者必须有的 / 目标必须有的），配置者要分清三者方向，**这本身就是设计债**。

### 15.4 三条不引入的理由

| # | 理由 | 具体 |
|---|---|---|
| ① | **已有正交路径（D11）** | 「目标有【虚弱】⇒ 攻击力 +20%」链已能表达：`WaitEvent(目标获得【虚弱】) → Branch(判定是不是我该反应的) → ApplyState(攻击力 +20% 的 Buff)`。把条件塞进修正器字段等于**在两条路上重复实现同一能力**，且链那条更通用（能表达「进入触发一次」「离开撤销」「有前置动作」），修正器字段只能表达「当前时刻成不成立」 |
| ② | **前置机制不在本域，属倒置归属** | 「实体标签容器」属 **State 域**（「实体身上在跑哪些状态」的词集），State 域还没开始裁。现在设计「修正器怎么查它」，就是让 Parameter 域规定 State 域必须长出什么形状——**与 D7 是同一个错误的镜像**：那次的教训是「被引用者不该等引用者」，反过来说就是「**本域不该替另一个域设计它的机制**」 |
| ③ | **违反铁律 ②：预建插槽** | 今天找不到「同一条 Buff 内多条修正器按条件择一」的真实配置。**没有消费者就先建字段**，正是铁律 ② 禁止的 |

### 15.5 三条进入条件（全部满足才重启）

| # | 条件 | 为什么是它 |
|---|---|---|
| ① | **State 域确实产出了「实体标签容器」这一机制** | 没有它本机制无处查标签——前置依赖必须先落地 |
| ② | **出现「同一条 Buff 内多条修正器按条件择一」的真实配置需求** | 这是链路径**唯一表达不了**的场景（链作用于修正器的**存在与否**，表达不了「同一条修正器在不同条件下取不同值或不计入」） |
| ③ | **证明「拆成多条 Buff」在本项目实际配置量下不可接受** | 若拆 Buff 能解决且配置量可控，就不该为省事引入第二套条件机制 |

> **②与③是同一条判据的两半**：②说「链做不到」，③说「也没有更土的替代做法」。两条都不满足时引入，就是为一个能绕过去的问题增加长期维护面。

### 15.6 若将来引入，命名 MUST 避开歧义

`InstigatorTags` 这个名字没问题（GAS 的 `Source` 在 TCS 会与 `FTcsSourceHandle` 撞词）。但 **`TargetTags` 在 TCS 里是有歧义的**（§2 已记录）：`Target` 在伤害流程里指「被命中的敌人」，在 Buff 里指「被加成的友军」——两者可以不是同一个实体（治疗技能：施法者 A、受害 B、治疗目标 C）。GAS 没这个问题，因为它的 GE 只有一个 target。

| 候选名 | 指向 |
|---|---|
| `OwnerTags` | **修正器宿主**的标签（Buff 挂在谁身上就查谁） |
| `DamageTargetTags` | **伤害目标**的标签 |

> **MUST NOT 叫 `TargetTags`**：它同时可以指上面两个，配置者无法从名字判断。

### 15.7 连带：`FTcsTagRequirements` 的消费者数量

> **本裁决不影响 `GameplayTag.md` 的归属**。`FTcsTagRequirements` 的消费者已有四个（Attribute 的 `ModifierTagRequirements` + State/Buff/Skill 三域类目筛选，`GameplayTag.md` §2），本裁决只是不给它添第五个。

---

## 调研方法与边界

- 扫描范围：`Source/` 全量 11 模块的头文件与 `.cpp`，`Documents/combat-system-design/` 的决策与规格档。
- `Content/` 下 **0 个 `.uasset`** ⇒ 无存量资产需迁移，「改名成本为零」有实证支撑。
- **⚠️ 「零实现者」判法已证伪一次**：原判法「grep `public I<接口名>`，命中 0 即认定」**只能扫 C++**。`ITcsParamSourceHost` 的 C# 夹具 `UTcsDevGcParamSourceHost` 就扫不到（§8.3）。此后凡此结论 MUST 说明**扫了什么**。
- **⚠️ 本机工具局限**：`read` 不带 `limit` 时**最多返回约 667 行且静默截断** ⇒ 改写长文件 MUST **分块读取（≤300 行/次）再拼接**。
- **本版（2026-11-13 精简）新增口径**：全文按【V1 实证】/【V2 推导】/【已裁 Dxx】三类标记。**【V1 实证】只能用于判断「V2 要解决什么问题」，MUST NOT 用来论证「V2 该长什么样」**——这是用户 2026-11-13 两次纠正（「不要过度参考 V1」「V2 现在是要重新设计」）的直接落实。
- 本轮未编译、未运行，全部结论来自静态阅读。
