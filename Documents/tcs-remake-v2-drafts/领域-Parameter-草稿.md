# 领域草稿 · Parameter（参数域）

> **族外草稿，不是实现依据。** 参数域正式文档尚未开始；本文只收录 **Attribute 域裁决过程中浮现的、属于参数域的待裁点**。未列入本文的不算已裁，已裁的也不等于已进参数域文档。

---

## 1. 值来源族（V1 现状，V2 待复核）

V1 的 `FTcsParamValue { FInstancedStruct Source }` 已有一组源（机制 12 复盘）：

| 源 | 语义 | V2 待定 |
|---|---|---|
| `FTcsParamSource_Literal` | 配置写死的数 | 保留 |
| `FTcsParamSource_StateLevelArray` | 按状态等级查表 | 保留 |
| `FTcsParamSource_InstigatorLevelArray` | 按施加者等级查表 | 保留 |
| `FTcsParamSource_AttributeScaled` | 系数 × 另一属性当前值 | **需重做**（见 §2） |

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

---

## 3. 多属性组合（待裁）

**背景**：GAS 用 `UGameplayEffectExecutionCalculation` 支持「一个值由多个属性组合」；TCS 的聚合器**无代码插点**（§2.1「计算在上游求值并传入终值」），所以组合能力必须落在参数域。

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

---

## 4. 与 Attribute 域的接口（已由 Attribute 域裁决确定）

- Attribute 域的 `FTcsAttrModInstance` 持有 `FTcsParamValue`（值载体），`AttrModDef` **不持有**（保泛用性）。
- 参数源通过 `FTcsParamEvalContext` 拿**属性读口接口**（不是 `UTcsAttributeSubsystem` 实体），解开两域互相依赖。
- 求值上下文带 `bSnapshot`：`true` 时 `AttributeScaled` **MUST NOT** 登记依赖边（快照不跟随）。

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

---

## 6. `Live` / `Snapshot` 的归属 ✅ 已裁决 = 归参数域（方案 A）

**触发（2026-11-06 用户质询）**：用户追问「按你的推论，`ParamSource` 直接在运行时使用，意味着参数在运行时不会被修改或更新，即便 `StateLevelArray` 这一类也不会吗」。

**该质询揭出的真问题**：我原来的表述只覆盖了**求值入口**（源是不可变纯函数，两态同一份），漏掉了**求值时机与结果的冻结**——那是参数域的另一半语义。

**V1 的实情（代码证据）**：

| 事实 | 证据 |
|---|---|
| 源对象本身不可变 | 四个等级源的 `Evaluate` 全 const；`Values` 是 `EditAnywhere, BlueprintReadOnly`（`TcsParamSource_StateLevel.h:34-35, :68`） |
| 参数值在运行时**有状态** | `FTcsParamSnapshot`（`TcsStateSnapshot.h:47-63`）：施加瞬间求值冻结，实例生命周期内不重算 |
| 冻结语义归参数域 | `FTcsStateSnapshot.h:60-61` 原话：「实现名取设计名 `FTcsParamSnapshot`——它是**参数域**的概念（修正器物化、技能侧参数行都要读它），不是状态模块专属」 |
| 模式分流的唯一实现 | `ETcsParamMode`（`TcsParamRow.h:17-21`），消费者只有 `TcsCastOps_Snapshot.cpp` |
| **修正器路径不按 Mode 分流** | 该文件注释：「`EPM_Live` / `EPM_Snapshot` 在本路径上同解：两者都走实时折叠（快照只影响快照面）」 |
| 快照里预留过 Live 位 | `TcsStateSnapshot.h:38-39`：`FInstancedStruct SourceRef`——「Debug 可见取值来源 / **Live 化预留**；无消费者」 |

**V1 没有 Curve 版等级源**：全仓 `Source/` 扫 `Curve` = 0 命中。只有 `{State,Instigator}Level{Array,Map}` 四种。

**用户裁决（2026-11-06）**：**方案 A —— 快照机制完全归参数域**。

**落地要求（参数域文档必须承担）**：

| # | 事项 |
|---|---|
| 1 | `ETcsParamMode` 的定义与语义住参数域（`TcsCore/Public/Parameter/`），值形 `{ Snapshot, Live }`（V2 无前缀风格） |
| 2 | 「此刻该不该求值、求完要不要冻」的判定归参数域 |
| 3 | 冻结值的**存储**归持有者（属性实例的 `SnapshotValue`、状态的 `FTcsParamSnapshot`），参数域只提供语义 |
| 4 | 参数域默认 `Snapshot`；属性域默认 `Live`（修正器本来就实时折叠，见上表第 5 行） |
| 5 | 求值上下文里的 `bSnapshot` 位（控制 `AttributeScaled` 要不要登记依赖边）同归参数域 |

