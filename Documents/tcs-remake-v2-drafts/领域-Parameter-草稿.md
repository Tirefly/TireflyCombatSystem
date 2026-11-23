# 领域草稿 · Parameter（参数域）

> **族外草稿，不是实现依据。** 参数域正式文档尚未开始。
>
> **2026-11-17 起从零重做。** 用户原话（m02148）：「我现在像完全刨除 V1 的原有内容和代码，不再做任何参考，从零和你探讨 Parameter 的设计。」
>
> **旧草稿已整体归档，2026-11-23 按用户裁决从工作区移除。**那份文件里有 **D1–D50 裁决流水（含用户原话）** 与**全部 `文件:行号` 实证**，两者都是**记录**而非设计，仍然有效；需要核对原文时按 `git show HEAD:Documents/tcs-remake-v2-drafts/领域-Parameter-草稿-归档-20261113.md` 取。但它的**设计段落**（§6 的 `Live`/`Snapshot` 归属、§8.8 冻结机制、§9 架构三选一、§10 确认清单）**不再是 V2 的依据**。

---

## 0. 本文的写法约定

| 标记 | 含义 | 效力 |
|---|---|---|
| **【已裁】** | 用户明确裁过，后附会话编号 | **不许重开**；要改 MUST 先说明依据变了 |
| **【推导】** | 从已裁项推出来的结论 | 用户确认前**不算已裁**，可被推翻 |
| **【未裁】** | 摆在那里等用户裁的问题 | 本文只列不答 |

**一条硬规矩**：本文 MUST NOT 用「现有代码是这么写的」充当任何结论的理由。现有代码只是**要被替换的东西**，它可以证明「这里有问题」，不能证明「那里该长什么样」。

---

## 1. 问题：那道落差

配置里写的是**「这个数怎么算出来」**；运行时真正要的是一个 `double`。

```text
配置侧                                    运行侧
"伤害 = 基础值 × 倍率"      ──落差──►      150.0
（一个描述）                                （一个数）
```

落差里包着三段，全都要有人管：

1. **算什么**：那个描述本身（基础值、倍率、怎么合）。
2. **什么时候算**：这一次出生算一次，还是每次读都重算。
3. **算完存哪**：算出来的 `double` 放在谁身上、活多久。

**Parameter 域就是为管这三段而存在的。**

---

## 2. 四块范围

**【已裁 甲｜m02166】用户原话：「基本就是甲」** —— 参数域范围 = 四块全要。

| # | 块 | 是不是有状态 | 管什么 |
|---|---|---|---|
| ① | **表达式** | 无状态（纯函数） | 一个数怎么算出来 |
| ② | **环境** | 无状态（一次调用的入参） | 算的时候能看见什么 |
| ③ | **时机** | **有状态** | 什么时候算、算一次还是每次算 |
| ④ | **命名值账本** | **有状态** | 算出来的数存在哪、被谁改、活多久 |

### 2.1 这一块最关键的分界

> **①② 是函数，③④ 是生命周期。它们不是同一件事。**

判据很硬：此前几轮所有卡点——「门」「出生求值」「冻结」「多 run 并存时读哪一发」——**全部落在 ③④，一个都不在 ①②**。

### 2.2 与核心基线判据的关系

`INDEX.md` §2.2 原文：「一个设施**不含任何领域词汇**、**被 ≥2 个领域依赖** ⇒ **核心基线**。含领域词汇 ⇒ 属于那个领域。」

- ①② **不含**领域词汇 ⇒ 它们**可以**作为内部设施住在核心基线里。
- ③④ **含**领域词汇（宿主、实例、生命周期、来源）⇒ 它们定义「Parameter 是一个域」。

⇒ **Parameter 要缩，不是要没**：缩小到只保留 ③④ 的领域性，①② 的实现可以下沉到基线，但**语义归属仍是 Parameter**。

---

## 3. 两对象劈开

**【已裁 m02176】** 问题原先是「SkillEntry 创建时持有 ParamInstance，还是 SkillInstance 才持有」。那个二选一**偏窄**——正确答案是**两个时刻各建不同的东西**：

| 对象 | 挂在哪 | 活多久 | 里装什么 | 可写吗 |
|---|---|---|---|---|
| **`FTcsParamState`** | 技能的「学会条目」 | 跨施法 | **修正器**集合 | 可增可删 |
| **`FTcsParamBorn`** | 一次施法实例 | 一次施法 | **算好的 `double`** 切片 | 出生时写，此后只读 |

### 3.1 为什么必须劈开（三条反证）

| 方案 | 怎么坏 |
|---|---|
| **只挂 Instance** | buff 施加时没有 Instance 可写；这一发放完就销毁，第二发拿不到；而 buff 是跨施法的 |
| **只挂 Entry** | 引导技能两发并存：A 发读到 120、B 发读到 100，Entry 只有一格 ⇒ **总有一发读不到自己出生时刻的值** |
| **混在一个对象** | 必须靠标志区分「同一个 Key 这次是哪种」；N 个键 × M 发并存 ⇒ **又回到「一个 Mode 位」**，只是把宿主从 Def 挪到了账本 |

**劈开后的好处是一个结构性好处**：「冻没冻」不需要任何标志——**查得到就是冻的**。信息由「它在哪个表里」天然表达，不靠位、不靠注释约定。

### 3.2 可推广的规则

> **可修改的参数状态，挂「该域最长的那个参数生命周期」；出生切片，挂「每一次读值的上下文」。**

| 域 | 最长生命周期 | 一次读值的上下文 | 结论 |
|---|---|---|---|
| **Skill** | 学会（Entry） | 一次施法（Instance） | **两个对象** |
| **State / Buff** | 那次效果本身 | 那次效果本身 | **同一个对象**（层数为 1 的退化解） |
| **Area** | 那一次放置 | 那一次放置 | **同一个对象**（同上退化解） |

⚠️ 退化解**不是另一个方案**：Buff 没有「学会」概念，施加即建档，所以「最长的参数生命周期」与「读值上下文」是同一个东西。劈开规则在有层差的域上表现为两个对象，在无层差的域上塌成一个。

⚠️ **Area 行只记参数域对它要满足的形态，不裁它归哪个域。** Area 在本文与族内全库**零命中**——它是 V2 新概念，归属未定。这一行要说明的只有一件事：**Area 的读值上下文是「那一次放置」，与施法实例不是同一个生命周期**（Area 生成后施法可以结束、可以被打断，而 Area 仍在跳）。⇒ Area 的参数 **MUST NOT** 引用施法实例的出生切片，跨界只能在生成 Area 的那一刻把值取出来。

### 3.3 形态

```cpp
// 修正器：一条「谁 把 哪个键 怎么改」
struct FTcsParamMod
{
    FGameplayTag     Key;
    ETcsAggregateOp  Op;              // 族内已裁的七带枚举，见《数值聚合器》§3
    double           Value    = 0.0;
    FTcsSourceHandle Source;          // 摘除锚点，成对性判据：不存在摘不掉的条目
    int32            Priority = 0;    // 仅覆盖带 + 仅 HighestPriority 策略时读
    uint64           OrderSeq = 0;    // 施加序，落账时取 Source.Id
};

// 住 Entry，跨施法。只增删修正器，**不存值**。
struct FTcsParamState
{
    TArray<FTcsParamMod>      Mods;         // 数值修正器
    TArray<FTcsBoolSwitchMod> SwitchMods;   // 布尔覆盖条目，见 §3.4

    void  Add(const FTcsParamMod& M);
    int32 RemoveBySource(FTcsSourceHandle S);   // 契约：MUST 同时扫上面两个数组
};

// 住 Instance，一次施法。出生瞬间的切片，此后只读。
struct FTcsParamBorn
{
    TMap<FGameplayTag, double> Values;      // 数值切片
    TMap<FGameplayTag, bool>   Switches;    // 布尔切片（§3.4、§4 的 ResolveSwitch）
};
```

> **注一**：`FTcsParamState` 里**没有** `TMap<Key,double> Values`。值不住那里——值的产生是 ③ 时机的事，不是账本的事。把两者塞进同一个类型，会得到一个「有一个字段永远是空的」结构体。

> **注二**：条目形状照**族内已裁**的 `FTcsAggregateEntry`（《数值聚合器》§3）——`Op` / `Value` / `Priority` / `OrderSeq` 四列，**没有第三个排序键**（定序靠 `(OrderSeq, Value)`）。参数域若自造一个 `SortKey`，折叠器就得吃两种条目、或者写第二份排序——两条都违反《数值聚合器》§2.3「单一实现」。

### 3.3.1 按来源摘除：六条契约

**【已裁 R29｜m04949】** 用户原话：「这几条我都审核过了，完全没什么问题」。

`RemoveBySource` 是账本的**唯一撤销入口**。撤销的最小单元是「**一次施加**」，不是一个键。

**为什么条目必须带 `Source`**：账面里三条 `{Key = DamageRate, Op = Add, Value = +0.3}` 可能来自三个不同来源。按 `Key` 撤会连坐，按 `Value` 撤会误伤同值的那一条，**唯一可辨的一列是「这一次施加是谁」**。

**为什么是「这一次施加」而不是「这个 buff」**：同一个 buff 叠两层是**两次**施加，要能分别撤。族内《运行时实例》那套「一柄两角」说的就是这个——当句柄表示「谁施加的」时，它 MUST NOT 当撤销锚点；撤销认的是**这一次施加**产生的编号。

```cpp
void FTcsParamState::Add(const FTcsParamMod& M)
{
    checkf(M.Source.IsValid(), TEXT("账本条目必须带来源：没有来源就摘不掉"));
    FTcsParamMod Copy = M;
    Copy.OrderSeq = M.Source.Id;      // 覆写：施加序只能由发号器给
    Mods.Add(Copy);
}

int32 FTcsParamState::RemoveBySource(FTcsSourceHandle S)
{
    if (!S.IsValid()) { return 0; }
    const uint64 Id = S.Id;
    int32 Removed = Mods.RemoveAll      ([Id](const FTcsParamMod&      M) { return M.Source.Id == Id; });
    Removed      += SwitchMods.RemoveAll([Id](const FTcsBoolSwitchMod& M) { return M.Source.Id == Id; });
    return Removed;
}
```

**六条契约**：

| # | 契约 | 为什么 |
|---|---|---|
| ① | **只做全摘，不提供「单 Key 撤销」** | 撤销的粒度由「一次施加」定义；「这次施加里只撤某一个键」没有消费者（铁律 1） |
| ② | 返回**摘掉的条目数**；摘到 `0` **不报错** | 重复撤销是正常路径：卸装备撤一次、角色死亡再撤一次 |
| ③ | **配对责任在施加方**；参数域不追踪来源存活 | 与属性域 `DetachMod` 同一纪律：谁 `Attach` 谁 `Detach`。漏调 = 条目留在账本里，**静默残留** |
| ④ | `Add` 两条不变式：覆写 `OrderSeq = Source.Id`；`Source` 无效直接拒绝 | `OrderSeq` 是「施加序」，只能由发号器给，调用方填的必然是错的 |
| ⑤ | **撤销动不了出生切片** | `Bind == Born` 的键值在出生时已抄进 `FTcsParamBorn`；撤账本只影响此后出生的实例 |
| ⑥ | 一次施加写多份账本时，**施加方逐份撤**；参数域不建引用计数 | 参数域不知道也不该知道「这次施加还写了哪些账本」 |

**③ 与 ⑥ 是同一个取舍的两面**：参数域不跟踪「施加方还活着吗」。换来的是它不需要认识施加方是什么——装备、buff、技能、天赋都不必在参数域登记；代价是漏调**静默残留**，不报错。要发现漏调，靠施加方自己的生命周期纪律（开发期可以在撤销处断言「返回 > 0」），参数域**不提供**超时清理——那需要它知道各来源的生命周期，正是 ⑥ 拒绝的那件事。

