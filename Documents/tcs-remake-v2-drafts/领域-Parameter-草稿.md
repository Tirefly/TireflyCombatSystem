# 领域草稿 · Parameter（参数域）

> **族外草稿，不是实现依据。** 参数域正式文档尚未开始；本文只收录**调研实证 + 待确认点**。未列入本文的不算已裁，已裁的也不等于已进参数域文档。
>
> **本轮工作方式（2026-11-06 用户裁定）**：原话「我觉得是不是应该先全面调研完成后，再说，可以先起一个草稿文档，逐步记录，逐步确认，最后再确定这三个选项」。
> ⇒ 推进顺序固定为 **调研 → 记录 → 逐条确认 → 最后裁架构**。§9 的三选一在 §10 确认清单走完之前**不作结论**，也不写进族内。

---

## 目录

| § | 主题 | 性质 |
|---|---|---|
| [0](#0-调研总表v1-里参数这个词散在六个模块) | 调研总表：V1 的参数散在六个模块 | 实证 |
| [1](#1-值来源族v1-现状v2-待复核) | 值来源族（V1 现状） | 实证 |
| [2](#2-attributesscaled-的读取侧待裁) | `AttributeScaled` 的读取侧 | 已裁（D12/D13/D14） |
| [3](#3-多属性组合composite-与-paramref第-7-条待议) | 多属性组合 + **第 7 条待议** | ⚠️ 待议 |
| [4](#4-与-attribute-域的接口) | 与 Attribute 域的接口 | 已裁 |
| [5](#5-值载体与源族的命名开篇第-0-号裁决) | 值载体与源族命名（P2/P3） | ⚠️ 待议 |
| [6](#6-live--snapshot-的归属--已裁决--归参数域方案-a) | `Live` / `Snapshot` 归属 | ✅ 已裁（D1） |
| [7](#7-参数域也有自己的修改器--会回头影响-attribute-域2026-11-06-用户提出) | 参数域自己的修改器 | 登记 |
| [7A](#7a-折叠器归属v1-那一份应当删除不是搬走2026-11-08-已裁) | 折叠器归属 | ✅ 已裁（D2–D5） |
| [8](#8-调研实证六条能力已建契约悬空) | 六条「能力已建、契约悬空」 | 实证 |
| [9](#9-三选一参数各部分该住哪待-10-走完后才裁) | 架构三选一 | ⚠️ **最后裁** |
| [10](#10-逐条确认清单) | **12 条确认清单（进度表）** | 索引 |
| [11](#11-2026-11-07--11-10-的裁决流水逐条可回溯) | 裁决流水 D1–D33 | 台账 |
| [12](#12-弗拉米尔案例来源排除机制d24d29-的落地形态) | 弗拉米尔案例 | 已裁（D25–D29） |
| [13](#13-遗留点逐条结案) | R1–R7 结案 | 已结 |
| [15](#15-修正器条件生效instigatortags--targettags当前不引入) | `InstigatorTags`/`TargetTags` | ✅ 已裁（D33） |
| [方法](#调研方法与边界) | 调研方法与边界 | 元 |

> **已裁的裁决以 §11 为准**；原 §14 已并入 §3（编号留空，不重排）。

---

## 0. 调研总表：V1 里「参数」这个词散在六个模块

> **核心事实**：**V1 不存在参数域模块**。全仓 `TcsParamSubsystem` = **0 命中**，没有任何参数域门面。

| # | 部件 | 住哪 | 消费者实证 | 含领域词汇？ |
|---|---|---|---|---|
| 1 | `FTcsParamValue`（载体） | TcsCore | **48 处，跨 7 模块** | 否 |
| 2 | `FTcsParamValueSource`（抽象基类） | TcsCore | 6 个内置派生 + 1 个域派生 | 否 |
| 3 | `FTcsParamEvaluateContext` | TcsCore | 48 处 | 否 |
| 4 | `FTcsParamEnumerableSource` | TcsCore | 4 个派生 | 否 |
| 5 | `FTcsParamSource_Literal` | TcsCore | 载体默认源；8 处显式引用 | 否 |
| 6 | `FTcsParamSource_ParamRef` | TcsCore | **实际只在状态侧物化路径生效**（§8.5） | 否 |
| 7 | `FTcsParamSource_HostDelegate` + `ITcsParamSourceHost` | TcsCore | 转发器 2 处；**宿主实现者 0** | 否 |
| 8 | `FTcsParamSource_{State,Instigator}Level{Array,Map}` | **TcsBuff** | 4 源；等级读口**实现者 0**（§8.2） | 否 |
| 9 | `FTcsParamSource_AttributeScaled` | **TcsAttribute** | 属性读口**实现者 0**（§8.1） | 否 |
| 10 | `ITcsParamTableReader` | TcsCore | **1 个实现**（`UTcsStateParamTableReader`） | 否 |
| 11 | `ETcsParamMode` | **TcsState** | **1 个真消费者**（`TcsCastOps_Snapshot.cpp:42`） | **是**（「施法那一瞬冻住」） |
| 12 | `FTcsParamSnapshot` | **TcsState** | 技能 / 状态 / Buff 三处 | **是**（施加瞬间冻结） |
| 13 | `FTcsNumericParamModifier` + 参数链 | **TcsSkill** | 技能域内 | **是**（「改技能参数表的键」） |
| 14 | `FoldTcsAttributeBands`（折叠器） | **TcsAttribute** | **3 处**（属性聚合 / 技能参数链 / 伤害流程属性） | 否（五带无领域语义） |
| 15 | `ETcsValueConventionFlag` + `ConvertToCanonical` | **TcsNotation** | 12 处，跨 5 模块 | 否 |
| 16 | 契约键 `Tag_TcsStateParam_Level` | TcsState | 技能等级修正 | 是 |
| 17 | 契约键 `Tag_TcsSkillParam_{CooldownPct, CostAmount}` | TcsSkill | 冷却 / 消耗减免 | 是 |
| 18 | `FTcsParamView` 视图族（Value/Series/Range/Attribute） | **未建**（R8） | 零 | — |

**读法**：第 1–10 行是**载体系**（无领域词汇、被 ≥2 域依赖）；第 11–13、16–17 是**真领域语义**；第 14 行是一个**住错了模块的纯函数**。

> **模块依赖实证**（各 `Build.cs` 的 `PublicDependencyModuleNames`）：
> `TcsCore` ← 无 Tcs 依赖｜`TcsAttribute` ← Core,Notation｜`TcsState` ← Core,Notation,**Attribute**｜`TcsBuff` ← Attribute,Effect,State｜`TcsSkill` ← Attribute,Effect,State｜`TcsDamage` ← Core,Attribute,Effect
> ⇒ **`TcsSkill` 与 `TcsDamage` 都要反向依赖 `TcsAttribute`**，只为了拿五带枚举与折叠器。

---

## 1. 值来源族（V1 现状，V2 待复核）

V1 的 `FTcsParamValue { FInstancedStruct Source }` 已有一组源（机制 12 复盘）：

| 源 | 语义 | 住哪 | V2 待定 |
|---|---|---|---|
| `FTcsParamSource_Literal` | 配置写死的数 | TcsCore | 保留 |
| `FTcsParamSource_StateLevelArray` | 按状态等级查表 | TcsBuff | 保留 |
| `FTcsParamSource_StateLevelMap` | 按状态等级查离散档 | TcsBuff | 保留 |
| `FTcsParamSource_InstigatorLevelArray` | 按施加者等级查表 | TcsBuff | **实现者 0，见 §8.2** |
| `FTcsParamSource_InstigatorLevelMap` | 按施加者等级查离散档 | TcsBuff | **实现者 0，见 §8.2** |
| `FTcsParamSource_AttributeScaled` | 系数 × 另一属性当前值 | TcsAttribute | **需重做**（见 §2、§8.1） |
| `FTcsParamSource_HostDelegate` | 宿主自定义求值语义 | TcsCore | **实现者 0** |

> **V1 的实测状态**：`AttributeScaled` 依赖宿主提供 `ITcsAttributeProvider`，而**全仓零实现者**（机制 12 复盘原话：「接口声明在册、实现者 0」）。⇒ V2 不能原样继承。

---

## 2. `AttributeScaled` 的读取侧（待裁）

**背景**：Attribute 域的修正器值可以来自「读某实体某属性 × 系数」。读**哪个**实体必须能区分。

**用户口径（2026-11-05）**：用 `Instigator` / `Target` 两态。

```cpp
enum class ETcsAttrReadSide : uint8
{
	Instigator,   // 施加者（施法者的属性）
	Target,       // 被加成的实体（修正器宿主的属性）
};
```

> **不在形态里区分 `Self`**：`Instigator` / `Target` 已覆盖，「自身」即 `Target`。

**命名坑（必须写进参数域文档）**：`Target` 在伤害流程里指「被命中的敌人」，在 Buff 里指「被加成的友军」——两者可以不是同一个实体（治疗技能：施法者 A、受害 B、治疗目标 C）。这个歧义只允许存在于参数源内部，MUST NOT 外溢成跨域通用词。

**V2 已裁的连带**：`Attribute.md` 把「依赖只有两个来源」（§5.1）定为 `Bounds.Dynamic` 与 `AttributeScaled`，而 **`ITcsAttributeProvider` 已被裁为删除**（机制 15、V2 属性域）⇒ `AttributeScaled` 在 V2 必须**直接走属性子系统**，不再经宿主接口。这决定了本节的形态要在参数域开工时重定。

---

## 3. 多属性组合：`Composite` 与 `ParamRef`（**第 7 条待议**）

**背景**：GAS 用 `UGameplayEffectExecutionCalculation` 支持「一个值由多个属性组合」；TCS 的聚合器**无代码插点**（《数值聚合器》§2.2「计算在上游求值并传入终值」），所以组合能力必须落在参数域。

**用户口径（2026-11-05）**：`Composite` 与 `ParamRef` **都要**。

```cpp
// 引用另一个参数
struct FTcsParamSource_ParamRef : FTcsParamValueSource
{
	FGameplayTag ParamTag;
};

// 组合多个值
struct FTcsParamSource_Composite : FTcsParamValueSource
{
	TArray<FTcsParamValue> Terms;
	ETcsParamCombineOp     Op;   // Add / Mul / Max / Min
};
```

**连带后果（参数域自己要还的债）**：

| 引入的东西 | 必须配套 |
|---|---|
| 表达式树（Composite 可嵌套） | **配置期环检测**（`ParamRef` 成环 = 无限递归） |
| 递归求值 | **求值深度上限** + `ensure` |
| 子树里的 `AttributeScaled` | **依赖边收集要遍历子树**，不是看顶层一个字段 |
| 参数域体量 | 从「值载体」变成「表达式引擎」——这是参数域文档必须正面写清的定位变化 |

> **与聚合器的边界不变**：参数域算出的是一个 `double`，聚合器仍然只做七带折叠、仍然封闭。表达式能力不构成「聚合器开插点」。

> **V1 实证**：`FTcsParamSource_Composite` **全仓 0 命中**——它是本轮新造的概念，不是 V1 遗产。

### 3.1 第 7 条要裁什么（四个已定事实 + 一个待议）

| # | 事实 | 来源 | 状态 |
|---|---|---|---|
| ① | `Composite` **V1 全仓 0 命中**，V2 新造 | 本轮扫描 | ✅ 确认 |
| ② | `Composite` 可嵌套 ⇒ 需**配置期环检测** + **求值深度上限** | 上表 | ✅ 确认（机制要建） |
| ③ | 子树里的 `AttributeScaled` ⇒ `ForEachAttributeRead` **必须递归** | `Attribute.md` §5.2 | ✅ 确认（实现要求） |
| ④ | 聚合器无 Custom 位 ⇒ 组合能力必须落在参数域 | 《数值聚合器》§2.2 | ✅ 确认 |
| ⑤ | **运算集 `Add / Mul / Max / Min` 是否够** | — | ⚠️ **待议** |

**⑤ 的实际分歧点**（不是「够不够」这么笼统）：

| 候选 | 支持理由 | 反对理由 |
|---|---|---|
| 维持四则 | 覆盖「Attack = Str×2 + Agi×0.5」这类全部已见配置 | 无 |
| 加 `Sub` / `Div` | 字面自然 | **减法与除法的操作数顺序敏感**，而 `Terms` 是数组——读不出「谁减谁」。要做就得改成 `{Left, Right}` 二元节点，与四则的统一形态不一致 |
| 加 `If` / 比较 | 能表达分段公式 | **那已经是脚本**。同一能力链路径能做（`Branch`），在参数源里再造一套是 D11 已否决的方向 |
| 加 `Clamp` | 常见需求 | 值域收口归属性域（`Attribute.md` §4），参数域做一次就是两处收口 |

> **我倾向维持四则**，但这是第 7 条要你裁的，不是我定的。

---

## 4. 与 Attribute 域的接口

- Attribute 域的 `FTcsAttrModInstance` 持有 `FTcsParamValue`（值载体），`FTcsAttrModDef` **不持有**（保泛用性）。
- 参数源通过 `FTcsParamEvaluateContext` 拿**属性读口**——V1 是接口 `ITcsAttributeProvider`，**V2 已裁删除**，改为直接走属性子系统。
- 求值上下文带 `bSnapshot`：`true` 时 `AttributeScaled` **MUST NOT** 登记依赖边（快照不跟随）。
- `Attribute.md` §7.2 已把修正器的模式字段定为 `ETcsParamMode Mode`（枚举归参数域），本域必须提供它的**定义与语义**。

---

## 5. 值载体与源族的命名（开篇第 0 号裁决）

**触发（2026-11-06 用户质询）**：用户的印象是「Def 里配的叫 `ParamSource`、运行时真正起作用的叫 `ParamValue`」，并问这是否不符合 TCS 的统一命名规范。

**事实澄清**：这个对应关系不成立——真实关系是**包含**，不是前后两态。

```text
FTcsParamValue                    ← 载体（外壳）。Def 里配的是它，实例里拿的也是它
└── Source : FInstancedStruct
    └── FTcsParamValueSource      ← 抽象基类，meta=(Hidden)
        ├── FTcsParamSource_Literal
        ├── FTcsParamSource_ParamRef
        ├── FTcsParamSource_Composite
        ├── FTcsParamSource_AttributeScaled
        ├── FTcsParamSource_HostDelegate
        ├── FTcsParamSource_{State,Instigator}Level{Array,Map}
        └── FTcsParamEnumerableSource    ← 可枚举分支基类
```

**V1 证据**：`TcsCore/Public/Parameter/TcsParamValue.h:34-55`（`FTcsParamValue` 的 `Source` 成员 + `Evaluate` 转发）、`TcsParamValueSource.h:84-137`（基类）。V2 两侧同为 `FTcsParamValue`：配置态 = `UTcsAttributeDef::DeriveExpression`，实例态 = `FTcsAttrModInstance::Operand`（`Attribute.md` §1 / §7.2）。

**已裁决（本轮，Attribute 域范围内）**：`FTcsAttrModInstance` 的值成员由 `Value` 改为 `Operand`——`FTcsParamValue` 自身内含 `Source`（值从哪来），而该结构另有 `Source`（谁施加的），同结构体一词两义。见 `Attribute.md` §7.2 的 Why Not。

**待裁两题**：

| # | 问题 | 我的倾向 |
|---|---|---|
| **P2** | 族根名 `FTcsParamValueSource` 与具体类前缀 `FTcsParamSource_` 差一个 `Value`——6+ 个具体类派生自它，名字里都不含基类名 | **基类改 `FTcsParamSourceBase`**（与 StateTree `FStateTreeConditionBase` 同构）；6+ 个具体类零改动 |
| **P3** | `FTcsParamValue` 装的是「怎么算出这个数」而不是「一个数」，且在配置态也出现 | **不改**——它是外观，外观名按用途取、不按实现取（同类先例：GAS `FGameplayEffectModifierMagnitude` 内部也是策略分派）。若否决此理由，则成套改 `FTcsParamExpr` 族（载体 + 基类 + 全部具体类） |

> **关于 P2 的现状形状**：族内现有两套规律——基类驼峰无下划线（`FTcsParamValueSource` / `FTcsParamEnumerableSource`）、具体类下划线（`FTcsParamSource_*`）。所以「派生名不含基类名」本身不算硬伤；P2 真正裁的是**要不要让基类名自解释**。

> **改名成本为零**：V1 代码与资产整体删除（`03-词汇与命名.md` §5），⇒ P2/P3 的取舍只看「新名是否更准」，不看「改动波及多大」。

---

## 6. `Live` / `Snapshot` 的归属 ✅ 已裁决 = 归参数域（方案 A）

**触发（2026-11-06 用户质询）**：用户追问「按你的推论，`ParamSource` 直接在运行时使用，意味着参数在运行时不会被修改或更新，即便 `StateLevelArray` 这一类也不会吗」。

**该质询揭出的真问题**：我原来的表述只覆盖了**求值入口**（源是不可变纯函数，两态同一份），漏掉了**求值时机与结果的冻结**——那是参数域的另一半语义。

**V1 的实情（代码证据）**：

| 事实 | 证据 |
|---|---|
| 源对象本身不可变 | 四个等级源的 `Evaluate` 全 const；`Values` 是 `EditAnywhere, BlueprintReadOnly`（`TcsParamSource_StateLevel.h:34-35, :68`） |
| 参数值在运行时**有状态** | `FTcsParamSnapshot`（`TcsStateSnapshot.h:47-63`）：施加瞬间求值冻结，实例生命周期内不重算 |
| 冻结语义归参数域 | `TcsStateSnapshot.h:60-61` 原话：「实现名取设计名 `FTcsParamSnapshot`——它是**参数域**的概念（修正器物化、技能侧参数行都要读它），不是状态模块专属」 |
| 模式分流的唯一实现 | `ETcsParamMode`（`TcsParamRow.h:17-21`），消费者只有 `TcsCastOps_Snapshot.cpp:42` |
| **修正器路径不按 Mode 分流** | 该文件注释：「`EPM_Live` / `EPM_Snapshot` 在本路径上同解：两者都走实时折叠（快照只影响快照面）」 |
| 快照里预留过 Live 位 | `TcsStateSnapshot.h:38-39`：`FInstancedStruct SourceRef`——「Debug 可见取值来源 / **Live 化预留**；无消费者」 |

**V1 没有 Curve 版等级源**：全仓 `Source/` 扫 `Curve` = 0 命中。只有 `{State,Instigator}Level{Array,Map}` 四种。

**用户裁决（2026-11-06）**：**方案 A —— 快照机制完全归参数域**。

**落地要求（参数域文档必须承担）**：

| # | 事项 |
|---|---|
| 1 | `ETcsParamMode` 的定义与语义住参数域（`TcsCore/Public/Parameter/`），值形 `{ Snapshot, Live }`（V2 无前缀风格） |
| 2 | 「此刻该不该求值、求完要不要冻」的判定归参数域 |
| 3 | 冻结值的**存储**归持有者（属性实例的 `SnapshotValue`、状态/Buff 的 `FTcsParamSnapshot`），参数域只提供语义 |
| 4 | 参数域默认 `Snapshot`；属性域默认 `Live`（修正器本来就实时折叠，见上表第 5 行） |
| 5 | 求值上下文里的 `bSnapshot` 位（控制 `AttributeScaled` 要不要登记依赖边）同归参数域 |

**已同步到 Attribute 域**：`04-领域/Attribute.md` §7.2 的 `ETcsValueMode ValueMode` 已删除，改用 `ETcsParamMode Mode`；§7.3 改写为「模式归参数域，冻结位归本域」并补分工边界表与 Why Not。

> **一处待确认**：要求 1 说「住参数域」，但**「参数域」在 V1 根本不是一个模块**（§0）。这正是 §9 三选一要解的问题——「它的定义住 TcsCore」与「它属于一个叫 Parameter 的领域」是两件事，见 §10 第 3 条。

---

## 7. 参数域也有自己的修改器 —— 会回头影响 Attribute 域（2026-11-06 用户提出）

**用户原话意旨**：「Parameter 也会有属于自己的修改器（比如 Skill 模块的技能修改器），所以可能会影响到 Attribute 域，不过最终应该会在 Parameter 域裁定完再说」。

**已确认的事实**：

| 东西 | 改的是什么 | 住哪 |
|---|---|---|
| `FTcsAttrModInstance`（属性域） | 属性的值 | `Attribute.md` §7.2 |
| `FTcsNumericParamModifier` / `FTcsNumericParamModInstance`（V1 技能侧） | **参数的值** | V1 `TcsSkill/Public/Skill/TcsNumericParamModifier.h` |
| `FBoolSwitchModifier` | 布尔开关的值 | **V1 未建**（`R6.5-e` 未开工） |

### 7.1 真正的耦合点 = 折叠器归谁

V1 明文写着（`SPEC-05-skill` D5-5 v3，原文见 `dec-02-fold-display.md:33`）：

> 「**折叠器单份住 TcsAttribute**（本模块提供类型与词汇），三处共用：M2 属性聚合、M5 参数链、TcsDamage 流程属性容器。归属理由：三个消费者的编译边都已含 TcsAttribute（零新边），且不把 Op 枚举与带序词汇搬进零语义的 TcsCore。」

**V1 的实证形态**：`TcsNumericParamModifier.h:11-12` 直接 `#include "Attribute/TcsAttrModInstance.h"`，注释写「`ETcsAttributeOp`（封闭五带）住 M2——**M5 复用同一套带序**，MUST NOT 另立同义枚举」。⇒ **产品模块反向依赖属性域，只为一个纯函数 + 一个枚举。**

**这个理由在 V2 已经失效一半**：V1 的归属论证是「三个消费者的编译边都已含 TcsAttribute（零新边）」——那是**既成事实的描述**，不是**正确性的证明**。V2 的模块边界重新设计后，这条边的方向可以重新选。

**必须先想清楚的三问**：

1. **参数修改器与属性修改器的带式折叠是不是同一套？** V1 说「与 M2 完全同式、顺序无关」（同五带 `Override / Add / PercentAdd / Mul / FlatAdd`、同带权）。若是同一套，折叠器不该由任何一个域私有。
2. **V2 的折叠器住哪？** 三个候选：① 继续住属性域（参数域反向依赖属性域）；② 上移到核心基线（两者都依赖它）；③ 参数域提供（属性域反向依赖参数域）。**§3 聚合接线与 `Attribute.md` §7.2 修正器都直接受此影响。**
3. **参数修改器的「作用域」与属性修改器的「落点」是不是同一个概念？** 属性侧有 `ETcsAttrLayer {Basic, Extra, Global}`；参数侧的 `CompeteGroup` / `Source` 撤销级联是另一套。两者形状相似但语义不同，MUST NOT 强行统一成一个枚举（同第 57 项 `ETcsValueMode` 的教训）。

**与 `Attribute.md` 的接触面**：

| 接触点 | 影响 |
|---|---|
| §3 聚合接线（对接《数值聚合器》） | 若折叠器上移或迁走，§3.4 的收集/求值要跟着改 |
| §7.2 `FTcsAttrModInstance::Operand` | 若参数域要消费属性修正器的折叠结果，`Operand` 可能不再是 `FTcsParamValue` 那么简单 |
| §9.2「它不做什么」 | 现在写着「折叠公式 → 《数值聚合器》」，折叠器搬家后这句的指向要改 |

> **不预建裁决**：本节的用途只是**登记耦合点**，参数域文档开工时必须先回答三问，再动 `Attribute.md` 的对应章节。

---

## 7A. 折叠器归属：V1 那一份应当删除，不是搬走（2026-11-08 已裁）

> **触发**：用户直觉「感觉这个折叠器说的很像是我在 V2 一开始提到的核心基线的【数值聚合器】」。**这个直觉是对的，而且比「很像」更强一层——两者是同一个东西，V1 那一份是它的弱化前身。**

### 7A.1 三处实证

**实证一：V2 聚合器 §4 的消费者，与 V1 折叠器的三个调用点逐一对应。**

| V1 折叠器调用点（实到 3 处） | V2《数值聚合器》 | 所在章节 |
|---|---|---|
| `TcsAttributePipeline.cpp:251`（属性聚合） | §4.1 属性聚合 | ✅ |
| `TcsParamChain_Evaluate.cpp:98`（技能参数链） | §4.2 参数链 | ✅ |
| `TcsFlowAttributes.cpp:45`（伤害流程属性） | §4.3 流程属性 | ✅ |

而《数值聚合器》§4.0 那张总表的**列标题就是「属性 / 参数 / 流程属性」**——三个消费者一个不少、一个不多。两者描述的是同一组分接线。

**实证二：V2 已把 V1 的调用形态写成「退化形态」。**

《数值聚合器》§3 的单段重载注释原文：「单段便捷重载：**V2 初版四带公式的退化形态**」。

对比两边签名：

```cpp
// V1（TcsAttributeBandFold.h:115）
inline double FoldTcsAttributeBands(
	double BaseValue,
	TConstArrayView<FTcsAttributeBandEntry> Entries,
	ETcsAttrOverrideTieBreak OverrideTieBreak = OTB_Max);

// V2（数值聚合器 §3）
double EvaluateAggregatedValue(
	double BaseValue,
	TConstArrayView<FTcsAggregateEntry> Entries,
	ETcsOverrideStrategy Strategy = ETcsOverrideStrategy::MaxValue);
```

**同签名、同语义、同退化关系**。V1 的 `TAO_Override` 就是 V2 的 `OverrideGlobal`（单段重载里它是唯一的覆盖带）。

**实证三：V1 的带权助手是死代码，且 V2 已明文否决它。**

- `GetTcsAttributeBandWeight`（`TcsAttrModInstance.h:41`）全仓 **1 处命中 = 它自己的定义**，零消费者。
- 《数值聚合器》§5.3 标题「**不提供「带权数字」**」，原文：「聚合器本身就是按 `Op` 分桶的，**根本不需要带权数字**。而一个「看起来能改变聚合顺序但实际上无消费者」的字段，只会诱导错误配置。」

**⇒ V2 早已预判并否掉了 V1 那个字段，只是当时不知道它在 V1 里存在。**

### 7A.2 用户裁决（2026-11-08）

> 「按照你给的倾向建议执行，至于 SourceHandle，**聚合器本身只执行计算，确实不需要保存 SourceHandle**，但是调用聚合器的几个域，却需要保留 SourceHandle」。

**三条结论**：

| # | 结论 |
|---|---|
| 1 | **V1 的 `FoldTcsAttributeBands` 删除，不搬**——V2 聚合器严格更强（N 段 / 3 覆盖 / 7 策略 vs 单层 / 1 覆盖 / 固定三级），且「搬到核心基线」这个选项不存在：聚合器**本来就是**核心基线（五项之一） |
| 2 | **覆盖比较维持单级**（《数值聚合器》§3.1 原裁决不变）：7 个策略互斥、无隐藏的更高优先级规则。V1 那套「优先级 → TieBreak → 有符号值」三级比较**正是**§3.1 那句话要否掉的「隐藏规则」 |
| 3 | **聚合器条目不持有 `FTcsSourceHandle`**，只持一个 `uint64` 施加序（见 §7A.3） |

### 7A.3 `Source` 字段的形态：`FTcsSourceHandle` → `uint64`

用户已指出分工，我把它写成可执行的判据：

| 层 | 持有什么 | 为什么 |
|---|---|---|
| **聚合器**（纯函数） | `uint64 OrderSeq`（施加序） | 它只做**比较与累加**。比较需要的是「谁更早」这一个**序关系**，不需要句柄的身份与有效性 |
| **域**（属性 / 参数 / 流程） | `FTcsSourceHandle Source` | 域要的是**级联撤销锚点**（`RemoveBySource`）与**UI 归因**（「被【破甲】覆盖了」）。那是句柄的完整语义 |

**这三条改写点的实证依据**：

| 用途 | 位置 | 聚合器需要的是 |
|---|---|---|
| `LatestApplied` / `EarliestApplied` 比较 | §3 `IsStronger` | 序关系（`uint64` 够） |
| 定序累加排序键 | §3 `FoldBands` | 序关系（`uint64` 够） |
| `FTcsAggregateDetail::OverrideWinner` | §3.3 明细入口 | **这是唯一真需要句柄的地方**——界面要显示「被【破甲】覆盖了」 |

**⇒ 明细的赢家字段不能降成 `uint64`**，否则界面拿不到来源身份。**这是一个必须单独处理的分歧点，见 §7A.4。**

**为什么降成 `uint64` 是对的**（三条判据）：

1. **耦合**：聚合器是「零领域词汇」的纯函数基线，而 `FTcsSourceHandle` 带 `IsValid()` / `operator==` / 进程唯一发号器的语义——聚合器一个都不用。**只用 `.Id` 却持有整个句柄，是过度耦合。**
2. **反射友好**：`FTcsSourceHandle` 是**非反射纯 C++ struct**（`TcsSourceHandle.h:20` 是裸 `struct`，无 `USTRUCT`）。它的存在直接禁止聚合器成为 `USTRUCT`。换成 `uint64` 后，`FTcsAggregateEntry` 的其余字段都是反射类型，聚合器**可以**被做成Blueprint 可达。
3. **铁律 ④ 的镜像**：注释只写当前契约。字段名叫 `Source` 而实际只读 `.Id`，读代码的人会以为聚合器在管身份。

### 7A.4 唯一的分歧点：明细的 `OverrideWinner`

§7A.3 的降型会让明细结构丢掉来源身份。三个处理方式：

| | 做法 | 代价 |
|---|---|---|
| **A** | 明细照降成 `uint64 OrderSeq`，域自己拿这个号去查自己的账本反查句柄 | 域要多一次反查（O(n) 扫账本，或建索引）；明细不再自足 |
| **B** | 值入口用 `uint64`，**明细入口单独收 `FTcsSourceHandle`**（两个重载） | 接口翻倍，但语义最清晰 |
| **C** | 维持 `FTcsSourceHandle` 不动 | 聚合器继续不能是 `USTRUCT`，且继续持有一个只用 `.Id` 的完整类型 |

**我倾向 A**，判据是《数值聚合器》§3.3 自己定的「明细只放调用方拿不到的量」——而**来源句柄恰恰是调用方给的**（`Entries` 是它递进来的）。让明细返回一个序号、域拿序号回查，符合「调用方能自己算出来的不重复给」。

> **但这条我没裁**——它取决于「域反查句柄」的成本是否可接受。属性域的账本本来就是 `TArray<FTcsAttrModInstance>`，扫一遍是 O(n)；而明细是 UI 路径（§3.3 已裁「MUST NOT 进热路径」），O(n) 完全可接受。**⇒ 事实上 A 已经成立。**

### 7A.5 既有文档的一处硬矛盾（**已于 2026-11-11 落地**）

《数值聚合器》§3.1 与 §4.3 存在一条**互相打架**的配置：

| 位置 | 原文 |
|---|---|
| §3 接口注释（`:137`） | `FTcsSourceHandle Source; // 施加序（仅 Override 带 + 仅 LatestApplied/EarliestApplied 读）` |
| §3.2 标题 | 「**施加序用 `FTcsSourceHandle`**，不引入 Sequence ✅」——已裁 |
| §4.3 流程域 | 「**流程域 MUST NOT 有 `Source` 字段**」——已裁 |
| §4.0 总表 | 「分配 Source：属性=是 / 参数=是 / 流程=**否**」 |

**矛盾**：如果聚合器的条目结构**必须**有 `Source` 字段（§3 接口），那流程域构造条目时就**必须**填它——可§4.3 明令流程域不准有 `Source`。

**V1 的实证对照**：`TcsFlowAttributes.cpp:34-41` 构造 `FTcsAttributeBandEntry` 时只填 `Op / Value / OverridePriority`，**不填 Source**（因为该结构根本没有这个字段）。V1 没有矛盾，因为 V1 的条目结构里没有 Source。

**⇒ 引入 `Source` 到 `FTcsAggregateEntry` 时，§4.3 那条裁决从未被重新检查过。**

**降成 `uint64 OrderSeq` 顺带解决了这个矛盾**，因为语义变了：

| | 有 `Source` 时 | 有 `OrderSeq` 时 |
|---|---|---|
| 流程域 | ❌ 必须填一个它不该有的来源 | ✅ 可以只填 `OrderSeq = 0`（= 无施加序语义，`LatestApplied`/`EarliestApplied` 对它无意义） |
| §4.3 的禁令 | 与 §3 接口冲突 | **不冲突**——`OrderSeq = 0` 不是「有 Source」，只是「不排施加序」 |

**落地结果（2026-11-11）**《数值聚合器》已改完，五处全部对齐：

| 位置 | 改成 |
|---|---|
| §3 条目结构 | `FTcsSourceHandle Source` → **`uint64 OrderSeq = 0`** |
| §3.2 标题与正文 | 「施加序用 `FTcsSourceHandle`」→「**施加序用 `uint64 OrderSeq`，不持有 `FTcsSourceHandle`**」，并补了「为什么降型」三条判据 |
| §3.3 明细 | `OverrideWinner`（句柄）→ **`OverrideWinnerSeq`（序号）**，两处结构都改 |
| §4.3 流程域 | 「MUST NOT 有 `Source` 字段」→「**MUST NOT 有 `FTcsSourceHandle`**」，并写明填 `OrderSeq = 0` / `Priority = 0` |
| §7 接线表 | 「分配 Source：流程=否」保留，另注流程填 `0` |
| §8.2 零依赖句 | 「`double` + 枚举 + `FTcsSourceHandle`」→「**`double` + `uint64` + 枚举，连 `FTcsSourceHandle` 都不引**」 |

> **`0` 这个保留值是把「矛盾」变成「语义」的关键**：它不是「有一个 Source」，而是「**不排施加序**」。`LatestApplied` / `EarliestApplied` 对全 `0` 的集合恒打平，落到 §5.1 决胜级按值比较——**这正是流程域该有的行为**。

### 7A.6 一个不在本域、但必须登记的连带（**已于 2026-11-11 落地**）

《数值聚合器》§4.1 原定「同源刷新 = **原地改值 + 标脏**，重算归读路径（惰性）」，
而 `Attribute.md` §2.2 已裁为「**写时重算**，帧末派发」。⇒ **两份文档对同一件事给了相反的时机。**

**处置：统一为「写时重算」。** 《数值聚合器》§4.1 已改，并补了三条理由：

| # | 理由 |
|---|---|
| ① | **链步骤是同步的**——`ApplyAttributeSet` → 行求值 → 行为链再入起链，全在同一帧同一调用栈。「先写、后读」必须读到新值 |
| ② | **读侧要保持纯内存返回**——`AttributeScaled` 跨实体读，读时重算会连锁唤醒另一侧；且宿主源回调里再读属性会**求值中途重入** |
| ③ | **批已经能表达「这一批算一次」**——惰性想解决的「多次写只算一遍」由 `BeginBatch` / `CommitBatch` 解决，不需要第二套机制 |

> **惰性的唯一收益是「少算」，代价是「可能读到旧值」**——对同步链式执行、要求「写完立即可读」的系统，这个交换不划算。

> **一处必须分清**：批内读到的是批开始前的值，这**不是惰性的残留**。批的语义是「要么一起生效、要么都还没生效」（`Attribute.md` §2.2）。**批内不读，与「写时不重算」是两件事。**

**连带改的**：`UpdateMagnitudesBySource` 的契约 ④ 从「标脏一次，不重算」改为「**写成功后立即结算**」；§7 责任表的「同源刷新」行同步。

---
## 8. 调研实证：六条「能力已建、契约悬空」

> 这是本轮全仓扫描的直接产物，**每条都可复核**。它们的共同形态：类型在册、有默认降级路径、**没有任何实现者** ⇒ 配了它不报错、只是静默落兜底。

### 8.1 `ITcsAttributeProvider` —— 0 实现者

- 声明：`TcsAttribute/Public/Attribute/TcsAttributeProvider.h:27`；唯一消费者 `TcsParamSource_AttributeScaled.h:105`。
- 全仓 `public ITcsAttributeProvider` = **0 命中**。
- 后果：`FTcsParamSource_AttributeScaled::Evaluate` 恒走 `Provider.GetObject()` 判空 ⇒ **恒返回 `Fallback`**。
- 代码自承：`TcsSkillDefData.h:108` 注释「全仓无实现者……配了它只会落兜底值」。
- **V2 已裁 2026-11-10（D13）**：删除。替代 = `FTcsParamEvaluateContext` 增 `TObjectPtr<UWorld> World` 字段（D14），源自己调 `UTcsAttributeSubsystem::Resolve(Context.World)`。
- ⚠️ **这个「0 命中」只扫了 `*.h/*.cpp`**，未扫 C# —— 见 §调研方法边界。8.2/8.4 同此局限（未复核）。

### 8.2 `ITcsEntityLevelProvider` —— 0 实现者

- 声明：`TcsBuff/Public/Host/TcsEntityLevelProvider.h:40`；消费者：`FTcsParamSource_InstigatorLevel{Array,Map}`。
- 全仓 `public ITcsEntityLevelProvider` = **0 命中**。
- 后果：**四个等级源里只有两个真能用**——`StateLevelArray/Map` 读 `Context.EffectiveLevel`（纯字段，可用）；`InstigatorLevelArray/Map` 读 `LevelProvider`（空）⇒ **恒落 `Fallback`**。
- 这是一条**只能靠读代码发现**的结论：四个源在同一文件里、签名对称、注释也对称，但**其中两个从未工作过**。
- **V2 已裁 2026-11-09（D8/D9/D10）**：四个源 + 本接口**全保留**，且**住 State 域**。接口是 (a) 类宿主契约（不建就要反向依赖宿主），**MUST NOT 因零实现而砍**，也**不补实现**。
- ⚠️ 局限同上（只扫了 C++）。

### 8.3 `ITcsParamSourceHost` —— **我的统计错了，它有 C# 实现者**

- 声明：`TcsCore/Public/Parameter/TcsParamSourceHost.h:38`；转发器 `FTcsParamSource_HostDelegate`。
- ❌ **我最初写「0 实现者」，这是错的。** V1 验收记录 `evidence/2026-10-04-state-param-snapshot-and-level-sources.md:124` 明写：宿主脚本夹具 **`UTcsDevGcParamSourceHost`**（C# / UnrealSharp）实现过它，且做过真实原生 GC 后的存活验证。
- **错因**：我只 grep 了 `Source/**/*.h|*.cpp`，**没扫 `*.cs`**。V1 自己在 `plan-r6-skill-layer.md:1024` 记过**完全相同**的错误（「第二次 = 『`ITcsParamSourceHost` 零实现者』（只扫了 `*.h`/`*.cpp`、没扫 `*.cs`）」）。我重犯了它。
- 该夹具随 V1 一起被删除（当前 `Content/` 与 `Script/` 都没有），但**它证明了这条路走得通**。
- **V2 已裁 2026-11-09（D11）：仍然删除。** 理由不是「零实现者」，而是三条：
  1. **它把两件事塞进一个源**——「条件」归触发/流程（`WaitEvent` + `Branch`），「值计算」归参数源；
  2. **C++ 宿主派生 `FTcsParamValueSource` 已覆盖同需求**（`meta=(BaseStruct)` 让宿主的源自动出现在 picker 里，零插件改动）；
  3. **脚本宿主有别的路**——配值/组合/流程都不需要新源类型；真需要新数学语义，宿主 C++ 派生，这正是框架该有的边界。
- 用户原话：「我觉得这些功能就应该宿主项目自己实现」「『当低血量时移速 +30%』就应该是一个被动 Buff，EffectChain 里 WaitEvent，给 Owner 上一个移速加成 Buff」。
- **V1 里这条链路每一环都已存在**：`FTcsStepWaitEvent`（`TcsStepWaitEvent.h:39`）+ `FTcsChainEventWaitRegistry`（同一 tag 的 N 条等待只订阅一条）+ `FTcsStepBranch` + `FTcsStepApplyState`。**三个步骤，零新机制。**

### 8.4 `ITcsParamSourceHost` 的姊妹接口 `ITcsSkillCostPolicyHost` —— 也是 0 实现者

- 声明：`TcsSkill/Public/Skill/TcsCostPolicyHost.h:66`。
- 后果：`FTcsSkillCostPolicy_HostDelegate` **fail-closed**（`CanAfford` / `CommitCost` 均返回 false）。
- ⇒ **V1 的「宿主可自定义消耗策略」这条能力，今天完全没被使用过。**
- ⚠️ 同 8.1 的统计局限（只扫 C++，未复核 C#）。**V2 尚未就此条裁决。**

### 8.5 `FTcsParamSource_ParamRef` —— 只在状态侧物化路径真能取到值

参数表 `Context.ParamTable` 的各处填法（实证）：

| 调用点 | `ParamTable` | 后果 |
|---|---|---|
| `TcsBuffModifierMaterializer.cpp:35` | ✅ `FTcsStateSnapshotScope` 绑该实例快照 | **唯一可用路径** |
| `TcsCastOps_Phase.cpp:55` | ✅ 绑 run 快照 | 可用 |
| `TcsCastOps.cpp:550` | ✅ 绑 run 快照（物化 `ParamChainRows`） | 可用 |
| `TcsCastOps.cpp:39`（`MakeContext`） | ❌ 恒空 | `ParamRef` 落兜底 |
| `TcsCooldownOps.cpp:150` | ❌ 恒空 | 同上 |
| `TcsCostOps.cpp:42` | ❌ 恒空 | 同上 |
| `TcsParamChain.cpp:128`（外部施加） | ❌ 恒空 | 同上 |
| `TcsParamChain_Evaluate.cpp:114` | ❌ 恒空 | 同上 |
| `TcsStepApplyState.cpp:77` | ❌ 恒空（注释自承「链侧参数表载体不存在」） | 同上 |

⇒ **`ParamRef` 不是一个「到处能用」的源**：它在技能侧的多数路径上恒落兜底。这是一个**文档从未写清、只能靠逐点追踪才能发现**的口径。

### 8.6 `ETcsParamMode` —— 三个配置面，真正按它分流的**零处**

> **本节于 2026-11-07 全面改写。** 初稿写的是「1 个真消费者 + 2 处零读取者」，那个结论**错了**——见下方实证。

**初稿的错误**：我把 `TcsCastOps_Snapshot.cpp:42` 的 `if (Row.Mode == EPM_Live) continue;` 当成了「真分流」。它不是——它是**一个没有对称读口的半条机制**。

#### 实证 A：`Mode` 只影响快照，快照又不是唯一读路径

| 地方 | 读的是什么 | `Mode` 生效吗 |
|---|---|---|
| `TcsCastOps_Snapshot.cpp:42` | 构建 `FTcsParamSnapshot` | ✅ **Live 行被跳过、不进快照** |
| `TcsParamChain_Evaluate.cpp:145-153`（`EvaluateParamKey` 的初值） | **直接遍历 `Def->Params` 求值** | ❌ **完全不看 `Mode`** |
| `TcsBuffOps_Snapshot.cpp:52-80`（Buff 侧 `BuildSnapshot`） | 构建快照 | ❌ **完全不看 `Mode`**（同一件事，另一处实现） |

**⇒ 关键矛盾**：`Mode = Live` 的行**不进快照**，可 `EvaluateParamKey` 又**不看 Mode、直接从参数行求值**。两者凑在一起的结果是：

- 命中参数链折叠（`EvaluateParamKey`）时，`Live` 与 `Snapshot` 的**差别就是零**——两边都从 `Def->Params` 实时求值；
- 命中参数表读取（`Ctx.ParamTable` → 快照）时，`Live` 行的键**查不到**，落 `Fallback`。

**同一个键，两条读路径给出两个不同的数**——这才是 `Mode` 的真实状态：它不是「一个消费者」，它是**半条机制被另一条读路径绕过**。

#### 实证 B：Buff 侧与技能侧对同一个字段的处理不一致

| | 技能侧 `BuildSkillSnapshot` | Buff 侧 `BuildSnapshot` |
|---|---|---|
| 是否检查 `Row.Mode` | ✅ `:42` | ❌ **没有这一行** |
| 其余逻辑（`SourceRef` / `Override` / 值约定 / `AllowsValueConvention`） | 逐字相同 | 逐字相同 |

两个函数体是**复制关系**，只在 `Mode` 这一处分叉。这不是疏忽——`TcsParamRow.h:89-93` 与 `TcsBoolSwitchRow.h:69-70` 都在注释里**如实承认**了缺口。

#### 实证 C：`Mode` 与载体的字段分布是**强的负相关**

| 统计项 | 数字 |
|---|---|
| `FTcsParamValue` 作为字段 | **20 处，跨 6 模块** |
| 这 20 处里所在结构体带 `Mode` 的 | **1 处**（`FTcsNumericParamRow`） |
| `ETcsParamMode` 作为字段 | **2 处** |
| 这 2 处里所在结构体含 `FTcsParamValue` 的 | **1 处**（同上） |
| 剩下那处 | `FTcsBoolSwitchRow::Mode`——所在结构体**连载体都没有**，存的是 `bool Base` |

**19 / 20 处载体没有模式列**，而有模式的那一处（布尔开关）连载体都没有。

#### 实证 D：代码自己写明「在本路径上同解」

`TcsCastOps_Snapshot.cpp` 的文件注释原文：

> 「`EPM_Live` / `EPM_Snapshot` 在本路径上同解：两者都走实时折叠（快照只影响快照面）」

同一载体、同一求值路径，模式的差异**被抹平**。只有「模式不是载体属性、而是某条路径要不要冻结的决策」才可能产生这句话。

#### §3 的 `Composite` 会把「模式属于载体」直接推成荒谬

§3 定了要引入 `FTcsParamSource_Composite { TArray<FTcsParamValue> Terms; }`。若模式随载体走：

`Attack = Strength×2 + Agility×0.5` 的表达式树里有 **3 个载体**，各带一个 `Mode`。求值时听谁的？**一个表达式不可能有多个求值时机。** ⇒ 模式必然在载体之外。

#### 结论（用户 2026-11-07 已裁）

**模式与载体正交，模式归持有者。**

用户裁决原话：「我自己用更贴近业务需求的几种情景思考了一下，在 **SkillParam-SkillModifier 这条线上，确实没有使用 Snapshot 的需求**，所以我也同意你的【模式与载体正交，模式归持有者】的设计」。

> **裁决的业务依据比我的技术论证更硬**：技能参数链整条线**今天就不需要冻结**（`EvaluateParamKey` 每次都重算，`FoldParamKeyWithBase` 由调用方给初值）。V1 的 `Mode` 列在技能参数行上**从来没有被任何业务需求要求过**——它是照着状态侧的形态**对称复制**过去的。

**V2 的连带结论（三条）**：

| # | 结论 | 影响的 V1 东西 |
|---|---|---|
| 1 | `Mode` 不是载体的字段 | `FTcsParamValue` **不得**加 `Mode` |
| 2 | `Mode` 只在**真有冻结语义**的持有者上声明 | 20 处载体里，只有属性修正器实例（`Attribute.md` §7.2）与状态/Buff 实例快照需要它 |
| 3 | **声明了 `Mode` 就必须有对应的冻结存储与分流读口**，否则这个字段是死配置 | `FTcsBoolSwitchRow::Mode`（零读取者）、状态/Buff 侧 `FTcsNumericParamRow::Mode`（零读取者）**要么补齐、要么删掉** |

> **这条裁决同时判死了 §8.6 那个形态**：V1 的 `Mode` 是「一个枚举 + 三个配置面 + 半条分流」。V2 若照抄，会原样继承「同一个键两条读路径给出两个数」的缺陷。

### 8.7 附带发现：`TcsNotation` 模块只有一个内容

`Source/TcsNotation/Public/` 全部内容 = `TcsValueConvention.h`（57 行）+ 日志通道。它是一个**独立编译单元 + 一条模块依赖边**，换一个 57 行的枚举与静态助手。
机制 14 的 V2 处置已经写了：「**机制保留，模块取消**——值约定并入『数值与参数』域，不再独立成模块」。

> **这条是 §9 的一个重要输入**：如果「值约定」并入参数域，那么参数域就是**第一个有真实跨模块内容物的域**；如果它留作核心基线，那么「并入」这个词就没有落点。

---

## 9. 三选一：参数各部分该住哪（**待 §10 走完后才裁**）

> **本节不是裁决，是把选项摆出来备查。** 用户已明确「最后再确定这三个选项」。

### 9.1 判据来自哪里

`INDEX.md` §2.2 的原文判据：

> 一个设施**不含任何领域词汇**、**被 ≥2 个领域依赖** ⇒ **核心基线**。
> 含领域词汇 ⇒ 属于那个领域。

而 `INDEX.md` 已把 Parameter 列为**七个领域**之一。**这里存在一个未解决的张力**：按 §0 的调研，这个「域」的大部分内容不含领域词汇、且被 7 个模块依赖。

### 9.2 三个选项

| | **A：全部并入核心基线** | **B：拆成基线 + 领域** | **C：维持一个域** |
|---|---|---|---|
| 形态 | 载体系 + 模式 + 修改器全进 `TcsCore`（或新增核心基线「数值源」） | `02-核心基线/数值源.md`（载体 + 源族 + 上下文 + 值约定）+ `04-领域/Parameter.md`（模式 + 修改器 + 参数表 + 快照） | 全部进 `04-领域/Parameter.md`，属性/伤害反向依赖它 |
| 符合 §2.2 判据 | 前半（载体）符合，后半（模式/修改器）不符合 | ✅ 逐件符合 | ❌ 让 Attribute 域依赖一个含领域词汇的域 |
| `Attribute.md` 要改吗 | §7.2 / §7.3 大改 | §7.3 只需指向基线文档 | §3.4 与 §7.2 都要改 |
| 折叠器（§7.1 三问） | 随载体系进基线 | **落基线**（它无领域词汇） | 落参数域，属性域反向依赖 |
| 值约定（§8.7） | 进核心基线 | 进 `数值源` 基线 | 进参数域 |
| 风险 | 模式枚举被无差别塞进基线；`TcsCore` 变成杂物间 | 一个逻辑概念横跨两份文档，读者要跳 | 依赖图出现「属性 → 参数 → 属性」的隐式环 |

### 9.3 我倾向 B，但**判据要等 §10 确认**

理由是「拿消费者数量划边界，比拿名字划边界可靠」：

- `FTcsParamValue` 今天有 **7 个模块**的真实消费者 ⇒ 基线；
- `ETcsParamMode` 今天只有 **1 个消费者** ⇒ 领域；
- `FTcsNumericParamModifier` 只有技能域 ⇒ 领域。

**第 1 条裁完后，这个理由的强度变了**——它不再是「如果两者不可分则 A 成立」的悬案，而是：

> **模式与载体已被确认正交**（§8.6），⇒ **B 的拆分不会劈开一个概念**。原本支撑 A 的唯一前提消失了。

但**这不等于 B 已经成立**：正交只证明了「可以拆」，没有证明「应该拆」。剩下要回答的是——

| 待答 | 落点 |
|---|---|
| 拆分后「参数域」还剩下什么？ | §10 第 8、9 条（快照 + 值约定） |
| 折叠器归属会连带决定载体系归属吗？ | §10 第 2 条 |
| 「域」与「模块」是两个轴，文档该怎么写？ | §10 第 3 条 |

> **A 并未被排除**：如果拆分后「参数域」只剩两三个类型，那它也许不该独立成一个域，而是并进别的域（比如 Attribute 或 State）。这个可能性要等 §10 走完才能判断。

---

## 10. 逐条确认清单

> **走法**：一次确认一条，确认结果就地改写本节状态。§9 的三选一在全部条目走完后再裁。

| # | 要确认的问题 | 我的当前判断 | 状态 |
|---|---|---|---|
| 1 | **`ETcsParamMode` 与 `FTcsParamValue` 是不是同一个概念的两面？** 模式能不能脱离载体单独理解？ | 能——模式描述的是「**什么时候求值**」，载体描述的是「**怎么算**」，两者正交 | ✅ **已裁 2026-11-07：正交 + 模式归持有者**（详见 §8.6 结论）。**连带定下第 3 条**：`Mode` 只在真有冻结语义的持有者上声明，声明了就必须有冻结存储与分流读口 |
| 2 | **折叠器归谁？** | 它不是「归谁」，而是 **V1 那份直接删除**——V2《数值聚合器》就是它的完整版，且已是核心基线。连带：条目 `Source` 降为 `uint64 OrderSeq` | ✅ **已裁 2026-11-08（D2–D5），且三条遗留已于 2026-11-11 全部落地**（详见 §7A）。① 删除不搬 ② 覆盖比较维持单级 ③ 聚合器条目不持 `FTcsSourceHandle`。**§7A.4 明细赢家 → `OverrideWinnerSeq`｜§7A.5 聚合器内部矛盾 → 已消解｜§7A.6 惰性 vs 写时 → 统一为写时重算** |
| 3 | **「住 TcsCore」与「属于 Parameter 域」是不是同一件事？** | **不是**——两个轴 | ✅ **已裁 2026-11-09（D6）**。连带：参数表归参数域（D7） |
| 4 | **源族保留集** | 保留 | ✅ **已裁 2026-11-09（D8/D10）**：四个等级源 + `ITcsEntityLevelProvider` 全保留，**住 State 域**。我原提议的「改走 `ITcsEntityQuery`」**已证伪**（该接口没有 `GetLevel`，且是 C++ 专用面） |
| 5 | **`HostDelegate` 保留吗？** | **删除** | ✅ **已裁 2026-11-09（D11）**。理由：条件是流程的事（`WaitEvent` + `Branch` + `ApplyState` 已能表达），值计算才归参数源；C++ 宿主派生基类已覆盖同需求。**我此前的「保留」判断与「0 实现者」统计都是错的**（后者只扫了 C++，漏了 C# 夹具） |
| 6 | **`AttributeScaled` 读取侧形态** | 保留 | ✅ **已裁 2026-11-10（D12/D13/D14）**：`FTcsParamSource_AttributeScaled` 保留、住 TcsAttribute；`ITcsAttributeProvider` 删除；给 `FTcsParamEvaluateContext` 加 `TObjectPtr<UWorld> World`，**不建新上下文类型** |
| 7 | **`Composite` 的运算集**：`Add / Mul / Max / Min` 够不够？ | 够 | **未开始**（见 §14）。注意：原判断「超出的走 `HostDelegate`」**已失效**——`HostDelegate` 已裁删除（D11） |
| 8 | **`FTcsParamSnapshot` 归谁？** 它已裁为「参数域的概念」，但它今天住在 `TcsState` | 归参数域。V2 里它是**模式语义的存储形态**，与 `ETcsParamMode` 同处 | 未确认 |
| 9 | **值约定（§8.7）并入参数域**，`TcsNotation` 模块取消——确认吗？ | 确认（机制 14 已有此裁决，只是还没落） | 未确认 |
| 10 | **P2 命名**：`FTcsParamValueSource` → `FTcsParamSourceBase`？ | 改 | 未确认 |
| 11 | **P3 命名**：`FTcsParamValue` 是否改 `FTcsParamExpr`？ | 不改 | 未确认 |
| 12 | **架构三选一**（§9） | 倾向 B | **等 1–11 走完** |

**清单之外的登记项**（§10 收不进去、但确实还悬着的）：

| # | 事项 | 落点 |
|---|---|---|
| A | §7.1 三问的第 3 问：属性侧 `ETcsAttrLayer` 与参数侧 `CompeteGroup` 是不是同一个概念 | **不并入 §10**——它属「作用域概念是否统一」，是架构三选一（第 12 条）的前置观察，不是独立裁决项 |
| ~~B~~ | ~~§7A.5 聚合器内部矛盾~~ | ✅ **2026-11-11 已落地**：`Source` → `uint64 OrderSeq`，流程域填 `0`，矛盾消解 |
| ~~C~~ | ~~§7A.6 惰性与写时重算的时机冲突~~ | ✅ **2026-11-11 已落地**：统一为写时重算，《数值聚合器》§4.1 + §7 已改 |
| D | 伤害记录形状 / **事件 Tag 命名公约** | **不并入本表**——两者都不属参数域（分属 Damage 域、跨域）。正文在族外 [`未决-核心基线.md`](未决-核心基线.md) §8–§9。**因果链与 Explain 面板已裁为「不建」**，见族内 [01-定位与判据 §1.2](../tcs-remake-v2/01-定位与判据.md) |

> **原「未决」节的六条已全部并入上表或 §10**（第 1→4 条、第 2→6 条、第 3→7 条、第 4→10/11 条、第 5→1/8 条、第 6→A）。**留着两份会各自过期。**

---

## 11. 2026-11-07 → 11-10 的裁决流水（逐条可回溯）

> **本节的用途**：把这一段对话里**用户明确裁过**的每一条按时间顺序记下来，供后续写正式文档时逐条摘用。**每条都有用户原话或明确确认**，不含我的推断。

| # | 日期 | 裁决 | 用户原话/确认方式 |
|---|---|---|---|
| D1 | 11-07 | **`ETcsParamMode` 与 `FTcsParamValue` 正交；模式归持有者** | 「在 SkillParam-SkillModifier 这条线上，确实没有使用 Snapshot 的需求，所以我也同意你的【模式与载体正交，模式归持有者】的设计」 |
| D2 | 11-08 | **V1 折叠器删除、不搬**（V2 数值聚合器即其完整版） | 「按照你给的倾向建议执行」 |
| D3 | 11-08 | **覆盖比较维持单级**（7 策略互斥，无隐藏三级） | 同上 |
| D4 | 11-08 | **聚合器条目不持 `FTcsSourceHandle`**，只持施加序；**域侧保留句柄** | 「聚合器本身只执行计算，确实不需要保存 SourceHandle，但是调用聚合器的几个域，却需要保留 SourceHandle」 |
| D5 | 11-09 | **明细赢家降为序号、域自己回查账本** | 「你给的方案我不反对」 |
| D6 | 11-09 | **文档层的「域」与实现层的「模块」是两个轴** | 「确实是两个轴」 |
| D7 | 11-09 | **参数表（`FTcsNumericParamRow`）归参数域**（Def 只是持有者） | 「参数表虽然是被 StateDef 引用，但完全可以在 Parameter 域内确定完所有逻辑后，再被 StateDef 引用」 |
| D8 | 11-09 | **「按施法者等级放大效果」能力需要保留** | 「LAC / 另外两款都需要」 |
| D9 | 11-09 | **`ITcsEntityLevelProvider` 是 (a) 类宿主契约，MUST NOT 因零实现而砍；不补实现** | 「TCS 本身是战斗系统插件，开发的很多功能就应该只有宿主项目侧才会使用到，不然 AddAttributeSet、LearnSkill 这种，岂不是也会在 TCS 内部完全没有消费者」 |
| D10 | 11-09 | **四个等级源住 State 域**（含 `ITcsEntityLevelProvider`） | 「可以把 StateLevelArray/Map 放在引用 Parameter 域的 State 域实现」「`ITcsEntityLevelProvider` + 两个 InstigatorLevel 源我觉得也应该放在 State 域」 |
| D11 | 11-09 | **`ITcsParamSourceHost` + `FTcsParamSource_HostDelegate` 删除** | 「我觉得不应该保留……这些功能就应该宿主项目自己实现」+ 「【低血量时移速 +30%】就应该是一个被动 Buff，EffectChain 里 WaitEvent，给 Owner 上一个移速加成 Buff」 |
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
| D24 | 11-10 | **`ETcsAttrReadScope { Current, BaseOnly }` 引入**（后经 D29 扩展） | 「倒是也没什么问题」 |
| D25 | 11-10 | **弗拉米尔这类「成对修正器」用批，配置者保证成对**；不建 `AttrModGroupDef` | 「我觉得 AttrModGroupDef 没什么必要」 |
| D26 | 11-10 | **不借鉴「层序替代依赖图」** | 「我也同意」 |
| D27 | 11-10 | **借鉴「来源排除」机制** | 「借鉴「来源排除」，我同意」 |
| D28 | 11-10 | **自建 `FTcsTagRequirements`**（不依赖 GAS 模块）；`TagQuery` 不建 | 「TCS 不准备依赖 GAS 模组，所以要在 TCS 内部建立一个同样形状的结构体」+ 「TagQuery 位不建 —— 确认」 |
| D29 | 11-10 | 过滤字段名 **`ModifierTagRequirements`**；语义照搬 GAS | 「名字就用 ModifierTagRequirements」+ 「我觉得可以照搬 GAS 的语义」 |
| D30 | 11-10 | **现在建 `02-核心基线/GameplayTag.md`**（第六项基线），后续 State/Skill 裁决时扩充 | 「现在就创建文档，文档名就叫 GameplayTag，可以引用 V1 的裁决」 |
| D31 | 11-10 | **`InstigatorTags` / `TargetTags`（修正器条件生效）列入未来计划**（**已被 D33 取代**） | 「我倒是觉得 InstigatorTags（GAS原SourceTag）、TargetTags 也可以引入，只不过可以放到未来计划中」 |
| D32 | 11-10 | **`ETcsAttrReadScope` 整体删除**，由 `ETcsAttrReadPart`（`Total`/`Basic`/`Extra`）接替；`ETcsAttrReadSide` 保留 | 讨论中确认「段值级 scope」正是 `Part` 在做的事——两者是同一个东西的两种命名 |
| D33 | 11-11 | **`InstigatorTags` / `TargetTags` 当前不引入**；条件生效走 D11 的链路径。重启需同时满足三条进入条件（见 §15） | 「我接受你的提议」 |
| D34 | 11-11 | **`FTcsSourceHandle` 保持纯数字，不承载因果**（撤销锚点有 5 处 MUST 级读点，见下条；V1 的 `CausalityChain` 塞进句柄已被证伪） | 「SourceHandle 如果是个纯数字，真的可以吗」——实测后确认保持 |
| D34b | 11-11 | **因果链 / 溯源边：裁为「不建」**。判据：V1 落地了字段 + 两个读数口，至今**零逻辑读取方**（全仓 `CausedBy` 命中 4 处 = 1 定义 + 2 写入 + 1 读数口；`GetRunCausedBy` 零调用）⇒ 铁律 ① 明文禁止的预建 | 「暂时全部裁掉，就算后面要加，也不是不能重新裁决」 |
| D34c | 11-11 | **Explain 调试面板：裁为「不建」**。判据：V1 规格有定义、**无任何 Task 认领**，11 个模块无编辑器模块；实际顶用的是日志 + dump 命令 | 同上 |
| D35 | 11-11 | **「游戏结算统计」= 插件提供实现框架方案（事件 + 记录形状 + 读取路径），宿主利用它实现自己的结算统计**；**统计口径本身裁为「不建」**（宿主词汇） | 「插件提供实现框架方案，宿主项目利用框架实现自己的结算统计」 |

> **本节只记录「裁了什么」，不记录「怎么论证的」**——论证在 §11 之后各节与 `Attribute.md` 里。

---
## 12. 弗拉米尔案例：来源排除机制（D24–D29 的落地形态）

> **本节是把 D27–D29 写成可执行形态**，供参数域与属性域文档直接摘用。

### 12.1 问题

LoL 弗拉米尔被动【血色契约】：

```text
① 每 40 额外生命 → +1 法术强度
② 每 1 法术强度 → +1.4 额外生命
③ 该效果不会自我循环叠加
```

**①②互相引用 = 依赖环。** 按「环即失败」的原裁决，它根本配不出来。

### 12.2 两份外部参考

| 来源 | 做法 | 借鉴与否 |
|---|---|---|
| 用户提供的伪代码 | `layers = [BASE, FLAT, PERCENT, CONVERT_HP_TO_AP, CONVERT_AP_TO_HP]`，层序即依赖方向 | ❌ **不借鉴**（D26） |
| GAS | `EGameplayModEvaluationChannel`（Channel0–9）逐通道折叠（`GameplayEffectAggregator.cpp:263-283`） | ❌ **不借鉴**（同一个思路，同一个否决） |
| GAS | `FGameplayTagRequirements{RequireTags, IgnoreTags}`（`GameplayEffectTypes.h:1521-1548`） | ✅ **借鉴**（D27–D29） |

> **为什么否决层序**：它是「人工提前算好的拓扑序」。TCS 的 `EvalOrder` 每次从实际依赖边算，**能自动检环**；层序靠人保证层序与语义一致，配错即静默错值。

### 12.3 机制：修正器带标签，读取方按标签筛

```cpp
// 修正器携带标签（新增字段）
struct FTcsAttrModInstance
{
	TObjectPtr<UTcsAttrModDef> Def;
	FTcsParamValue             Operand;
	FTcsSourceHandle           Source;

	// 本条携带的标签（供读取方筛选；空 = 无标签，永不被 IgnoreTags 排除）
	FGameplayTagContainer      Tags;
};

// 读取源按标签要求筛选（新增字段）
struct FTcsParamSource_AttributeScaled
{
	FGameplayTag       Attribute;
	ETcsAttrReadSide   Side;
	double             Coefficient;

	// 只计入标签满足此要求的修正器。空 = 全计入（默认）
	FTcsTagRequirements ModifierTagRequirements;

	double             Fallback;
};
```

`FTcsTagRequirements` 的形态见 `02-核心基线/GameplayTag.md` §1（`RequireTags` + `IgnoreTags` + 两行判定）。

### 12.4 环判据：不是「有边即环」，而是「边的粒度」

**先说结论：标签排除本身不消环。它只决定「连哪几条边」。消环靠的是图节点降到修正器级。**

| # | 事实 | 效果 |
|---|---|---|
| ① | 依赖图的节点**不是属性**，是求值单元（`SegmentBase` / `Modifier` / `Segment` / `Total`） | 边可以精确到「某一条修正器」 |
| ② | 过滤读**跳过段节点**，直连「段基值节点 + 每条被计入的修正器节点」 | 被排除的修正器**不在依赖链上** |
| ③ | 标签排除只是 ② 里「挑哪些修正器」的手段 | 它决定**连哪几条边**，不决定「连不连」 |

**弗拉米尔的真实形态**（HP 上不止一条修正器）：

```text
HP 上的修正器（假设都在 Extra 层）：
  M1  装备 +100                Tags = {}
  M2  符文 +50                 Tags = {}
  M3  由 AP 转化 +k·AP         Tags = {Pact}

AP 上的修正器：
  M4  由 HP 额外生命转化       Tags = {Pact}
      读：HP 的 Extra 段，IgnoreTags = {Pact}
```

**M4 连哪些边**（`Attribute.md` §5.2 的过滤读分支）：

```text
连    SegmentBase(HP.Extra)   ← 段基值，无条件
连    M1                      ← Tags 空 ⇒ HasAny({Pact}) = false ⇒ 计入
连    M2                      ← 同上
不连  M3                      ← Tags = {Pact} ⇒ HasAny 命中 ⇒ 排除
```

**图**：

```text
SegmentBase(HP.Extra) ─┐
M1 ────────────────────┼─→ M4 ─→ Segment(AP.Extra) ─→ M3 ─→ Segment(HP.Extra) ─→ Total(HP)
M2 ────────────────────┘
```

**无环。** 关键在于 M4 **不连** `Segment(HP.Extra)`——若连了，`Segment(HP.Extra) → M3` 会把 M3 拽进 M4 的输入，而 M3 依赖 AP、AP 依赖 M4 ⇒ 环。

> **本节早先版本写错过一条**：原句是「HP 上只有一条修正器 ⇒ 全被排除 ⇒ 不建边 ⇒ 无环」。**错在两处**：
> ① HP 上真实还有装备、符文这些**无标签**修正器，它们不被 `IgnoreTags` 排除；
> ② 即使真的全被排除，得出「不建边」也是错的目标——那会让 M4 变成常量，读不到它该读的装备/符文加成。
> 正确的表述是：**排除只剔掉 M3 这一条，其余照连**。

> **对照：假如 M4 读总值（`Part = Total`）**——总值节点无法按修正器拆，M4 只能连 `Total(HP)`，而 `Total(HP)` 依赖 `Segment(HP.Extra)` 依赖 M3 ⇒ **环**。这正是「必须分读取粒度」的根本理由（`Attribute.md` §3.5）。

> **对照：假如过滤为空**——M4 连 `Segment(HP.Extra)` ⇒ 同样闭环。所以弗拉米尔的**两个配置项缺一不可**：`Part = Extra` **且** `ModifierTagRequirements.IgnoreTags = {Pact}`。

### 12.5 成本：不配过滤 = 零成本

过滤读与普通读走**同一张图**，区别只在**连几条边**：

| 源的形态 | 连边数 | 建图成本 | 求值成本 |
|---|---|---|---|
| `Part = Total`（默认） | 1（→ `Total` 节点） | — | O(1) |
| `Part = Basic/Extra`，过滤为空 | 1（→ `Segment` 节点） | — | O(1) |
| `Part = Basic/Extra`，过滤非空 | `1 + 该段被计入的修正器数` | +O(该段修正器数) | O(被计入数) |

> **代价在建图，不在每帧**：多出来的边只在 `RebuildGraph` 时付一次（结构变更才发生，`Attribute.md` §5.2）。求值侧多出的 `O(被计入数)` 只在本条修正器节点被重算时出现。

**过滤读的求值路径**（与普通读不同，必须写清）：

```cpp
// M4 这类节点被求值时：不能读 CachedCurrent，也不能读 Segment 节点（那是含 M3 的全量结果）
double Evaluate(Entity, Store, const FTcsParamSource_AttributeScaled& Self)
{
	const int32 Slot = Store.TagToSlot[Self.Attribute];

	if (Self.ModifierTagRequirements.IsEmpty())
	{
		// 无过滤：直接取对应节点已算好的值
		return Store.NodeValues[FindNode(Store, Slot, KindForPart(Self.Part))];
	}

	// 有过滤：现场重折叠——段基值 + 每条被计入修正器的**节点值**
	const ETcsAttrSegment Seg = SegmentForPart(Self.Part);
	TArray<FTcsAggregateEntry> Entries;
	for (int32 ModIndex = 0; ModIndex < Store.Instances[Slot].Mods.Num(); ++ModIndex)
	{
		const FTcsAttrModInstance& Mod = Store.Instances[Slot].Mods[ModIndex];
		if (Mod.Def->Layer != SegmentToLayer(Seg)) { continue; }
		if (!Self.ModifierTagRequirements.RequirementsMet(Mod.Tags)) { continue; }

		// 用节点值，不用 Mod.Evaluate()——节点值是本次结算已经算好的，再算一遍是重复
		Entries.Add({ Mod.Def->Op, Store.NodeValues[FindModifierNode(Store, Slot, ModIndex)],
			Mod.Def->Priority, Mod.Source });
	}

	const double Base = Store.NodeValues[FindSegmentBaseNode(Store, Slot, Seg)];
	return FoldSegmentEntries(Base, Entries);   // 《数值聚合器》§3 的段内折叠
}
```

> **它读的是节点值，不是 `CachedCurrent`**：所以与图完全一致——`NodeValues` 就是图节点的缓存，过滤读只是**换了一组输入**去折叠。这消解了 §13 R1 原来的担心。

> **不引入第二条缓存**：过滤读不存结果，它每次现算。因为它的结果**不属于任何一个节点**——它是「这一条修正器的输入」，而不是「某个属性的状态」。多算一次的量级是 O(被计入数)，且只在被依赖变化触发时发生。

### 12.6 三条语义边界（与 `GameplayTag.md` §1.2 一致）

| # | 情形 | 结果 |
|---|---|---|
| ① | 无标签的修正器 + `IgnoreTags` 非空 | **计入**（`HasAny(空集)` = false）← 弗拉米尔成立的前提 |
| ② | 无标签的修正器 + `RequireTags` 非空 | **被排除**（`HasAll(空集)` = false）← 危险配置 |
| ③ | 两个集合都空 | 恒通过（走快路径） |

> **② 建议在数据校验里加警告**：`RequireTags` 非空时提示「没有标签的修正器将全部被排除」。

---

## 13. 遗留点：逐条结案

> 本节原为「尚未裁决的遗留点」。2026-11-10 逐条复核后，**六条全部有结论**；结论不是「裁了」就是「问题本身不成立」。

| # | 原问题 | 结论 | 依据 |
|---|---|---|---|
| R1 | 带过滤的读取是「第二条读取路径」，与 `CachedCurrent` 的关系要不要收窄？ | ✅ **已消解，无需改 `Attribute.md` §2.1** | 过滤读读的是 **`NodeValues` 里的节点值**，不是另一份缓存（§12.5）。`CachedCurrent` 只是 `Total` 节点值的一个视图，定义不需要动 |
| R2 | `ETcsAttrReadScope` 最终几档？ | ✅ **已删除，问题消失** | D32：整个 `ETcsAttrReadScope` 取消，由 `ETcsAttrReadPart`（`Total`/`Basic`/`Extra`）接替。「第三档段值级 scope」正是 `Part` 做的事，两者本是同一个东西的两种命名 |
| R3 | `BaseOnly` 是否需要段参数？ | ✅ **同上，已删除** | 「读 Extra 段的基值」= `Part = Extra` + 读 `SegmentBase` 节点。`Attribute.md` §5.2 的过滤读分支无条件连 `FindSegmentBaseNode`，正是这个语义 |
| R4 | `ShouldBuildEdge` 在「过滤非空但暂无修正器」时的行为 | ✅ **问题不成立 + 挖出真缺陷并已修** | ① `ShouldBuildEdge` 已随槽位级建边一并删除，改为修正器级 `BuildModifierOutEdges`（`Attribute.md` §5.2）；② 真缺陷是 `BatchWrittenNodes` 存节点下标、`RebuildGraph` 后失效 ⇒ 已裁为「重建即丢弃、全量重算」（`CommitBatch`） |
| R5 | `Graph::Rebuild` 检出环后返回 `false`，`CommitBatch` 要不要中止？ | ✅ **不中止；原子性口径已写进 `Attribute.md`** | §5.4 兜底分支 + §7.1 返回值说明：`ApplyAttributeSet` 的 `true` 只承诺「改动已应用、值已结算」，**不含「图无环」** |
| R6 | `FTcsTagRequirements` 的「≥2 域依赖」不满足 | ✅ **已满足，非勉强论证** | `GameplayTag.md` §2：Attribute（已落地）+ State/Buff/Skill 三域类目筛选（`FTcsStateDefBase.CategoryTags` / `FTcsSkillEntrySelector.ByCategoryTags` / 关系表族规则）。**消费者是四个，不是靠「工具类型豁免」** |

**本轮新发现的一条（R7）**：

| # | 问题 | 处置 |
|---|---|---|
| R7 | `Attribute.md` §3.4 的 `Evaluate` 仍是**按实例整体求值**的旧形态（`CollectEntries` 遍历 `Inst.Mods` 全量、`EvaluateAggregatedValue` 一次算完），而 §5.3 已改成**按节点求值**（`EvaluateNode` 分四种 `Kind`）。两处对同一件事有两套实现 | ✅ **已修**：§3.4 重写为「只讲怎么交给聚合器，不讲什么时候算」——`CollectLayerEntries` 只读节点值、`FoldSegment`/`EvaluateTotal` 从 `Segment` 节点取值，并新增「缓存与节点值的关系」表（`CachedSegment`/`CachedCurrent` 是 `NodeValues` 的具名视图，不是第二份缓存） |

---
## 15. 修正器条件生效（`InstigatorTags` / `TargetTags`）：当前不引入

> **对应裁决 D33（2026-11-11，「我接受你的提议」）**。原 D31 的「列入未来计划」已作废。

### 15.1 用户提出的问题

> 是否应该引入 GAS 的 Modifier 配置中的 `SourceTags` 和 `TargetTags`？

GAS 把它们放在 `FGameplayModifierInfo` 上（`GameplayEffect.h:584-594`）：

```cpp
UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
FGameplayTagRequirements  SourceTags;

UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
FGameplayTagRequirements  TargetTags;
```

语义：「施加者 / 目标身上的标签满足这个要求，这条修正器才计入」。

### 15.2 GAS 实证：这套机制本身不是好样板

| 路径 | 判定时机 | 查哪个标签集 | 证据 |
|---|---|---|---|
| **Duration**（有聚合器） | 每次 `Evaluate` 全量重扫 | `GetAggregatedTags()` = ActorTags **+ SpecTags** | `GameplayEffectAggregator.cpp:391-395` |
| **Instant / Periodic** | 施加时逐条 `continue` | `GetActorTags()` = **只有** ActorTags | `GameplayEffect.cpp:3113-3125` |

两条路径查的**不是同一组标签**。

而且 Instant 那条的检查是**后补的补丁**，源码自己写着：

```cpp
// GameplayEffect.cpp:100-101
bool bUseModifierTagRequirementsOnAllGameplayEffects = true;
FAutoConsoleVariableRef CVarUseModTagReqsOnAllGE{ TEXT("AbilitySystem.Fix.UseModTagReqsOnAllGE"),
	bUseModifierTagRequirementsOnAllGameplayEffects,
	TEXT("Fix an issue where MustHave/MustNotHave tags did not apply to Instant and Periodic Gameplay Effects"),
	ECVF_Default };
```

> 「**Fix an issue where … tags did not apply to Instant and Periodic Gameplay Effects**」——它长期不生效，后来才补上，而且补丁还留在 Cvar 后面。**这直接证明两条路径的分裂是历史债，不是设计。**

**标签在施加瞬间捕获**（`CapturedSourceTags`），之后不跟随变化。Duration 路径虽然每次 `Evaluate` 重扫标签要求，但重扫用的是**同一份捕获结果**——源身上的标签后来改了，这里不会知道。

> **结论**：这是历史演进留下的双路径，不是设计意图。**照搬会把缺陷一起带进来。**

### 15.3 TCS 现在缺前置条件

`SourceTags` / `TargetTags` 要查的是「**实体身上现在有哪些标签**」。

> **TCS V2 没有「实体标签容器」这个概念。** `02-核心基线/战斗实体.md` 全文 `Tag` 出现 **0 次**。

V2 现有三个标签机制，**方向都不同**：

| 机制 | 方向 | 位置 |
|---|---|---|
| `FTcsAttrModInstance::Tags` | 修正器**自报**「我是什么」 | `Attribute.md` §7.2 |
| `ModifierTagRequirements` | 读取方**筛选**「计入哪些修正器」 | `Attribute.md` §5.2 |
| **GAS `SourceTags`/`TargetTags`** | 修正器**要求**「别人有什么」 | ← **方向相反** |

引入后 `FTcsAttrModInstance` 上会同时出现三个标签字段（我携带的 / 施加者必须有的 / 目标必须有的）。**配置者要分清三者的方向，这本身就是设计债。**

### 15.4 三条不引入的理由

#### ① 已有正交路径（D11）

「目标有【虚弱】⇒ 攻击力 +20%」这类需求，链已经能表达：

```text
WaitEvent(目标获得【虚弱】)
Branch(判定是不是我该反应的)
ApplyState(攻击力 +20% 的 Buff)
```

把条件塞进修正器字段，等于**在两条路上重复实现同一个能力**——而且链那条路径更通用（它能表达「进入时触发一次」「离开时撤销」「有前置动作」），修正器字段那条只能表达「当前时刻成不成立」。

#### ② 前置机制不在本域，属于倒置归属

「实体标签容器」属于 **State 域**（「实体身上在跑哪些状态」的词集），而 State 域还没开始裁。

现在设计「修正器怎么查它」，就是让 Parameter 域规定 State 域必须长出什么形状——**和你纠正过我的那条是同一个错误，只是方向反了**：

> 「参数表虽然是被 StateDef 引用，但完全可以在 Parameter 域内确定完所有逻辑后，再被 StateDef 引用」

那次是「被引用者不该等引用者」，反过来说就是「**本域不该替另一个域设计它的机制**」。

#### ③ 违反铁律 ②：预建插槽

今天找不到「同一条 Buff 内多条修正器按条件择一」的真实配置。**没有消费者就先建字段**，正是铁律 ② 禁止的。

### 15.5 三条进入条件（全部满足才重启）

| # | 条件 | 为什么是它 |
|---|---|---|
| ① | **State 域确实产出了「实体标签容器」这一机制** | 没有它，本机制无处查标签——前置依赖必须先落地 |
| ② | **出现「同一条 Buff 内多条修正器按条件择一」的真实配置需求** | 这是链路径**唯一表达不了**的场景（链作用于修正器的**存在与否**，表达不了「同一条修正器在不同条件下取不同值或不计入」） |
| ③ | **证明「拆成多条 Buff」在本项目实际配置量下不可接受** | 若拆 Buff 能解决且配置量可控，就不该为省事引入第二套条件机制 |

> **②与③是一条完整判据的两半**：②说「链做不到」，③说「也没有更土的替代做法」。两条都不满足时引入，就是为一个能绕过去的问题增加长期维护面。

### 15.6 若将来引入，命名 MUST 避开歧义

`InstigatorTags` 这个名字没问题（GAS 的 `Source` 在 TCS 会与 `FTcsSourceHandle` 撞词）。

但 **`TargetTags` 在 TCS 里是有歧义的**——§2 已记录：

> `Target` 在伤害流程里指「被命中的敌人」，在 Buff 里指「被加成的友军」——两者可以不是同一个实体（治疗技能：施法者 A、受害 B、治疗目标 C）

GAS 没这个问题，因为它的 GE 只有一个 target。TCS 的伤害流与 Buff 流共用一套词汇，所以必须写清是哪一个：

| 候选名 | 指向 |
|---|---|
| `OwnerTags` | **修正器宿主**的标签（Buff 挂在谁身上就查谁） |
| `DamageTargetTags` | **伤害目标**的标签 |

> **MUST NOT 叫 `TargetTags`**：它同时可以指上面两个，配置者无法从名字判断。

### 15.7 连带：`FTcsTagRequirements` 的消费者数量

> **本裁决不影响 `GameplayTag.md` 的归属**。

`FTcsTagRequirements` 的消费者已有四个（Attribute 的 `ModifierTagRequirements` + State/Buff/Skill 三域类目筛选，`GameplayTag.md` §2），本裁决只是不给它添第五个。

---
## 调研方法与边界

- 扫描范围：`Source/` 全量 11 模块的头文件与 `.cpp`，`Documents/combat-system-design/` 的决策与规格档。
- `Content/` 下 **0 个 `.uasset`** ⇒ 无存量资产需迁移，「改名成本为零」有实证支撑。
- **⚠️ 「零实现者」判法已证伪一次**：原判法「grep `public I<接口名>`，命中 0 即认定」**只能扫 C++**。`ITcsParamSourceHost` 的 C# 夹具 `UTcsDevGcParamSourceHost` 就扫不到（§8.3）。此后凡此结论，MUST 说明**扫了什么**。
- **⚠️ 本机工具局限（2026-11-10 实测）**：`read` 不带 `limit` 时**最多返回约 667 行且静默截断**——用它的结果做「全文重写」会**丢文件尾部**。本轮补写时曾因此两次损坏本文件。此后改写长文件 MUST **分块读取（≤300 行/次）再拼接**。
- 本轮未编译、未运行，全部结论来自静态阅读。