**已同步到 Attribute 域**：`04-领域/Attribute.md` §7.2 的 `ETcsValueMode ValueMode` 已删除，改用 `ETcsParamMode Mode`；§7.3 改写为「模式归参数域，冻结位归本域」并补分工边界表与 Why Not。

> **关于 P2 的现状形状**：族内现有两套规律——基类驼峰无下划线（`FTcsParamValueSource` / `FTcsParamEnumerableSource`）、具体类下划线（`FTcsParamSource_*`）。所以「派生名不含基类名」本身不算硬伤；P2 真正裁的是**要不要让基类名自解释**。

> **改名成本为零**：V1 代码与资产整体删除（`03-词汇与命名.md` §5），⇒ P2/P3 的取舍只看「新名是否更准」，不看「改动波及多大」。

---

## 7. 参数域也有自己的修改器 —— 会回头影响 Attribute 域（2026-11-06 用户提出）

**用户原话意旨**：「Parameter 也会有属于自己的修改器（比如 Skill 模块的技能修改器），所以可能会影响到 Attribute 域，不过最终应该会在 Parameter 域裁定完再说」。

**已确认的事实**：

| 东西 | 改的是什么 | 住哪 |
|---|---|---|
| `FTcsAttrModInstance`（属性域） | 属性的值 | `Attribute.md` §7.2 |
| `FTcsNumericParamModifier` / `FBoolSwitchModifier`（V1 技能侧） | **参数的值** | V1 `TcsSkill/Public/Skill/TcsNumericParamModifier.h` |

**真正的耦合点 = 折叠器归谁**。V1 明文写着（`SPEC-05-skill` D5-5 v3）：

> 「**折叠器单份住 TcsAttribute**，M2 属性聚合 / M5 参数链 / TcsDamage 流程属性三处共用」

V1 的形态是：属性域提供折叠器，参数域与伤害域**借用**它。V2 若要延续，参数域就得依赖属性域；若不延续，折叠逻辑会有两份。

**必须先想清楚的三问**：

1. **参数修改器与属性修改器的带式折叠是不是同一套？** V1 说「与 M2 完全同式、顺序无关」（同五带 `Override / Add / PercentAdd / Mul / FlatAdd`、同带权）。若是同一套，折叠器不该由任何一个域私有。
2. **V2 的折叠器住哪？** 三个候选：① 继续住属性域（参数域反向依赖属性域）；② 上移到核心基线（两者都依赖它）；③ 参数域提供（属性域反向依赖参数域）。**§3 聚合接线与 §7.2 修正器都直接受此影响。**
3. **参数修改器的「作用域」与属性修改器的「落点」是不是同一个概念？** 属性侧有 `ETcsAttrLayer {Basic, Extra, Global}`；参数侧的 `CompeteGroup` / `Source` 撤销级联是另一套。两者形状相似但语义不同，MUST NOT 强行统一成一个枚举（同第 57 项 `ETcsValueMode` 的教训）。

**与 `Attribute.md` 的接触面**：

| 接触点 | 影响 |
|---|---|
| §3 聚合接线（对接《数值聚合器》） | 若折叠器上移或迁走，§3.4 的收集/求值要跟着改 |
| §7.2 `FTcsAttrModInstance::Operand` | 若参数域要消费属性修正器的折叠结果，`Operand` 可能不再是 `FTcsParamValue` 那么简单 |
| §9.2「它不做什么」 | 现在写着「折叠公式 → 《数值聚合器》」，折叠器搬家后这句的指向要改 |

> **不预建裁决**：本节的用途只是**登记耦合点**，参数域文档开工时必须先回答三问，再动 `Attribute.md` 的对应章节。

---

## 未决

1. §1 的源族是否全保留（尤其两个 LevelArray 源）。
2. §2 的枚举命名与归属（`ETcsAttrReadSide` 住参数域还是属性域）。
3. §3 的 `Composite` 运算集（Add / Mul / Max / Min 够不够）。
4. §5 的 P2 / P3 两条命名裁决（参数域文档开篇必须先裁）。
5. §6 的落地要求五条要写进参数域文档（`ETcsParamMode` 定义位置、判定归属、存储归属、两侧默认值、`bSnapshot` 位）。
6. §7 的三问（折叠器归谁、两套带式折叠是否同一套、作用域概念是否统一）——**它决定 `Attribute.md` §3 与 §7.2 要不要改**。