**⑤ 常引起一个疑问**：「buff 加了 `+0.3`，我撤了账本，为什么这一发的伤害没变？」——因为它已经被出生冻结了。这不是没撤掉，是撤销的作用域到此为止：要影响那一发只能重算它，而重算不存在（§4.1）；等下一发。

**`RemoveBySource` MUST 同时扫两个数组**：布尔条目（§3.4）有完全相同的残留问题——一个 buff 关掉某个开关，撤销时若不扫 `SwitchMods`，开关会永远停在被覆盖的状态上，而且不报错。

**走查**（`Tag_TcsParam_CooldownReduction` 基础 `-2`，`Bind = Live`）：

| 时刻 | 事件 | 读到的值 |
|---|---|---|
| T0 | 学会技能 A | `-2` |
| T1 | 天赋（`Source = S1`）`Add(-2)` | `-4` |
| T2 | 装备（`Source = S2`）`Add(-3)` | `-7` |
| T3 | 施法，读到 `-7`，交给 SkillModifier 给 B 减 7 秒 | `-7` |
| T4 | 卸装备 ⇒ `RemoveBySource(S2)` 返回 `1` | `-4` |
| T5 | 天赋被移除 ⇒ `RemoveBySource(S1)` 返回 `1` | `-2` |
| T6 | 再撤一次（重复）⇒ 返回 `0`，不报错 | `-2` |

### 3.4 布尔开关：独立代数，只记覆盖

**【已裁 m01574】** 用户原话：「门管不管 布尔参数？**管的，必须要管的**。」

**【已裁 m02494】** 布尔的代数与写入通道：

| # | 裁决 | 用户原话 / 确认方式 |
|---|---|---|
| B1 | 布尔**独立代数**，账本里**只有覆盖**，不复用 `Add`/`PercentAdd`/`Mul`/`FlatAdd` | 「参考乙，但是冲突解决方案我觉得还是用多重方案比较好，就像运算符 `Override` 一样，有 Newest、Oldest、Priority 等方案」 |
| B2 | 策略集合 = **三值**：`LatestApplied` / `EarliestApplied` / `HighestPriority`，**不提供「任一为真 = 真」（OR）这类语义** | 选「丙：只三值，不提供 OR/AND」 |
| B3 | 策略**住开关行**（每行一列），不住单条修正器 | 选「甲：每个开关行自己配一列」 |
| B4 | `HighestPriority` 打平时，**补第二级「施加序较新者胜」** | 选「甲：Priority 大者胜，打平比注册序」 |
| B5 | **写入通道现在建**，与数值对称 | 选「甲：现在建，与数值对称」 |

```cpp
// 布尔账本条目：只有覆盖，没有累加
struct FTcsBoolSwitchMod
{
    FGameplayTag     Key;
    bool             Value    = false;
    FTcsSourceHandle Source;          // 摘除锚点，与数值条目同一套成对性判据
    int32            Priority = 0;    // 仅 HighestPriority 策略时读
    uint64           OrderSeq = 0;    // 施加序，落账时取 Source.Id
};

// 开关行上的一列（B3）：策略是**作者意图**，与开关同住
enum class ETcsBoolTieStrategy : uint8
{
    LatestApplied,    // 施加序最大者胜
    EarliestApplied,  // 施加序最小者胜
    HighestPriority,  // Priority 最大者胜，打平比 OrderSeq 较新者胜
};
```

**为什么 B4 必须补第二级**（这是 N4 里唯一不能省的一条）：数值域能用「有符号值大者胜」收尾（《数值聚合器》§3.2 `IsStronger` 的第 254 行决胜级），因为数值有大小。**布尔只有两个值，打平就真的打平了** ⇒ 两条覆盖同 `Priority` 而一 `true` 一 `false` 时，赢家会取决于数组遍历顺序。补 `OrderSeq` 才让它成为全序，结果与遍历顺序无关。

**`OrderSeq` 从哪来**：来源句柄的 `Id`。族内《数值聚合器》§3.2 表里「参数：`Handle.Id`」那一行已经定了这个口径——发号器是**进程唯一、原子递增**的，`OrderSeq` 直接取它即可，**不需要另建发号器**。（该表引用的具体实现已随 V1 代码一并删除，原文见 `git show HEAD:Documents/tcs-remake-v2-drafts/证据快照-V1引用出处.md` §2.4。）

**为什么不做 OR / AND**：`true` 与 `false` 之间没有 `Add` 那样的代数——「两个来源都说不许，叠加起来等于什么」没有自然答案。用户裁三值策略，等价于说：**布尔上只允许「谁说了算」，不允许「多个来源共同决定」**。

| 策略 | 打平可能吗 | 谁赢 |
|---|---|---|
| `LatestApplied` | 否（每条 `OrderSeq` 不同） | 号大者 |
| `EarliestApplied` | 否 | 号小者 |
| `HighestPriority` | **是** | `Priority` 大者；打平比 `OrderSeq`，号大者胜 |

**开关行的形状**（与数值参数行同构，只是没有表达式）：

```cpp
struct FTcsBoolSwitchDef
{
    FGameplayTag        Key;
    bool                Base     = false;                          // 无任何覆盖时的值
    ETcsBoolTieStrategy Strategy = ETcsBoolTieStrategy::LatestApplied;
    ETcsParamBind       Bind     = ETcsParamBind::Born;            // 与数值参数同一个枚举、同一个默认值
};
```

**`Bind` 同样管布尔**：`Born` ⇒ 出生那一刻按上表裁决一次、结果进切片；`Live` ⇒ 每次读按上表重算一次。布尔与数值走**同一个 `FTcsParamResolver`**，只是折叠函数不同（数值七带、布尔据上表选赢家）。

**布尔没有「假跟随」问题**（对照 §7.3）：布尔的基数是行上的 `Base` 常量，不引用别的键 ⇒ 引用闭包里不可能出现 `Born` 键 ⇒ §7 那一整节的不变量对布尔不适用。

### 3.5 基数从哪来

**【已裁 甲｜m03179】** 用户原话：「Q1：甲」。该裁决的实质是：**键不必在参数表里有一行，`FoldNumeric(Key, Base)` 的 `Base` 是形参**。

`Base` 不从 `Defs` 查，这带来两种合法用法，它们共用同一份折叠与同一份账本：

| 用法 | 基数从哪来 | 例子 |
|---|---|---|
| **参数行** | `Defs` 里同键那一行的表达式 | 伤害加成、射程加成 |
| **运行时基数** | 调用方当场算出来的一个 `double` | 这一跳的原始伤害、这一发的原始护盾 |

**这不是新增能力，是 `Base` 本来就是形参的直接后果**——本文从未把折叠器定义成「只能折参数行上的键」。把它写成一条，是因为它决定了**键的来源不限于参数表**：

> 一个 `FGameplayTag` 只要「有人在读它、有人在写它」，它就是一个合法参数键；它**不需要**在参数表里有一行。

**参数表里没有行**与**参数表里有行**的差别只有一条：有没有表达式可求。没有表达式 ⇒ 基数必须由调用方给。**账本、折叠算术、按来源摘除三件事对两者完全一致**——这也是「账本不认载体」的落点。

### 3.5.1 参数键 MUST NOT 镜像别的域的状态字段

**【已裁 R28｜m04864 / m04879】** 用户原话：「如果你说的是，ParameterModifier 修改 State-Level、Skill-Cooldown 这种没有 Key 的 Parameter 类型，我强烈反对 创建一个 对应的 参数键 Tag」。

**判据**：参数键装的是「**施加给别人的改动量**」，不是「**别人身上那个值**」。被改的东西留在它自己的域里，由那个域自己的入口承受改动。

| 反例（MUST NOT） | 正例 |
|---|---|
| 给技能冷却建 `Tag_TcsSkillParam_CooldownDuration`，在参数域里镜像 Skill 资产上那个 `double` | 建 `Tag_TcsParam_CooldownReduction`，值是 `-2`；施加方把它应用到**被改的那个域的入口**上（技能冷却的入口是 SkillModifier，不是参数域） |
| 给状态层数建 `Tag_TcsStateParam_StackCount`，镜像状态的 `StackCount` 字段 | 建 `Tag_TcsParam_StackBonus`，值是 `+1`；施加方调状态域的加层入口 |

**为什么立这条**：镜像键会让同一个数在两处各配一份，作者不知道该改哪个；而它换来的「统一读口」并不需要——**参数域出的是算术与账本，不是这些值的保管处**。

**它注销了原 N10**（「一个键既有参数行、又由字段基数折叠时谁是基数」）：那条问题只在有人建镜像键时才存在。禁掉镜像键，问题**不诞生**，不需要回答。

**它与「运行时基数」不冲突**：运行时基数不是别人域里的字段，是这一发 / 这一跳当场算出来的数，那个键没有参数行，因此也不存在第二个来源。

### 3.5.2 同一个数，谁承受

**【已裁｜m04911 / m04918】** 用户原话：「SkillModifier 应该修改的是 SkillEntry 的参数值，或者 SkillEntry 的 ActionChain」；「那确实只应该通过 Skill Modifier 来执行，不能通过 Parameter，但是可以用 Parameter 设置数值，执行时传给 Skill Modifier」。

以「天赋：让技能 A 的冷却变短」为例，「变短」有三种互不相同的语义，承受它们的地方也不同：

| 需求 | 语义 | 谁承受 | 参数域出什么 |
|---|---|---|---|
| **正在冷却**的那一条变短 | 改 Skill 域里已经起冷的那条计时 | Skill 域的 `AdjustCooldown` 一类入口 | 需要缩放时出一个数 |
| **以后每一发**的冷却变短 | 改 SkillEntry 的参数值，下一发施法按新值起冷 | **SkillModifier** | 出「减多少」这个数 |
| **这一发**的冷却变短（已出生） | 不可行（见下） | 不存在 | 不存在 |

三行里参数域都只做一件事：**出一个数**。它不保管「技能 A 当前冷却多少」——那住在 Skill 域。这与 §3.5.1 是同一条判据的两面：镜像键是把别人的值搬进来（禁），这里是把别人的值留在原处、只出改动量（正例）。

**第二行怎么落地**：参数域算出 `Resolve(Tag_TcsParam_CooldownReduction)` 得 `-2`，施加方把这个数交给 SkillModifier，由它去改 SkillEntry 的参数。参数域既不知道、也不需要知道这个数最后被用在了哪个技能上。

#### 冻结的范围是「这一发」，不是「这个技能」

`Bind = Born` 常被误读成「这个技能的冷却被定死了」。它不是。

| 时刻 | 事件 | 技能 A 下一次施法读到的冷却 |
|---|---|---|
| T0 | 施法一号，实例出生，读到 `10` | 一号起冷 10 秒 |
| T0.5 | 天赋施加（SkillModifier 给 A 的账本写 `-2`） | 一号**不变**，仍在跑 10 秒 |
| T1 | 施法二号 —— **新实例、新出生** | 读到 `8` |

⇒ 冻结的边界是**实例**：已出生的那一发不跟随，之后每一发都重新出生、重新读一次账本。

**由此得到一条观测不可区分性**：对「每发只读一次」的参数（冷却时长、消耗金额），`Born` 与 `Live` **在观测上完全一样**——两者都是「施法那一刻读一次账本」。差别只在「实例存续期内有第二次读它」的场合（例如链走到第 2 秒又去读一次冷却）。这反过来说明：这类参数不需要任何额外机制。

---

## 3A. 运算枚举归谁

**【已确认，无需再裁】** `ETcsAggregateOp` 与 `ETcsOverrideStrategy` 住**核心基线**，族内 `02-核心基线/数值聚合器.md` §3 已完整定义并 ✅ 全裁（含 7 个覆盖策略、`FTcsAggregateEntry` 四列、`IsStronger` 的两级比较、`OrderSeq` 的施加序语义）。

**参数域的做法 = 直接复用，不另立枚举、不另写折叠器。**

| 复用什么 | 出处 | 后果 |
|---|---|---|
| `ETcsAggregateOp`（七带） | 《数值聚合器》§3 | 参数修正器的 `Op` 列就是它 |
| `ETcsOverrideStrategy`（七值） | 同上 | 数值侧覆盖裁决直接用 |
| `FTcsAggregateEntry` 的字段集 | 同上 `§3` | `FTcsParamMod` 与之同构（多一个 `Key` 与 `Source`） |
| `EvaluateAggregatedValue(Base, Entries, Strategy)` | 同上 §3 单段重载 | 参数折叠**就是**它，参数域不写第二份公式 |

⚠️ **与 V1 代码现状的落差（不是设计缺陷，是待同步项）**：V1 的运算枚举只五带、无段级/全局覆盖，且名字带着属性域前缀。族内已裁的名字是 `ETcsAggregateOp`。**这是将来的实现追赶文档，不是文档改口**。V1 代码已按 2026-11-18 的裁决删除，需要核对原文时按 `git show HEAD:Documents/tcs-remake-v2-drafts/证据快照-V1引用出处.md` §1 恢复。

---

## 4. 求值器

**【已裁 m02177】用户原话：「我目前也倾向使用【求值器】」** —— 求值环境给的是**求值器**，不是**值表**。

```cpp
struct FTcsParamResolver
{
    const FTcsParamDefs* Defs  = nullptr;   // Key → 表达式 + 绑定时机
    FTcsParamState*      State = nullptr;   // 修正器集合（可写）
    FTcsParamBorn*       Born  = nullptr;   // 出生切片；为空 = 无切片（一律实时）
    FTcsParamEnv         Env;               // 装什么见 §4.3（封闭清单）
    TSet<FGameplayTag>   InFlight;          // 只在求值期间有效，用于兜环
};

// ── 数值 ────────────────────────────────────────────────
double FTcsParamResolver::Resolve(FGameplayTag Key)
{
    // ① 切片命中 ⇒ 立刻返回。**冻结与记忆化都在这一行发生。**
    if (Born)
        if (const double* Hit = Born->Values.Find(Key)) return *Hit;

    // ② 环检测（运行期兜底）：命中 ⇒ 返回 0 + 打 Error（§6.3）。正常永不触发
    if (InFlight.Contains(Key)) { /* Error 日志，见 §6.3 */ return 0.0; }
    InFlight.Add(Key);
    ON_SCOPE_EXIT { InFlight.Remove(Key); };

    // ③ 基数：表达式求值
    const FTcsParamDef* D = Defs ? Defs->Find(Key) : nullptr;
    const double Base = D ? D->Expr.Evaluate(Env, *this) : 0.0;

    // ④ 折叠挂在本键上的修正器（七带，复用长在核心基线的同一份算术）
    const double Final = State
        ? State->FoldNumeric(Key, Base)
        : EvaluateAggregatedValue(Base, /*空条目*/{}, ETcsOverrideStrategy::MaxValue);

    // ⑤ 出生键：算完进切片，此后永不再算
    if (D && D->Bind == ETcsParamBind::Born && Born)
        Born->Values.Add(Key, Final);

    return Final;
}

// ── 布尔：同一条流水线，折叠函数不同（§3.4）────────────
bool FTcsParamResolver::ResolveSwitch(FGameplayTag Key)
{
    // ① 切片命中（布尔切片与数值切片分开存，见下）
    if (Born)
        if (const bool* Hit = Born->Switches.Find(Key)) return *Hit;

    // ② 布尔不参与表达式求值 ⇒ 没有引用链 ⇒ 不存在环，无 InFlight 参与

    // ③ 基数：开关行自己的 Base
    const FTcsBoolSwitchDef* D = Defs ? Defs->FindSwitch(Key) : nullptr;
    const bool Base = D ? D->Base : false;

    // ④ 按开关行自带的策略裁决（§3.4 表），无条目时返回 Base
    const bool Final = State ? State->DecideSwitch(*D, Key, Base) : Base;

    // ⑤ 出生键同上
    if (D && D->Bind == ETcsParamBind::Born && Born)
        Born->Switches.Add(Key, Final);

    return Final;
}
```

**两条流水线的差别只有两处**：数值有 ③（表达式求值）与 ②（环检测），布尔两者皆无——布尔的基数是开关行上的 `Base`，没有表达式可递归。**其余步骤（切片命中、折叠/裁决、写切片）逐字同构**。这正是不需要两套机制的理由。

### 4.1 从这段代码直接得出的结论

> **「冻结」不是一个机制，是「把算好的 `double` 存进切片」这个动作的副产品。**

值一旦是切片里的一个 `double`，它就不再与任何东西相连——没有边、没有监听、没有回源。**不需要任何位、门、`Mode` 去「实现冻结」**；只需要决定「算一次之后还回不回源」。回源的（`Live`），每次读被重算，切片里留不下它。

### 4.2 为什么求值器优于值表

| | 值表 | 求值器 |
|---|---|---|
| 切片里装什么 | **必须装全键**，否则任一键查不到就落空 | 只装 `Bind == Born` 的键 |
| `Live` 键怎么办 | 逼你在出生时也冻一份副本，而读口又不认它 ⇒ **一张表里放两种语义不同的格子** | 现场算，不进切片 |

#### 4.2.1 源族：被求值的东西长什么样

求值器求的不是「数」，也不是「表」，是**一份值描述**。值描述只有一种基类：

```cpp
// 源基类：把「这个数怎么来」变成一个可求值的对象。宿主扩展入口就是派生它（§4.3.2）
USTRUCT(Abstract)
struct FTcsParamSource
{
    GENERATED_BODY()

    // Env：这一刻的求值上下文（纯数据四字段，§4.3）
    // R  ：求值器——递归问别的键、复用同一份环检测与切片命中
    virtual double Evaluate(const FTcsParamEnv& Env, FTcsParamResolver& R) const { return 0.0; }
};
```

**为什么是两个形参而不是一个**：

| 如果只给 `Env` | `ParamRef` 做不了。`Env` 是纯数据四字段，里面没有 `Defs`、没有 `InFlight`、没有切片——引用型源拿到它**递归不了**。要让 `Env` 装得下这些，它就从「这一刻的上下文」变成「求值会话」，与 `FTcsParamResolver` 的职责边界消失；而且 `Resolver` 已持 `Env`（§4 开头），两者互相指。 |
| 如果只给 `R` | 技术上够用（`R.Env` 就在上面）。但那样每个源都要写 `R.Env.Target`，把「我只要上下文」和「我要递归」两件事混成同一个入口；且纯求值路径（Area 出生、属性域派生表达式）也被迫先造一个 `Resolver` 才能读一个常量。 |

⇒ 两个形参各自对应一件不同的事：**`Env` 是「我读什么」，`R` 是「我问谁」**。只有 `ParamRef` 真用到 `R`，其余源收着不用——这是形式上的代价，换来「引用型源不需要任何旁路设施」，与 §4.3.2 删掉外部参数表是同一个理由。

> 裁决出处：m05249 用户原话「Context 是要的，你说的 Resolver 也可以加进去，后续可能还会有调整，目前先暂时这么裁定」。**形参表属可调整项**，调整时此节与 §4 求值器、§4.3.3、§4.3.6、§4.3.7 四处实现签名一并改。

#### 4.2.2 值描述：`FTcsParamValue`

源的基类之上还需要一层容器——因为**配置字段装的是「一份值描述」，不是「一个源」**：同一个字段今天装常量、明天装属性读、后天装组合，类型必须在运行时可换。

```cpp
USTRUCT()
struct FTcsParamValue
{
    GENERATED_BODY()

    // 一份「这个数怎么来」的描述。只装 FTcsParamSource 的派生（由 BaseStruct 约束）
    UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/<Module>/FTcsParamSource", ExcludeBaseStruct))
    FInstancedStruct Source;

    bool IsValid() const { return Source.IsValid(); }

    template <typename TSource>
    const TSource* GetSource() const { return Source.GetPtr<TSource>(); }

    // 唯一求值入口：转发给源（R34）
    double Evaluate(const FTcsParamEnv& Env, FTcsParamResolver& R) const
    {
        const FTcsParamSource* S = Source.GetPtr<FTcsParamSource>();
        return S ? S->Evaluate(Env, R) : 0.0;
    }
};
```

> `<Module>` 是 UHT 要求的资产路径前缀；模块名尚未定（族内 `INDEX.md` §2.4 明确核心基线 MUST NOT 预设为独立模块），此处不代它决定。

**为什么包一层，而不用 `using FTcsParamValue = FInstancedStruct`**：

| 理由 | 说明 |
|---|---|
| 属性域有个 `UPROPERTY` 等着它 | 族内 `领域-Attribute-草稿.md` §1.1 的 `DeriveExpression` 是 `UPROPERTY(EditAnywhere)`。别名过不了 UHT 的类型识别 |
| 「只能装源」要能被执行 | `meta = (BaseStruct = …)` 让编辑器只列 `FTcsParamSource` 的派生；裸 `FInstancedStruct` 什么都能装，约束只剩一句注释 |
| 需要门面 | `IsValid()` / `GetSource<T>()` / `Evaluate()` 三件事若散在调用点，就是每处各写一遍 `Source.GetPtr<FTcsParamSource>()` |

**为什么求值入口挂在值描述上，而不是求值器上的 `ResolveExpr`**：

`Env` 已经在调用点手上，`R` 也已经在手上。`Operand.Evaluate(Env, R)` 读起来是「求这个值」；`R.ResolveExpr(Operand)` 是把值交回求值器再取回结果——同一个动作多一次转手，主语从「这个值」变成「求值器」。

R22 当年写的是「MUST NOT 做成**源族自带的自由函数**」。`FTcsParamValue::Evaluate` 是成员方法，不是自由函数；「表达式求值有两个入口」的风险仍被挡住，因为**只有这一个成员方法，`ResolveExpr` 这个名字不再存在**。

> 裁决出处：m05269 用户原话「先用你给的两个方案吧」。

### 4.3 环境里装什么

求值器带到每一次 `Evaluate` 调用里的，只有下面四项。**这份清单是封闭的**——加字段 MUST 先过 4.3.1 的判据。

（类型名本文沿用草稿的 `FTcsParamEnv`；正式命名属命名项，与本文其他命名一并裁。）

```cpp
struct FTcsParamEnv
{
    TObjectPtr<UWorld>     WorldContext   = nullptr;   // ① 世界：世界级子系统从这里按类型取
    FTcsCombatEntityHandle Target;                     // ② 本次求值为谁（宽义，见 §4.3.1）
    FTcsCombatEntityHandle Instigator;                 // ③ 本次求值的施加者
    int32                  EffectiveLevel = 0;         // ④ 「这个东西」多强（技能等级 / 效果等级），见 §4.3.4
};
```

#### 4.3.1 判据：什么该进环境，什么不该

> **环境里只放「能被句柄指到的参与者」的句柄，不放它们的任何值；只有「句柄指不到」的东西，才配占一个值字段。**

| 种类 | 例子 | 怎么拿 | 进不进环境 |
|---|---|---|---|
| **世界级子系统** | 属性账本 | `WorldContext` 上按类型取 | **不占字段**——由用到它的那个源自己取 |
| **参与者** | `Target` / `Instigator` | 调用方在求值前填句柄 | **占字段**（装的是句柄，不是值） |
| **句柄指不到的量** | `EffectiveLevel` | 调用方在求值前填 | **占字段**——没有别的来源 |

**`Target` 取宽义**：不是「被打的那个实体」，而是「**这份参数为谁求值**」。技能参数以它的持有者（施法者）为 `Target`，buff 参数以被施加者为 `Target`，Area 参数以施法者为 `Target`。窄义在两处直接失效——AoE 技能读「半径」时没有单一目标；而技能参数的主体本来就不是受击者。

这条判据挡的是**字段随参与者数量膨胀**：只要允许「参与者的某个值」进环境，角色每多一种就要多一个字段——今天是 `InstigatorLevel`，明天要 `TargetLevel`，后天还有 `OwnerLevel`、`SummonerLevel`。而它们全都是**同一个问题的同一个答案**：问 `ITcsEntityQuery::TryGetEntityLevel`（§4.3.5）。句柄已经在环境里了，值不必再来一份。

**`EffectiveLevel` 为什么留下**——它不是「参与者的值」，是「**这个东西自己的值**」。技能、buff、Area **都不是 `FTcsCombatEntityHandle`**，没有句柄能指到它们，`TryGetEntityLevel` 也问不出来。这不是「暂时没有句柄」，是结构上没有：

| | 持有者**是**实体 | 持有者**不是**实体 |
|---|---|---|
| 谁 | 施法者 / 目标 / 宠物 / 主人 | 技能 / buff / Area |
| 等级怎么取 | **问 `ITcsEntityQuery`**（句柄已在环境里） | **只能由装配方填进环境** |
| 环境里有没有对应字段 | 无 | `EffectiveLevel` |

⇒ 去掉 `InstigatorLevel` 与留下 `EffectiveLevel` **不是取舍，是同一条判据的两个必然结果**。

判据还有一条非审美理由：字段一旦进环境，**每一个**求值调用点都得填它。把世界能取到的东西做成字段，等于逼每一个调用点各自重复一次「子系统在哪」的判断，而且判断错了不报错、只静默落兜底。

环境因此仍是**纯数据**（无虚函数、无派生类）——那套「派生环境 + 源内 checked cast」机制的退场见 §4.4。

#### 4.3.2 一个被删掉的字段：外部参数表

从零看，引用型源（`ParamRef`）要的是「**同一份 `Defs` 里的另一个键**」。这件事已经由 `Resolve(Key)` 提供——递归、环检测、切片命中全在它里面。再留一个「只能查数」的旁路表，等于同一件事两套表示，而两套表示迟早会在「谁是权威」上分叉。

⇒ **环境里没有外部参数表字段**。引用型源的求值走 `D->Expr.Evaluate(Env, *this)`——它拿到的是**求值器**，问「另一个键是多少」就 `Resolve(Key)`。

连带退场的三样（不是决定，是这一个字段消失的后果）：

| 退场的东西 | 为什么它没有理由了 |
|---|---|
| 「只读键值查询」这一接口形状 | 它唯一的真实消费者就是引用型源；引用型源改问求值器之后，接口零消费者 |
| 它的反射壳（绑定 / 解绑 / 取当前绑定） | 壳的职责是让一个非反射结构体经由 UObject 转一手；字段没了，没有东西可绑 |
| 为「非反射结构体里的对象引用 GC 看不见」而手写的追踪代码 | 那类结构体连同它的持有链一起消失 |

**宿主扩展入口收敛为一条**：提供一种 `FTcsParamSource` 派生。这比「宿主提供一张表」更窄，也更准确——宿主想给的从来是「这个数怎么来」，而不是「这里有一堆数」。

#### 4.3.3 属性源怎么读（含一条跨域前置条件）

```cpp
double FTcsParamSource_AttributeScaled::Evaluate(const FTcsParamEnv& Env, FTcsParamResolver& R) const
{
    UTcsAttributeSubsystem* Attrs = UTcsAttributeSubsystem::Resolve(Env.WorldContext);
    if (!Attrs) return Fallback;                                    // 没有世界 ⇒ 兜底，不报错

    // Side 决定读谁的属性：与属性域同名的 ETcsAttrReadSide 一套词汇（见下）
    const FTcsCombatEntityHandle Who = (Side == ETcsAttrReadSide::Instigator) ? Env.Instigator : Env.Target;

    double V = 0.0;
    if (!Attrs->TryGetValue(Who, Attribute, V)) return Fallback;   // 取不到 ⇒ 兜底
    return Coefficient * V;
}
```

**`Side` 这个字段是「`Target` 宽义」的直接收益**：属性域的同一个源早就能读两侧（`Attribute.md` §3.5 的 `ETcsAttrReadSide`，`{ Target, Attr.Strength, 2.0 }` 那种写法），而参数域原先只读主体一侧（该字段旧名 `Subject`，现名 `Target`）。两边词汇对齐成 `Target` / `Instigator` 之后，参数域补这一个字段就与属性域逐字同构——**不需要新概念，只是把已裁的形状搬过来**。

⚠️ **前置条件（跨域请求，本文不改任何代码）**：属性侧今天只提供「取值」形式的口，**缺一个「取不到能报出来」的 Try 口**。缺了它，上面第二行的兜底永远不生效——实体未注册、属性未定义时取回 `0`，与「属性确实是 0」不可区分，作者配的 `Fallback` 被静默吞掉。

⇒ **这个读口属性域已经有了，不需要新补**。族内 `领域-Attribute-草稿.md` §8.1：

```cpp
// 返回 CachedCurrent。不重算、不发事件
bool TryGetValue(FTcsCombatEntityHandle Entity, FGameplayTag Attribute, double& OutValue) const;
```

`bool` 正好区分「取不到」与「确实是 0」——`Fallback` 因此有个明确的触发条件；`CachedCurrent` 正是「当前值」，正好是本源要的东西。它「不重算」也正合适：本源是在属性域的结算**内部**被调用的，若它反过来触发重算就是求值中途重入（族内 §2.2 已否决过）。

#### 4.3.4 只有一个等级字段

**【裁决修订｜m04670】** 用户原话：「我忽然意识到之前的裁决有问题，ParamEvaluateContext 里 会有 Instigator、Target（也就是Subject，我还是觉得Target更合适），现在我觉得不需要添加 Instigator，因为如果需要，可以直接在 Param 自己的 Evaluate 函数中，通过 Instigator 获取到；况且万一需要的时 TargetLevel呢，那岂不是又要给 ParamEvaluateContext 添加内容」

⇒ **R18 的「环境加 `InstigatorLevel` 字段」那一半撤回**。环境只留 `EffectiveLevel`（§4.3 的四项）。

**「两种等级」这个事实不变，变的是它们各自的表达方式。** 等级确实有两种语义：

| 语义 | 「**这个东西**多强」 | 「**那个人**多强」 |
|---|---|---|
| 例子 | 技能练到几级、这份效果叠到几层 | 施法者 / 施加者这个实体本身多少级 |
| 受它影响的数值 | 技能自己的参数（伤害、射程、冷却）按等级取档 | 「随施法者等级成长」的那部分数值 |
| 怎么表达 | **环境字段** `EffectiveLevel` | **源** `FTcsParamSource_ContextLevel{ Instigator }`（§4.3.6） |
| 为什么是这个 | 技能 / buff / Area **不是实体**，没有句柄指得到 | 施法者**是实体**，句柄已在环境里 |

两者在引导技能、宠物技能、装备授予技能这三类里**必然不同**（技能等级与主人的角色等级毫无关系）。⇒ 它们**仍然是两个数**，只是取值路径按「持有者是不是实体」分了岔——判据和那张表在 §4.3.1。

**为什么 R18 当年的理由现在不成立**：R18 的实质理由是「源不需要认识宿主；等级提供者是『宿主注入的可替换实例』，属于装配期知识」。它反对的是一个**宿主注入的可替换实例**。

而 R19（`m03403`，**在 R18 之后**）把取等级搬进了 `ITcsEntityQuery`。族内 `02-核心基线/战斗实体.md` §3 原文：「**它是 (a) 类依赖倒置缝**：核心不能反向依赖宿主，故不受『2 个实现者』限制」。⇒ 等级的来源从「宿主注入的可替换实例」变成了**核心基线自己的依赖倒置缝**，源读它是在用框架语义，不是宿主词汇。**R18 的理由被它之后的一条裁决消解掉了。**

**成本对比（这才是决定性的）**：

| | 加字段（R18） | 走源（本节） |
|---|---|---|
| 今天 | 一个 `int32` | 一个源类型，零状态 |
| 角色多一种（要读目标等级） | **再加一个字段**，且 `FTcsParamEnv` 的**每个**调用点都得填 | 已覆盖：`ContextLevel{ Target }` |
| 角色再多一种（要读主人等级） | 再加一个字段 | 已覆盖：源里加一个枚举值，环境不动 |
| 加错/漏填 | 静默落兜底 | 兜底同样静默，但**只有用到它的那个源**要填 |

⇒ 字段方案的成本随角色种类**线性增长**，且增长的是「每个调用点都要维护」的那一层；源方案的增长只落在一个类型内部。

**边界要写清**：`EffectiveLevel` **仍然**由装配方填，因为没有任何句柄能指到技能 / buff / Area。这不是「还没做」，是结构上没有（§4.3.1 第二张表）。

#### 4.3.5 等级从哪来：战斗实体契约

**【已裁 丁｜m03403】** 用户原话：「我个人倾向于 方案【丁】，虽然 EntityLevel 确实算是 宿主词汇，但我能想到的所有需要技能系统的游戏，几乎都需要这个功能，所以可以说是从宿主需求中提炼出来的通用功能；再者，你提到的 宠物跟主人等级、这个完全可以靠 接口重载；而装备授予的技能按装备等级，首先按照我的理解，装备不能算 Entity，其次就算要实现这个需求，也应该是配置方案解决，比如 LearnSkill 时，直接读取装备等级，设置技能等级；」

**裁决**：取「某个实体多少级」的唯一来源是**战斗实体查询契约**——它加一支取等级。

**两个调用方**（R18 撤回后，两个等级字段只剩一个，但问契约的位置从一个变两个）：

| 谁问 | 什么时候问 | 为什么 |
|---|---|---|
| 装配方 | 装配技能 / 效果实例时 | 填 `EffectiveLevel`——但注意它填的**不是实体等级**，是「技能几级 / 效果几层」（§4.3.1 第二张表） |
| **源**（`FTcsParamSource_ContextLevel`，§4.3.6） | 每次 `Evaluate` | 读 `Target` / `Instigator` 的实体等级 |

⚠️ 上表第一行容易被读错：`EffectiveLevel` **不是**从契约来的。技能 / buff / Area 没有句柄，装配方填的是它们自己的档位（技能练到几级），来源由装配方自己决定。契约只管「**实体**多少级」。

```cpp
// 核心基线 · 战斗实体契约（ITcsEntityQuery），新增一支
virtual bool TryGetEntityLevel(FTcsCombatEntityHandle Entity, int32& OutLevel) = 0;
```

**【已裁 甲｜m03431】** 签名取 `bool` + 出参这一形态（而非「直接返回 `int32`、0 表示没有等级语义」）。

**【推导】为什么是 `bool` + 出参，而不是直接返回 `int32`**：与同一个契约里既有方法的形态一致（`GetLocation` 也是「`bool` + 出参」）。取不到等级与「等级就是 0」必须可区分——否则「这个实体没有等级语义」会被读成一个真实的 0 级值，本该走兜底的那条路静默走通。这条判据在属性侧已经付过一次代价：`TryGetBaseValue` 之所以是 Try 形态，理由逐字同构。

**【推导】为什么「等级」进这个契约不算越界**：族内 `战斗实体.md` §5.4 的禁区原文是「**固定属性集**会把宿主词汇绑进契约」——它反对的是在契约里开一张**可扩展的属性清单**（体型 / 阵营 / 类型…每加一个维度就改一次契约）。等级是**一个标量、一种语义**，与既有那支 `GetLocation` 返回一个 `FVector` 同构：宿主词汇（等级叫什么、怎么算）留在宿主，契约只承诺「问得到」。

**为什么「来源各不相同」不构成障碍**（用户原话要点）：宠物跟主人等级、召唤物跟着召唤者，都是**同一个问题在不同实现里的不同答案**，用接口重载即可——同一个 `TryGetEntityLevel`，不同实体类型给不同数。框架不需要知道它们为什么不同。

**为什么「装备授予的技能按装备等级」不在此列**（用户原话要点）：装备不是实体。这条需求属于配置方案——`LearnSkill` 那一刻读装备等级、把结果写进技能等级，而不是让参数域去问一件不是实体的东西。

⚠️ **这不构成对 R16 的推翻**：R16 收的是「**读参数值**的宿主扩展」，那条路收敛为「提供一种 `FTcsParamSource` 派生」；取等级是「**读世界**」，属战斗实体契约那一路。两个口各自封闭，不是同一件事的两种表示。

**连带**：宿主侧的「等级提供者」接口失去唯一职责（取等级改问战斗实体契约），环境派生机制随之失去全部字段——处置见 §4.4。

#### 4.3.6 `FTcsParamSource_ContextLevel`：把等级暴露给表达式

**【新增｜m04651】** 用户原话：「可以直接新增一个 Param 类型，类型名就叫 ContextLevel，三种选项，Instigator、Target、State」；第三值的定名见下（`m04671` 定为 `StateSelf`）。

```cpp
UENUM()
enum class ETcsContextLevelSource : uint8
{
    Instigator,   // 施加者实体等级 —— 问 ITcsEntityQuery(Env.Instigator)
    Target,       // 主体实体等级   —— 问 ITcsEntityQuery(Env.Target)
    StateSelf,    // 这个东西自己的档位 —— 读 Env.EffectiveLevel，不走契约
};

USTRUCT()
struct FTcsParamSource_ContextLevel : public FTcsParamSource
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    ETcsContextLevelSource LevelOf = ETcsContextLevelSource::StateSelf;

    UPROPERTY(EditAnywhere)
    double Fallback = 0.0;

    virtual double Evaluate(const FTcsParamEnv& Env, FTcsParamResolver& R) const override
    {
        // 「这个东西自己的档位」（技能几级 / 效果几层）：没有句柄，只能读环境字段，不走契约
        if (LevelOf == ETcsContextLevelSource::StateSelf)
        {
            return static_cast<double>(Env.EffectiveLevel);
        }

        // 「某个实体的等级」：句柄已在环境里，问契约要值（不把值再存一份进环境）
        const ITcsEntityQuery* Query = GetEntityQuery(Env.WorldContext);
        if (!Query) return Fallback;

        const FTcsCombatEntityHandle Who =
            (LevelOf == ETcsContextLevelSource::Instigator) ? Env.Instigator : Env.Target;

        int32 Level = 0;
        if (!Query->TryGetEntityLevel(Who, Level)) return Fallback;   // 取不到 ⇒ 兜底，不报错
        return static_cast<double>(Level);
    }
};
```

四件事要说清：

**一、`StateSelf` 这个名字。** 用户原话倾向「State」，`m04671` 定为 `StateSelf`——取「这个东西自己的档位」以避开与 V2 七个领域名里的 **State 域** 撞词。⚠️ 它是**建议名**，命名属命名项，可随时改。

**二、不加 `Coefficient`。** `AttributeScaled` 有系数，是因为它常被用作「× 2.0」；`ContextLevel` 的乘法由外层组合完成。⇒ 不加系数，源族才一致：

```cpp
// HeroAttackPower × HeroLevel × 0.3
// Terms 的元素类型是 FTcsParamValue（与属性域 §7.4 的写法同形）
FTcsParamValue Dmg = FTcsParamSource_Composite{
    CombineOp = ETcsAggregateOp::Mul,
    Terms = {
        FTcsParamValue{ Source = FTcsParamSource_AttributeScaled{ Side = Target, Attribute = Attr.AttackPower, Coefficient = 1.0 } },
        FTcsParamValue{ Source = FTcsParamSource_ContextLevel{ LevelOf = ETcsContextLevelSource::Instigator } },
        FTcsParamValue{ Source = FTcsParamSource_Literal{ Value = 0.3 } },
    },
};
```

**三、⚠️ 一个尚未定的接缝**：`ITcsEntityQuery` 从 `Env.WorldContext` 怎么拿到实例？族内 `02-核心基线/战斗实体.md` §3 只定义了接口形状（「宿主创建、宿主销毁；框架只持非拥有引用」），没写获取路径。`AttributeScaled` 走的是 `UTcsAttributeSubsystem::Resolve(Env.WorldContext)` 这条同类路。⇒ 该路径要么已存在、要么要补一句，**族内落地时与战斗实体契约一并定**，本文不替它决定。

**四、它不受 R27 限制（原 N13）。** R27 禁的是「属性修正器的值描述读**参数账本**」。`ContextLevel` 读的是**实体等级**与**环境字段**，两者都不是参数账本——所以这个源可以在参数域的表达式里自由使用，与「属性修正器的表达式里能不能出现 `ParamRef`」是两件事。

#### 4.3.7 无 `Key` 的值：消费点自己冻

**【已裁 甲｜m04671】** 用户原话：「N5，暂时可以先这么裁决」。裁决内容：**参数域不为「无 `Key` 的值」提供冻结设施，冻结归消费点自己。**

**问题**：`Bind` 只能配在 `FTcsParamDef`（参数行）上，切片 `FTcsParamBorn` 是 `TMap<FGameplayTag, double>`。而无 `Key` 的值（Area 的每跳伤害、某时段时长、冷却时长、消耗金额、链步骤内联值）既没地方配 `Bind`，也没格子在切片里。⇒ 它们要「生成那一刻定死」时，靠什么？

**形态**：消费点自己两行——Def 上一个 `ETcsParamBind`，实例上一个 `double`。

```cpp
// Def 侧（共享，配置期）
struct FTcsAreaDef
{
    FGameplayTag   Id;
    FTcsParamValue TickDamageExpr;                      // 一份值描述，无 Key
    ETcsParamBind  TickDamageBind = ETcsParamBind::Born;
    double         Interval = 2.0;
};

// 实例侧（每个 Area 一份）
struct FTcsAreaInstance
{
    const FTcsAreaDef*     Def = nullptr;
    FTcsCombatEntityHandle Instigator;
    double                 BornTickDamage = 0.0;        // 一个 double，没有任何「有没有冻」的标记

    // 由「一次放置」的时刻调用，形参是那一刻有效的环境与求值器
    void Birth(const FTcsParamEnv& Env, FTcsParamResolver& R)
    {
        if (Def->TickDamageBind == ETcsParamBind::Born)
        {
            BornTickDamage = Def->TickDamageExpr.Evaluate(Env, R);
        }
    }

    double GetTickDamage(const FTcsParamEnv& Env, FTcsParamResolver& R) const
    {
        return (Def->TickDamageBind == ETcsParamBind::Born)
            ? BornTickDamage
            : Def->TickDamageExpr.Evaluate(Env, R);
    }
};
```

**为什么这不是 R22 撤掉的那种「一位两形状」**：R22 撤的是同一个 `FTcsAttrModInstance` 里既放变量又放它的副本，于是必须回答「副本何时失效、怎么保持一致、要不要登记依赖边」。这里的 `FTcsAreaInstance` **全程只有一个形状**（一个 `double`）；它什么时候被填，由 **Def 上的配置**决定，实例自己不问、也不存那个判断。⇒ 一个配置 + 一个缓存，不是「一个位表达两种形状」。

**为什么消费点自己管是正解，而不是「参数域偷懒」**：

| 判据 | 为什么指向甲 |
|---|---|
| §4.1 已裁 | 「**冻结不是机制，是『把算好的 `double` 存起来』这个动作的副产品**」。单个值不需要容器，存字段就是冻结 |
| 铁律 1 | 乙（统一槽）的消费者是「多个消费点的相同两行」，但那两行**本来就不一样**——算的时刻不同（Area 是 Spawn、时段是进入、冷却是起冷）、存放位置不同。统一它们得不到任何东西 |
| 概念纯度 | 丙（给无 Key 值发内部 Key）会让「只有人读、没有人写」的参数行出现，直接作废 R17 的判据（「有人在读它、有人在写它，才是合法参数键」） |

**走查表**（配置：`AttackPower` T0 = 100，T0+1s 吃 +20% buff ⇒ 120；`Level` = 10，T0+4s 升 11 级；`Interval` = 2 秒；`TickDamageExpr` = `AttackPower × Level × 0.3`）：

| 时刻 | `Bind = Born` | `Bind = Live` |
|---|---|---|
| T0 生成 | `Birth` 算 `100 × 10 × 0.3 = 300` 存进字段 | 不算 |
| T0+2s 第一跳 | **300** | `120 × 10 × 0.3 = 360` |
| T0+4s 第二跳（已 11 级） | **300** | `120 × 11 × 0.3 = 396` |
| 中途热重载把 `0.3` 改掉 | **300**（已生成的 Area 不受影响） | 跟随新配置 |
| 作者要写多少代码 | `Birth` 里一个 `if`；每多一个要冻的值多一行 | 不写 |

⚠️ 注意第一跳两者**就已经不同**（`Born` 用的是生成那一刻的 100，`Live` 用的是 buff 后的 120）——不是「第二跳才分岔」。

**规模边界（要写清，否则甲会被滥用）**：单个消费点要冻的量 **≤ 3～5 个** 时用字段；超过就应当**发键**——那时它其实是在建一张表，用丙（局部发键，进 `Defs` + 切片）更省。判据是「少量就字段，多了就该建表」，不是「甲永远对」。

**一处 ⚠️ 待族内落地时才定**：`FTcsParamSource_ContextLevel` 怎么从 `Env.WorldContext` 拿到 `ITcsEntityQuery`（§4.3.6 第三条）。Area 的 `Birth` 要读等级，就依赖这条路径。
---

### 4.4 派生环境机制退场

**【已裁 甲｜m03431】** 环境派生机制（「结构体继承 + 源内 checked cast」整套）**整机制退场**。

它是为「某个字段不能住基类环境」而生的：基类不得反向依赖域，于是域把字段挂在自己的派生结构体上，源在 `Evaluate` 里用 `GetScriptStruct()` 判定是不是自己认识的那个派生类型，不是就落兜底。整套机制的**唯一存在理由**是那个「基类装不下的字段」。

R14（世界级东西走 `WorldContext`）之后，这类字段**就已经一个不剩**；`m04670` 撤回 `InstigatorLevel` 只是把这个结论补严（§4.3.4）：

| 曾经的派生环境 | 它挂的字段 | 现在从哪来 |
|---|---|---|
| 属性派生环境 | 属性提供者 | `WorldContext` → 属性账本，由源自己取（§4.3.3） |
| 等级派生环境 | 等级提供者 | 源问 `ITcsEntityQuery::TryGetEntityLevel`（§4.3.5），**不进环境** |
| 属性派生环境 | （无其它字段） | — |

⇒ 两个派生环境**各自的唯一字段都没了**，机制失去全部消费者。

**退场清单**（连带，不是决定）：

| 退场的东西 | 为什么它没有理由了 |
|---|---|
| 两个派生环境结构体 | 各自唯一的字段已改走别的路 |
| 基类上的「自报类型」虚函数 | 它是「源内 checked cast」的锚点；没有派生类型可 cast，它就是一条永远走真分支的死路 |
| 等级提供者接口，及其注入面（设置 / 读取 / 持有） | 取等级改走 §4.3.5 的战斗实体契约 |
| 各源 `Evaluate` 里的类型判定与兜底分支 | 同上：无派生类型可判 |

**源只认一个环境结构体**（§4.3 那四项）。这条是退场的直接收益：所有源的函数签名同形，「这次求值看得见什么」在类型上只有一种答案，调用点不可能填错结构体。

⚠️ **代价**（记录，不是反对）：将来若真出现「世界里取不到、又必须按类型分派」的环境字段，这套机制要重新引入。判据是**那时有没有那个字段**，不是「这套机制曾经有用」——今天它是一个零消费者设施。

**【推导】为什么等级这种「按等级取档」的便利源不算判据**：它读的 `EffectiveLevel` 是环境上的普通 `int32`，而「某个实体多少级」走 `ITcsEntityQuery`（§4.3.5），两者都不需要**按类型分派到不同的环境结构体**，因此与派生环境无关，一并留下。

### 4.5 与属性域的接缝

**【已裁 乙｜m03526】用户原话：「同一实体内部『属性 A 变化要触发属性 B 重算』这个需求 是属性域必须支持的硬性需求，所以应该选择乙方案」**

⇒ **属性修正器保持「持有一份值描述」，不改成「持有一个算好的 `double`」。**

理由在属性域那边：它的依赖图靠「修正器自己说得出它读了哪条属性」建成。值一旦被算成数，「读了什么」就不在配置里了，那条边建不出来，「A 变 ⇒ 依赖 A 的 B 重算」整条链失效。这是硬需求，不是优化。

**两个账本的修正器，从零之后不同形**：

| | 参数账本的修正器 | 属性账本的修正器 |
|---|---|---|
| 存的 | `double Value`（算好的数，§3.3） | 一份值描述 |
| 为什么敢 / 不敢存数 | 参数键的基数归参数域自己管，修正器只回答「改多少」 | 基数在属性图里，修正器必须自己说得出读了谁 |

⇒ 不能把参数账本那个形状套到属性域头上，也不能反过来。

**「借参数域的源族」不是「属性域直接用参数域那个类」**：那样 `AttributeScaled` 的求值实现会有两处（属性域原有的运算数种类 + 参数域的源），正是「一类值一个求值器」要禁止的形态。**属性域原有的运算数种类退场，只留「一份表达式 + 运算带 + 优先级」**。

**【已裁 甲｜m03601】用户原话：「这个功能不是已经被 Parameter 自己的 Born 语义取代了吗，所以应该是 方案【甲】吧；」**

⇒ **属性修正器不带任何「冻结位」。** A（跟随）与 B（定死）的差别由**值描述本身**表达，修正器不参与裁决。

这不是「那个位没用所以删」，是**那个位在制造一个本来不存在的问题**：用一个位加一种形状去表达两种形状（常量 / 依赖别处的量），于是「冻」的那一支必须在同一个结构体里同时放「变量」和「它算出来的副本」，立刻要回答副本何时失效、怎么保持一致、要不要登记依赖边。删掉位，两种形状各是一种，**一致性问题不诞生**——不是被解决。

**【已裁 甲｜2026-11-19】N13：属性修正器的值描述 MUST NOT 读参数账本。**

用户原话（问答自定义回答）：「先不能吧，我暂时也没想到 真实业务案例。」

⇒ 「值描述里能出现什么」因此是一条**封闭清单**：

| 源 | 能不能出现在属性修正器的值描述里 |
|---|---|
| `FTcsParamSource_Literal` | ✅ 字面量，本身就是常量 |
| `FTcsParamSource_AttributeScaled` | ✅ 读一条属性。**但 `Side` 只能取 `Target`**——本域的求值环境里 `Instigator` 恒无效（R30，族内 `领域-Attribute-草稿.md` §7.2.1）。要读施法者的属性有两条路，都不经过修正器的 `Operand`：**一次算完**的（伤害、护盾）写在参数行里（`Bind = Born`，参数行的环境两个句柄都齐）；**要长期跟随**的走施加方订阅重挂（R32） |
| `FTcsParamSource_Composite` | ✅ 上述两类递归组合 |
| `FTcsParamSource_ParamRef` | ❌ **MUST NOT**。它只能出现在参数行自己的表达式里——那里有求值器 |

⇒ 冻结语义的归属只剩两处，都在修正器之外：

| 值描述引用了什么 | 「冻不冻」由谁定 |
|---|---|
| 一条属性 | **没有冻结概念**——读属性天然跟随。要定死，施加方先算成一个常量再挂 |
| 字面量 | 本身就是常量 |

⇒ 属性修正器上没有任何东西可放，它就是「一份值描述 + 运算带 + 优先级」。

**要一个「此刻的参数值」，唯一写法是在 `AttachMod` 之前取出来、当字面量传**：

```cpp
const double Amp = SkillInstance.TryGetParam(Tag_TcsSkillParam_IgniteAmp);   // 账本读数
Attrs->AttachMod(Target, ModDef_IgniteVuln,
                 FTcsParamValue{ Source = FTcsParamSource_Literal{ Value = Amp } },
                 SourceHandle, /*Priority*/ 0);
```

**为什么不能允许**——四条判据，没有一条是「V1 没这么干」：

1. **属性域的每一次读 MUST 能被 `ForEachAttributeRead` 抽成一条依赖边**。属性是写时重算 + `CachedCurrent` 缓存（族内 `领域-Attribute-草稿.md` §2.2，读路径原文「不重算、不发事件、零副作用」），缓存正确性**完全**靠边维持。而建边机制只认属性词汇（同文档 §5.2 的 `ForEachAttributeRead(Mod.Operand, …)`，回调三参数是 `SrcTag` / `ETcsAttrReadSide` / `ETcsAttrReadPart`）——参数键混进 `Operand` 抽不出任何边，**且不报错、静默失效**。这等于给自己写的不变式（同文档 §5.4「不变式把『漏』变成『不可能』」）开口子。
2. **能承受「不维持的跨域读」的只有读时求值的域**。参数域已经有这么一处：`FTcsParamSource_AttributeScaled` 读属性、而属性变化不触发参数键重算（§4.3.3）。**反方向不行**——这不是对称的两个方向，是两个形状不同的域各有一条自己能承受的读法。
3. **没有真实消费者**（铁律 1）。用户明确：暂时想不出「只活在参数账本里、从不写进属性、却必须被属性修正器读到」的量。
4. 业界可查的五家（UE GAS / AbilityKit / WoW / LoL / Dota2）**无一家**实现「允许读但不建依赖边」；GAS 用 `bSnapshot` 的两个分支把这个中间态直接排除了。

⚠️ **「跟随」有三种，出路不同——MUST NOT 把前两种当成第三种**：

| 想要的效果 | 出路 | 配置即生效吗 |
|---|---|---|
| 加成跟着**自己身上**的属性走 | `Side = Target` 的 `AttributeScaled`，进依赖图（族内 `领域-Attribute-草稿.md` §5.1） | ✅ 是 |
| 加成跟着**施法者**的属性走 | **D15 订阅**：施法者那条属性变 ⇒ 回调里 `DetachMod` + `AttachMod`（R32） | ❌ 要写订阅代码 |
| 加成跟着**参数账本**走 | ❌ 这个组合没有消费者 | — |

**第三行为什么没有消费者**——完备二分：账本里的每一个数，来源只有三种（配置常量 / 从某条属性读来 / 被另一个 `FTcsParamMod` 改出来），而第三种的 `Value` 又只能是前两种。⇒ 账本里的数**要么已经落在某条属性上，要么它的消费者不在属性域**（冷却缩减、消耗减免、前摇时间）。前者读属性就行，还额外拿到聚合后的值与依赖边；后者属性修正器根本不参与。

⇒ **被 R27 挡住的不是需求，是「跟随参数账本」这一种写法。** 若将来真出现反例，翻案的方向是「允许 **且建边**」，**不是**「允许但不建边」：后者给不出「什么时候刷新」的确定答案（同一个写法在「`Operand` 纯 `ParamRef`」与「混了一个属性读」两种情况下行为完全不同）。

**「把一份值描述求成一个数」的入口不是新设施**：参数域 `Resolve(Key)` 的第 ③ 步本来就是「取表达式 → 求值」，这个动作就是 `FTcsParamValue::Evaluate(Env, R)`（§4.2.2），转手调源的 `Evaluate`——**复用同一个环境、同一个 `InFlight`、同一份环检测**，零新实现。⚠️ MUST NOT 另造一个同义入口（自由函数，或求值器上的 `ResolveExpr`）——那样表达式求值有两个入口，而环检测与切片都住在求值器上，裸求值两样都没有。

---

## 5. 出生

**【已裁 甲｜m02185】用户原话：「出生是"实例建立时一次算完"（甲）」** —— 急切出生，不是惰性。

**【已裁 m02218】** `Bind` 默认 = `Born`。

### 5.1 出生是一段职责，不是一个副作用

```cpp
void FTcsParamResolver::Birth()
{
    check(Born && Born->Values.Num() == 0 && Born->Switches.Num() == 0);   // 前置：空切片

    for (const FTcsParamDef& D : Defs->All())
        if (D.Bind == ETcsParamBind::Born)
            Resolve(D.Key);                                            // 数值

    for (const FTcsBoolSwitchDef& S : Defs->AllSwitches())
        if (S.Bind == ETcsParamBind::Born)
            ResolveSwitch(S.Key);                                      // 布尔，同一份绑定语义

    // 后置不变量：每个 Bind==Born 的键都能在对应的切片里查到（Debug 可断言）
}
```

遍历顺序无关：`Resolve` 递归时上游已经算好，轮到该键时命中缓存直接返回。

### 5.2 门只存在于一处

急切出生之后，`Bind` **只在出生那一刻被读一次**。此后 Born 键走切片、Live 键每次重算，**两条路都不再问 `Bind`** ⇒ **门只存在于 `FTcsParamDef` 里，一处，不进实例**。

（旧方案曾把门拷进实例；急切出生把那次拷贝的必要性消掉了。）

### 5.3 时序契约（唯一要守的东西）

> **宿主 MUST 在实例建立后、任何读取之前调用 `Birth()`。**

不调用的后果：`Resolve` 第一次跑时第 ⑤ 步仍会写切片，行为**静默退回惰性**。这是必须守的契约，不是可选优化。

### 5.4 默认 `Bind` = `Born` 的理由

| | `Born`（默认） | `Live` |
|---|---|---|
| **忘配的后果** | 这一发少跟随一次中途变化，**下一发自愈** | 这一发被中途变化打扰，**行为随读取时序变** |
| 配错的发现成本 | 低（表现为「慢一拍」） | 高（表现为「有时对有时不对」） |
| 典型参数 | 伤害 / 时长 / 射程 / 消耗——绝大部分 | 引导技持续跟随某 debuff 的少数 |

三条理由：

1. **默认值要让「忘记配置」的后果更轻。** `Born` 忘配只是慢一拍；`Live` 忘配是行为错乱且与读取顺序有关。
2. 参数行的主流用法是「这一次用一个数」，真正需要中途跟随的是少数。
3. 与已经裁定的**急切出生**配套：出生本来就要算全部键。

**代价**：出生会计算全部 `Born` 键。某技能只读 3 个却配了 30 个，那 27 个白算。

---

## 6. 引用与环

### 6.1 表达式可以引用同一份定义内的另一个参数

**【已裁 m02181】** 用户原话：「参数表达式 确实需要引用 SourceObject 内的 另一个 参数」。

**边界**：引用**只在同一份 `Defs` 内**，MUST NOT 跨资产。这里的「表」指那批参数定义本身，**不是** §4.3.2 里被删掉的那个运行时查数旁路。

判据（【推导】）：跨资产的需求用**修正器**表达——buff 往账本写 `{Key, Op, Value}`，按 `Key` 命中，天然不需要依赖边。允许表达式跨资产引用 = 在参数表达式里藏**隐式依赖边**，而「因果链 / 溯源边」在 V2 是明确不建的东西（`01-定位与判据.md` §1.2）；并且「A 是否需因 B 变化而重算」不再是纯函数能回答的问题。

### 6.2 走查一：三行参数

```yaml
Params:
  - Key: DamageBase  Base: Literal(100)                    Bind: Born
  - Key: DamageRate  Base: Literal(1.5)                    Bind: Born
  - Key: DamageOut   Base: Mul(ParamRef(DamageBase), ParamRef(DamageRate))  Bind: Born
```

| 时刻 | 动作 | 切片 | 读数 |
|---|---|---|---|
| T0 出生 | `Resolve(DamageOut)` → 切片无 → `InFlight={DamageOut}` → 递归算出 100、1.5 → 150 入切片 | `{DamageBase:100, DamageRate:1.5, DamageOut:150}` | — |
| T1 | 阶段伤害读 `DamageOut` | 命中 | **150** |
| T2 | buff 给 `DamageBase` +20% | 不变 | — |
| T3 | 再读 `DamageBase` / `DamageOut` | 命中 | **100 / 150**（冻住） |
| T4 | 第二次施法，新建切片 | `{DamageBase:120, …, DamageOut:180}` | **180** |

> 这次走查成立的前提正是 6.1 的裁定；若不允许表达式引用另一参数，`Resolve(DamageBase)` 会 miss 并落到兜底值。

### 6.3 环：配置期检测 + 运行期兜底

**【已裁 甲｜m02181】** 用户原话：「环的处理方式，我也倾向用【配置期检测 + 运行期兜底】」。

| 时机 | 做法 |
|---|---|
| **配置期** | 对每个键做 DFS，看是否回到自身 ⇒ 是则报错（资产校验期） |
| **运行期** | `InFlight` 兜一次：命中已在栈上的键 ⇒ 返回 `0.0` + 打 **Error** |

运行期兜底**不是**为了容纳配置错误（配置期已经拦了），是为了「配置期漏检时不要崩、不要死循环」。

**【已裁 甲｜m05106】** 用户原话：「我也倾向甲，不过不应该给 Warning，我觉得应该给 Error」。⇒ 命中 `InFlight` ⇒ 返回 `0.0` + 打 **Error**。

**为什么不做得更精细**：R4 已裁「表达式只能引用**同一份** `Defs` 内的另一个参数」，而同一份 `Defs` 内的环在配置期 DFS 里是**完备可检**的 ⇒ 运行期这条路径正常永不触发，它只需要满足 R5 的「不要崩、不要死循环」，不需要更精细的返回值。返回「上一个切片值」更糟：出生那一刻根本没有上一个值。

⚠️ **连带后果，也是它必须是 Error 的原因**：返回的 `0.0` 会参与上层计算，并按 `Bind == Born` 被**写进切片**——这一发的该键从此就是 0。Warning 会被日志淹掉，Error 才能让它在开发期就被看见，而不是变成一条「伤害莫名其妙是 0」的工单。

---

## 7. 引用链上的混合行为

**【推导】** 这一节是从已裁的 4.1（切片即常量）直接推出来的，用户尚未回应。

### 7.1 情形一：`Born` 引用 `Live`

```yaml
DamageBase  Literal(100)              Bind: Live
DamageOut   ParamRef(DamageBase)      Bind: Born
```

| 时刻 | 读 `DamageBase` | 读 `DamageOut` |
|---|---|---|
| 出生 | — | 递归取 100 → **100 入切片** |
| 中途 +20% 打 `DamageBase` | 重算 → **120** | 切片 → **100** |

⇒ 下游 `Born` 把上游 `Live` **在求值那一刻的值**固定了。合理。

### 7.2 情形二：`Live` 引用 `Born`

```yaml
DamageBase  Literal(100)              Bind: Born
DamageOut   ParamRef(DamageBase)      Bind: Live
```

| 时刻 | 读 `DamageBase` | 读 `DamageOut` |
|---|---|---|
| 出生 | **100 入切片** | 出生不算它 |
| 中途 +20% 打 `DamageBase` | 切片 → **100** | 每次重算，但上游恒 100 → **100** |

⇒ 下游**确实每次重算**，但每次重算读到的上游是常量，所以它的结果**也是常量**。

### 7.3 由此推出一条不变量

> **切片里的一格写进去就是常量。`Live` 的「活」只覆盖它自己这一层——它自己的表达式，加上挂在它自己 `Key` 上的修正器。上游一旦是 `Born`，那条支路就是常量。**

推论：**一个 `Live` 键，若它的上游闭包里存在 `Born` 键，它对上游的「跟随」是假跟随。**

这条**不是错误**：它多半表示作者意图与配置不符，而不是配置非法。它和环检测在**同一趟配置期遍历**里就能算出来（求可达集时顺带知道闭包里有没有 `Born`），成本接近于零。

**【未裁 N2】** 要不要加这条配置期提示（提示 / 静默 / 报错）。

### 7.4 一条必须明说的推论

`Resolve` 的第 ① 步查切片、第 ⑤ 步写切片，都只是 `if (Born)` 保护的一段。

> **Entry 层没有切片 ⇒ 那两步都不会执行 ⇒ 一切实时。**

同一份 `Defs`，面板读出 120、这一发读出 100，**区别不在 `Bind`，而在调用时有没有提供切片**。`Bind` 决定的是「有切片时哪些键进切片」，它不改变「没有切片时一律实时」。

⇒ 两条通道自然存在，且是同一份配置的两个投影：

| 通道 | 调用形态 | 语义 | 典型调用方 |
|---|---|---|---|
| **条目通道** | `Resolver{Defs, State, /*Born=*/nullptr}` | 「这个技能**此刻**是多少」 | 技能面板 / UI / 伤害流程取基准 |
| **实例通道** | `Resolver{Defs, State, Born}` + `Birth()` | 「这一发**当时**是多少」 | 技能机读前摇 / 开关 / 链内引用 |

---

## 8. 未裁清单

> 本节只列**当前确实悬空**的问题。已裁的见 §9。N3（运算枚举归谁）与 N4（布尔开关）已裁，分别落在 §3A 与 §3.4；N7（与属性域的接缝）与 N12（属性修正器的冻结位）已裁，都落在 §4.5；N9（派生环境机制）与 N11（等级从哪来）已裁，分别落在 §4.4 与 §4.3.5；**N5 已裁（R25），落在 §4.3.7**；**N13 已裁（R27），落在 §4.5**；**N6 已裁（R29），落在 §3.3.1**；环境判据（什么该进环境）已裁（R23/R24），落在 §4.3.1 与 §4.3.4；**N8 已裁（R30）**——族内 `Attribute.md` §7.2 / §7.3 已按 R21 / R22 / R27 重写落盘；**N1 已裁（R31），落在 §6.3**；**N10 已注销（R28）**——禁掉镜像键之后它的问题不诞生。

| # | 问题 | 影响面 | 我的倾向 |
|---|---|---|---|
| **N2** | 「假跟随」要不要配置期提示（§7.3） | 作者意图与配置不符时能否被发现 | 倾向提示、不报错 |

---

## 9. 已裁一览（从零重做之后）

| # | 裁决 | 用户原话 / 确认方式 |
|---|---|---|
| **R1** | **域范围 = 甲：四块全要**（表达式 / 环境 / 时机 / 命名值账本） | 「基本就是甲」（m02166） |
| **R2** | **两对象劈开**：参数状态挂最长生命周期（Entry），出生切片挂每次读值上下文（Instance） | 用户就「Entry 还是 Instance 持有」提问后，接受「两个时刻各建不同东西」的结论（m02176） |
| **R3** | **求值环境给求值器，不给值表** | 「我目前也倾向使用【求值器】」（m02177） |
| **R4** | **表达式可引用同一 `Defs` 内的另一参数**；跨资产用修正器表达 | 「参数表达式确实需要引用 SourceObject 内的另一个参数」（m02181） |
| **R5** | **环 = 配置期检测 + 运行期兜底** | 「环的处理方式，我也倾向用【配置期检测 + 运行期兜底】」（m02181） |
| **R6** | **出生 = 急切：实例建立时一次算完** | 「出生是"实例建立时一次算完"（甲）」（m02185） |
| **R7** | **`Bind` 默认 = `Born`** | ask_user_question 选「Born（出生即固化）」（m02218） |
| **R8** | **运算枚举归核心基线**：参数域复用 `ETcsAggregateOp` 与 `ETcsOverrideStrategy`，不另立枚举、不另写折叠器 | 族内 `数值聚合器.md` §3 已 ✅ 全裁，无需再裁，见 §3A |
| **R9** | **布尔独立代数，只记覆盖**（B1） | 「参考乙，但是冲突解决方案我觉得还是用多重方案比较好，就像运算符 Override 一样，有 Newest、Oldest、Priority 等方案」 |
| **R10** | **布尔策略三值** `LatestApplied`/`EarliestApplied`/`HighestPriority`，**不提供 OR/AND**（B2） | 选「丙：只三值」 |
| **R11** | **布尔策略住开关行**，每行一列（B3） | 选「甲：每个开关行自己配一列」 |
| **R12** | **`HighestPriority` 打平比施加序，较新者胜**（B4） | 选「甲：Priority 大者胜，打平比注册序」 |
| **R13** | **布尔写入通道现在建**，与数值对称（B5） | 选「甲：现在建，与数值对称」 |
| **R14** | **环境里装 `WorldContext`**：世界级子系统经它按类型取，不各占字段 | 「它是 WorldSubsystem啊，只要让 TcsParamEvaluateContext 中新增一个 WorldContext 就可以了」（m02834） |
| **R15** | **环境里不装外部参数表**：引用型源向求值器要值 | ask_user_question 选「甲：上下文只留 WorldContext / 主体 / Instigator / Level，删 ParamTable」（m02955）；主体字段现名 `Target`，见 R24 |
| **R16** | **宿主扩展入口收敛为「提供一种 `FTcsParamSource` 派生」** | R15 的直接后果，见 §4.3.2 |
| **R17** | **参数键不需要在参数表里有一行**：折叠的基数既可以来自同键的参数行表达式，也可以来自调用方传进来的 `double`（**运行时基数**，不是别的域的字段——见 R28）。两种用法共用同一份折叠与同一份账本 | 「Q1：甲」（m03179），见 §3.5 |
| ~~**R18**~~ | ~~**环境分两个等级字段**：`EffectiveLevel`（这个东西多强）与 `InstigatorLevel`（那个人多强）。读「施加者等级」的源改读字段，不再自己去问等级提供者~~ ⇒ **已被 R23 取代**：后半（`InstigatorLevel` 也进环境）撤销，理由（「源不需要认识宿主」）被 R19 消解——取等级搬进 `ITcsEntityQuery` 之后，源问契约，不问宿主 | 「Q2：甲」（m03179），见 §4.3.4 |
| **R19** | **等级进战斗实体契约**：`ITcsEntityQuery` 加 `TryGetEntityLevel(Entity, int32& Out)`；签名为 `bool` + 出参（「取不到」与「就是 0 级」可区分）。**实体等级一律问契约**；`EffectiveLevel` 由装配方填（它不是实体等级，是「这个东西」自己的档位，见 R23）。取等级是「读世界」，与 R16 的「读参数值」是两个各自封闭的口，不构成推翻 | 「我个人倾向于 方案【丁】…可以说是从宿主需求中提炼出来的通用功能」（m03403）；签名选「甲」（m03431），见 §4.3.5 |
| **R20** | **派生环境机制整机制退场**：源只认一个环境结构体；两个派生环境、基类「自报类型」虚函数、等级提供者接口及其注入面、各源的类型判定分支一并退场 | ask_user_question 选「甲：整机制退场」（m03431），见 §4.4 |
| **R21** | **属性修正器持「值描述」，不存算好的数**：属性域要支持「同实体 A 属性变化触发 B 属性重算」，依赖边靠「修正器自己说得出它读了谁」建立，值一旦算成数该信息就没了。属性域原有的运算数种类退场，只留「一份表达式 + 运算带 + 优先级」 | 「同一实体内部『属性 A 变化要触发属性 B 重算』这个需求 是属性域必须支持的硬性需求，所以应该选择乙方案」（m03526），见 §4.5 |
| **R22** | **属性修正器不带冻结位**：A（跟随）与 B（定死）由值描述自身表达——引用参数键归该键的 `Bind`，引用属性天然跟随，要定死则施加方先算成常量。另：「把一份值描述求成一个数」的唯一入口是 `FTcsParamValue::Evaluate(Env, R)`（R34，§4.2.2），复用求值器的环境与环检测，MUST NOT 另造源族自由函数或 `ResolveExpr` | 「这个功能不是已经被 Parameter 自己的 Born 语义取代了吗，所以应该是 方案【甲】吧」（m03601），见 §4.5 |
| **R23** | **环境只放「能被句柄指到的参与者」的句柄，不放它们的任何值；只有「句柄指不到」的东西才配占一个值字段**。⇒ `InstigatorLevel` 退场（施加者的等级问 `ITcsEntityQuery`，§4.3.5）；`EffectiveLevel` 留下（技能 / buff / Area 不是 `FTcsCombatEntityHandle`，结构上没有句柄）。**撤 R18 的后半** | 「1.5 我认为没问题」（m04671），见 §4.3.1 与 §4.3.4 |
| **R24** | **`Target` 取宽义**：「这份参数为谁求值」，不限于「被攻击者」。它是 `Subject` 的改名，并让参数域与属性域词汇对齐（属性域早有 `Target` / `Instigator` 两侧） | 「Target 语义，可以按照你说的，取宽义（「这份参数为谁求值」）」（m04671），见 §4.3.1 与 §4.3.3 |
| **R25** | **无 `Key` 的值：冻结归消费点自己**（N5 = 甲）。`Bind` 只配在参数行上、切片按 `Key` 索引，无 `Key` 的值结构上放不进去。⇒ 消费点自己两行：Def 上一个 `ETcsParamBind`、实例上一个 `double` | 「N5，暂时可以先这么裁决，其实 Area 之后具体如何实现还不好说」（m04671），见 §4.3.7 |
| **R26** | **等级进表达式 = `FTcsParamSource_ContextLevel`**，三值 `Instigator` / `Target` / `StateSelf`。`StateSelf` 读环境字段 `EffectiveLevel`，另两值问战斗实体契约。不带 `Coefficient`（乘法交外层 `Composite{Mul}`） | 「ContextLevel 第三个枚举值叫什么，我还是倾向 State，不过可以和你的结合一下，叫 StateSelf」（m04671），见 §4.3.6 |
| **R27** | **属性修正器的值描述 MUST NOT 读参数账本**：允许的源只有字面量 / 读属性 / 两者的组合。要「此刻的参数值」就在 `AttachMod` 之前取一次、当字面量传。判据：属性域缓存正确性完全靠依赖边，而建边只认属性词汇——参数键混进 `Operand` 抽不出边且不报错；且无真实消费者 | 问答自定义回答「先不能吧，我暂时也没想到 真实业务案例。」（2026-11-19），见 §4.5 |
| **R28** | **参数键 MUST NOT 镜像别的域的状态字段**：参数键装「施加给别人的改动量」，不装「别人身上那个值」。被改的东西留在它自己的域里，由那个域自己的入口承受改动（建 `CooldownReduction = -2`，不建 `CooldownDuration`；建 `StackBonus = +1`，不建 `StackCount`）。⇒ **注销 N10**：那条问题只在镜像键存在时才有意义 | 「我强烈反对 创建一个 对应的 参数键 Tag…这个 ParamMod 在哪儿配置；我能想到的，就是后续要落地的 SkillModifier」（m04864 / m04879），见 §3.5.1 |
| **R29** | **账本按来源摘除**（N6）：`RemoveBySource` 是唯一撤销入口，撤销的最小单元是「一次施加」而非一个键。六条契约——只做全摘；返回摘掉条数、`0` 不报错；配对责任在施加方（参数域不追踪来源存活，漏调 = 静默残留）；`Add` 覆写 `OrderSeq = Source.Id` 并拒绝无效 `Source`；撤销动不了出生切片；一次施加写多份账本由施加方逐份撤，参数域不建引用计数 | 「这几条我都审核过了，完全没什么问题」（m04949），见 §3.3.1 |
| **R30** | **属性修正器只读本实体（`Side == Target`）**（N8 的第二半）：`FTcsAttrModInstance` 只持 `FTcsSourceHandle`，而它按《运行时实例》§1.3 是生命周期概念、不携带发出者实体 ⇒ 修正器手上没有任何东西能指到施法者。MUST NOT 为让 `Side == Instigator` 可用而往实例上补实体字段——那会造出一条「读了但不会因它重算」的读，与 R27 同一条判据。跨实体跟随走既有订阅路径（`DetachMod` + `AttachMod`）。⇒ 属性域的求值环境里 `Instigator` 恒无效、`EffectiveLevel` 恒 `0` | 「我也想不到 Side == Instigator 的 应用场景，先用你倾向的 甲 方案吧」（m05046），见族内 `领域-Attribute-草稿.md` §7.2.1 与 §5.1。**场景后由 m05152 补上**（友军身上挂 buff、加成跟施法者移速走），**结论不变、理由替换**：从「想不出场景」改为「本域的求值环境与依赖图都给不了它」，见 R32 |
| **R31** | **运行期兜环：返回 `0.0` + 打 Error**（N1 = 甲）。配置期 DFS 对同一份 `Defs` 是完备的（R4 限定表达式只能引用同一份 `Defs`），所以这条路径正常永不触发，只需要「不崩 + 有声音」。MUST NOT 返回「上一个切片值」（同一份 `Defs` 不同发次数值不同，且出生期没有上一个值） | 「我也倾向甲，不过不应该给 Warning，我觉得应该给 Error」（m05106），见 §4 第 ② 步与 §6.3 |
| **R32** | **属性修正器不跨实体读属性；跨实体的「跟随」走订阅**。`FTcsParamSource_AttributeScaled` 的 `Side` 在修正器的 `Operand` 里只能取 `Target`。要「施法者的属性变 ⇒ 我身上的修正器跟着变」，唯一路径是 D15：施加方订阅那条属性的变更，回调里 `DetachMod` + `AttachMod` 重新算成字面量挂上。三条理由：① `FTcsAttrModInstance` 只持 `FTcsSourceHandle`，按《运行时实例》§1.3 它是生命周期概念、不携带发出者实体 ⇒ 修正器手上没有能指到施法者的东西 ② 依赖图 MUST 停在实体边界内（族内 `领域-Attribute-草稿.md` §5.1），跨实体边会让一次属性写入连锁唤醒另一实体结算、递归深度不可控 ③ 跨实体读若允许而不建边，会退化成「读了但不重算」——它能配得出来、跑得通、结果是静默的旧值。⇒ 修正器 `Operand` 里写 `Side == Instigator` 属**配置错误**，配置期校验 SHOULD 拒绝 | 「我接受 Case B 走订阅」（m05171），见 §4.5 与族内 `领域-Attribute-草稿.md` §5.1 |
| **R33** | **源族求值形参 = `Evaluate(const FTcsParamEnv& Env, FTcsParamResolver& R)`**：两个形参对应两件事——`Env` 是「我读什么」（纯数据上下文），`R` 是「我问谁」（递归、环检测、切片命中、`Defs` 全在它手上）。MUST NOT 把 `Defs` 或 `Resolver` 塞进 `Env`：那会让环境从「纯数据快照」变成「求值会话」，`FTcsParamEnv` 与 `FTcsParamResolver` 的职责边界消失 | 「FTcsParamSource::Evaluate 的形参是什么，Context 是要的，你说的 Resolver 也可以加进去，后续可能还会有调整，目前先暂时这么裁定」（m05249），见 §4.2.1 |
| **R34** | **`FTcsParamValue` = `USTRUCT` 包一层 `FInstancedStruct Source`（`meta = (BaseStruct = FTcsParamSource)`），求值入口是这个结构体自身的 `Evaluate(Env, R)`；`FTcsParamResolver::ResolveExpr` 这个名字退场**。不用 `using` 别名（过不了 UHT 类型识别，且「只能装源」的约束消失）；不把求值入口放回求值器（`Env` 与 `R` 已在调用点手上，多一次转手只让主语变远） | 「先用你给的两个方案吧」（m05269），见 §4.2.2 |

---

## 10. 与族内现状的关系

| 族内文档 | 状态 | 本文的影响 |
|---|---|---|
| `01-定位与判据.md` | ✅ 全裁 | **无**。本文的四条铁律引用照旧有效 |
| `INDEX.md` §2.2 / §4 | ✅ 全裁 | **无**。§2.2 的判据正是本文 §2.2「Parameter 要缩不是要没」的依据 |
| `02-核心基线/数值聚合器.md` | ✅ 全裁 | **无**。七带折叠是本文 §4 第 ④ 步直接复用的算术 |
| `02-核心基线/战斗实体.md` | ✅ 全裁 | **有，且需用户点头**：R19 要在 §3 的契约上**新增一支** `TryGetEntityLevel(Entity, int32& Out)`（既有两支 `EnumerateEntities` / `GetLocation` 不动）。它是**实体等级的唯一来源**（R23）：环境里的 `EffectiveLevel` 不是实体等级，不进这条契约。另有 ⚠️ 待定：`ITcsEntityQuery` 从 `Env.WorldContext` 怎么取到实例（§4.3.6）。⚠️ 现有代码里另有 `IsAlive` / `IsEntityReady` 两支，属待废弃的旧实现——族内 §7 边界清单已把它们判为「不进本域」，文档 MUST NOT 据代码补齐它们 |
| `领域-Attribute-草稿.md` | ✅ 全裁 | **已落盘（R30 / N8）**：§7.2 删掉 `ETcsParamMode Mode` 与 `SnapshotValue` / `bHasSnapshot` 两个冻结位，`EvalModValue` 改为只做 `Operand.Evaluate(Env, R)`（形参同 R33，属性域一并收 `FTcsParamResolver&`）；§7.3 整节重写为「修正器求值：没有冻结位」；新增 §7.2.1「求值环境从哪来」；§5.1 的 `Side == Instigator` 行由「不进图」改为「不允许」。属性修正器仍持值描述（R21），只是不再有任何模式位 |
| `领域-Attribute-草稿.md`（**跨域依赖**） | ✅ 已具备 | 本域的 `FTcsParamSource_AttributeScaled` 读属性走的是族内 §8.1 既有的 `bool TryGetValue(Entity, Attribute, double& OutValue) const`（「返回 `CachedCurrent`。不重算、不发事件」）。**无需为本域新增任何读口**（见 §4.3.3） |

---

## 11. 下一步

按用户裁定的工作方式（「调研 → 记录 → 逐条确认 → 最后裁架构」），**N8 已落盘**；**N1 已裁（R31）**；剩下 **N2** 一个口径。已裁：N3、N4（§3A、§3.4）、N7（§4.5）、N9（§4.4）、N11（§4.3.5）、N12（§4.5）、N5（§4.3.7）、N13（§4.5）、**N6（R29，§3.3.1）**、**N10 已注销（R28，§3.5.1）**；另环境判据已裁（§4.3.1 与 §4.3.4）。

**只剩 N2 一件**：假跟随要不要配置期提示。它不阻塞任何既有形态，可随时插入。

⚠️ 关于已裁的 N13：它的理由是「值描述的源可以是哪些」决定属性域那些「靠依赖边维持的缓存」会不会被一类抽不出边的读法污染——**不是「会成环」**，跨域读成不了环，只会形成一条单向的、不被维持的读。

---

## 附：归档文件里仍然有效的东西

| 内容 | 为什么仍然有效 |
|---|---|
| §11 裁决流水 **D1–D50**（含用户原话） | **裁决记录**，不是设计推导。`D42–D46` 作废也必须在案 |
| **全部 `文件:行号` 实证**（§0 现状清点、§7A 折叠器归属、§8 调研实证、§12/§13/§15） | **事实**，不因为设计换了就失效 |
| §10 确认清单第 1–7 条（`ETcsParamMode` 与载体正交、折叠器归基线、域与模块两个轴、源族保留集、`HostDelegate` 删除、`AttributeScaled` 读取侧、`Composite` 运算集） | 这七条与「冻结机制」无关，**未被本次重做触及**，仍然有效 |
