# 领域 · Attribute（属性）

> **定位**：V2 的第一个领域文档。属性是数值地基——它把「一堆修正」变成一个**可读的数**，并管理这个数的生命周期。
> **回答什么问题**：一个实体有哪些属性、每个属性的值怎么算出来、什么时候通知别人它变了。

**审核状态图例**：✅ 已裁决并批准 ｜ ⚠️ 未定 ｜ ❓ 待调研

---

## 0. 它是什么 ✅

**一句话**：属性是**挂在实体上的有名字的数值**，其值 = 基值 + 一组修正器，经《数值聚合器》折叠得出。

**三个不变量**：

| # | 不变量 |
|---|---|
| ① | **身份稳定**：一个实体上的属性由 `FGameplayTag` 标识，`DefTag` 永不复用 |
| ② | **值可重算**：任何时刻都能从「基值 + 修正器 + 值域」重算出同一个结果（纯函数 + 定序累加） |
| ③ | **变化可观察**：值真的变了 MUST 能被观察到（见 §6） |

**它不是什么**：

- 不是容器——属性值不是随便读写的寄存器，读写都要经过 §8 的读口与 §7 的施加路径
- 不是公式——折叠规则全在《数值聚合器》，属性域只负责「收集条目、交给它、收口」
- 不是状态——Buff/State 有自己的实例，属性只是它们作用的**目标**

---

## 1. 三段式结构 ✅

**定义 / 实例 / 容器**三层分离：定义是常量，实例是每实体状态，容器是实体的属性表。

### 1.1 定义：`UTcsAttributeDef`

```cpp
UCLASS()
class TCSATTRIBUTE_API UTcsAttributeDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// 属性身份。主资产身份 = [PrimaryAssetType, DefTag.GetTagName()]
	UPROPERTY(EditAnywhere, meta = (Categories = "Attr"))
	FGameplayTag DefTag;

	// 两段初始基值（#52 定死两段：基础 / 额外）
	UPROPERTY(EditAnywhere) double BasicValue = 0.0;
	UPROPERTY(EditAnywhere) double ExtraValue = 0.0;

	// 值域：边界 + 越界语义
	UPROPERTY(EditAnywhere) FTcsAttributeBounds Bounds;
	UPROPERTY(EditAnywhere) ETcsValueDomainMode ValueDomain = ETcsValueDomainMode::Clamp;

	// 覆盖带决胜策略（《数值聚合器》§3 的七选一）
	UPROPERTY(EditAnywhere) ETcsOverrideStrategy Strategy = ETcsOverrideStrategy::MaxValue;

	// 能否被运行时停用。false ⇒ 常驻属性，RemoveAttributeSet 拒绝把它减到 0
	UPROPERTY(EditAnywhere) bool bRemovable = true;

	// 派生表达式（可选）：本属性的值还包含这个表达式的结果。
	// 多源不靠多个字段，靠参数域的 Composite 源（见下）
	UPROPERTY(EditAnywhere) bool           bDerived = false;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bDerived"))
	FTcsParamValue DeriveExpression;
};
```

> **不含 `FTcsAttributeDefData` 中间结构**：V1 抽出它是为了让 DataTable 行与资产共享字段声明；V2 取消 DataTable 后这个理由消失，字段直接住在资产上。

> **`DeriveExpression` 是 `FTcsParamValue`，它是多源的**：`FTcsParamValue` 装的是一个 `FInstancedStruct` 源，源可以是单条 `AttributeScaled`，也可以是 `Composite`（`Terms: TArray<FTcsParamValue>`，可嵌套）。

**一个字段就够，不需要属性域另造组合结构**：

```text
单源   Attack = Strength × 2
       DeriveExpression → AttributeScaled{ Target, Attr.Strength, 2.0 }

多源   Attack = Strength × 2 + Agility × 0.5
       DeriveExpression → Composite{ Op = Add, Terms = { AttributeScaled(…×2.0), AttributeScaled(…×0.5) } }
```

> **Why Not 属性域自己展开成 `TArray<FTcsParamValue> DeriveTerms` + `Op`**：那会让属性域拥有一套与参数域 `Composite` 同义的组合语义——技能域、状态域将来遇到「多值组合」时又得各造一份。**组合能力归参数域，属性域只声明「这里有一个表达式」**。完整写法见 §7.4。

> 与修正器模板不同——`FTcsAttrModDef` **MUST NOT** 持有 `FTcsParamValue`（它被状态/技能/装备/纯代码共同使用，绑死参数域会锁死泛用性）；`UTcsAttributeDef` 只被属性域使用，可以。

### 1.2 实例：`FTcsAttributeInstance`

```cpp
struct FTcsAttributeInstance
{
	// 身份 + 定义引用。DefTag 用于快照/复制，Def 指针用于运行期读字段
	FGameplayTag                 DefTag;
	TObjectPtr<UTcsAttributeDef> Def;      // 需子系统 AddReferencedObjects 追踪

	// 两段基值。这是实例自己的状态（Def 里只是初值）
	double BasicValue = 0.0;
	double ExtraValue = 0.0;

	// 求值缓存：段内公式结果 / 全局层 + 收口之后。只有求值路径（§3.4）写它
	double CachedSegment[2] = { 0.0, 0.0 };   // [0]=Basic / [1]=Extra
	double CachedCurrent    = 0.0;

	// 生命周期
	int32  RefCount = 0;                      // 有多少活跃的 ApplyAttributeSet 需要它
	bool   bActive  = false;                  // RefCount > 0

	// 运行期解析缓存（重建依赖图时重填）
	int32  DynamicTargetSlot = INDEX_NONE;

	// 修正器条目（含 §7.4 的派生内置条目）
	TArray<FTcsAttrModInstance> Mods;
};
```

> **`Def` 与 `DefTag` 并存不是冗余**：`Def` 让热路径读字段不必再查表；`DefTag` 是快照/复制时的唯一身份（指针不可序列化）。
> **`bRemovable` / `ValueDomain` / `Bounds` / `Strategy` 不拷贝到实例**：直接读 `Def->`。它们对同一份定义恒定，拷贝只会制造两份真相。

### 1.3 依赖图：节点是「求值单元」，不是属性

**这是本域最关键的结构决定。** 若图的节点是属性（槽位），「额外生命 ↔ 法术强度」这类互相引用就是环（GAS 到此为止：跑 10 层掐断、值可能错）。

**把节点降到修正器级，环就消失**——因为真实的值依赖链是无环的：

```text
HP 的 Extra 段 = 折叠(HP.ExtraValue, M1装备+100, M2符文+50, M3由AP转化)
AP 的 Extra 段 = 折叠(AP.ExtraValue, M4由HP的Extra段转化)

M4 读的是「HP 的 Extra 段**排除 M3 后**」的值 ⇒ M4 不依赖 M3。
图：M1,M2 ─→ M4 ─→ AP.ExtraSeg ─→ M3 ─→ HP.ExtraSeg   （无环）
对照：M4 若读「HP 总值」⇒ 依赖 M3 ⇒ 有环（这正是必须按标签排除的原因）
```

#### 节点种类

```cpp
UENUM()
enum class ETcsEvalNodeKind : uint8
{
	SegmentBase,    // 一个属性某一段的基值（无输入；它本身就是权威状态）
	Modifier,       // 一条修正器的求值（输入 = 它的 Operand 读到的值）
	Segment,        // 一个属性的某一段的折叠结果（输入 = 段基值节点 + 该段全部修正器）
	Total,          // 一个属性的总值（输入 = 两段结果 + 全局层修正器 + 值域边界）
};

struct FTcsEvalNode
{
	ETcsEvalNodeKind Kind   = ETcsEvalNodeKind::Modifier;
	int32            Slot   = INDEX_NONE;                // 所属属性槽位
	ETcsAttrSegment  Segment = ETcsAttrSegment::Basic;   // Kind == Segment 时有效
	int32            ModIndex = INDEX_NONE;              // Kind == Modifier 时：Inst.Mods 的下标
};
```

#### 容器的三层职责保持不变

```cpp
struct FTcsAttributeGraph
{
	// 节点表：Nodes[i] 是第 i 个求值单元。**图自己排序、自己找边，调用方只给边列表**
	TArray<FTcsEvalNode> Nodes;

	// 反向边：Dependents[i] = 依赖节点 i 的那些节点下标
	TArray<TArray<int32>> Dependents;

	// 拓扑序（节点下标序）。MUST 稳定（§5.4）
	TArray<int32> EvalOrder;

	bool bDirty = false;

	bool Rebuild(TConstArrayView<FTcsEvalNode>  InNodes,
	             TConstArrayView<FTcsEvalEdge> InEdges);
};

// 一条依赖边：From 变化 ⇒ To 要重算。节点用下标标识
struct FTcsEvalEdge
{
	int32 FromNode = INDEX_NONE;
	int32 ToNode   = INDEX_NONE;
};

struct FTcsAttributeStore
{
	// 内容层
	TArray<FTcsAttributeInstance> Instances;   // 下标 = 槽位（见下注）
	TMap<FGameplayTag, int32>     TagToSlot;

	// 结构层
	FTcsAttributeGraph Graph;

	// 时序层
	int32         BatchDepth = 0;
	TArray<int32> BatchWrittenNodes;    // 待重算的**节点**下标（不是槽位）

	// 缓存：按节点存结果
	TArray<double> NodeValues;          // 与 Graph.Nodes 对齐
};
```

> **`Instances` 的下标就是槽位**：`Slot` 是寻址层的概念（数组位置），`Instance` 是内容层的概念。**代码落地时 MUST 在 `Instances` 的声明处写明这条对应关系。**

> **Why 节点表与边表分开**：`Rebuild` 只做拓扑排序与环检测，不认识属性、标签、值。**建图逻辑（谁依赖谁）归属性域，排序归图**——两者可以各自验证。

> **Why 缓存按节点存**：段值、总值、每条修正器的值都是图上的节点，各自有结果。按槽位存不够——同一个属性的 Basic 段与 Extra 段是两个不同的量。

---

## 2. 值的权威与缓存 ✅

### 2.1 权威是「基值 + 修正器」，不是缓存

| 东西 | 权威性 |
|---|---|
| `BasicValue` / `ExtraValue` | **权威**——这是属性自己的状态，只有 §7 的写入路径能改 |
| `Mods` | **权威**——修正器条目的集合 |
| `CachedCurrent` / `CachedSegment` | **派生缓存**，任何时候都能重算出来 |

> **缓存值 MUST NOT 被外部直接写**。它只有一个生产者：求值路径（§3.4）。

### 2.2 写时重算，帧末派发

**写入路径（§7）成功后**——同步，全在同一个调用栈内：

| 当前是否在批内 | 行为 |
|---|---|
| **批外**（`BatchDepth == 0`） | **立即结算**：重算该属性 + 沿 `Dependents` 按拓扑序传播（§5.3），返回时值已是最新 |
| **批内**（`BatchDepth > 0`） | 只把**节点**登进 `BatchWrittenNodes`，**不算**；等 `CommitBatch` 归零时一次结算 |

**读路径（§8）**：直接返回 `CachedCurrent`，**不重算、不发事件、零副作用**。批内读到的就是批开始前的值。

> **批内读到旧值是有意的**：批的语义是「这一批改动要么一起生效、要么都还没生效」。让外部读到「一半生效」的中间态，只会让调用方更难推理。

**事件路径**：结算时 `New != Old`（阈值 `1e-5`）⇒ 按 `(Entity, Attribute)` 记一条待发变更；所有待发变更在帧末按实体归组合并派发（§6.4）。批内对同一属性的多次改动合并成一条，保留首次 `Old`、末次 `New`。

```cpp
// 批：嵌套计数。真实消费者 = 等级成长这类宿主升级事务（§7.6）
void BeginBatch(FTcsCombatEntityHandle Entity);   // BatchDepth++
void CommitBatch(FTcsCombatEntityHandle Entity);  // BatchDepth--；归零时才 SettleWrites(全部 BatchWrittenNodes)
```

> **Why Not 批内也立即算**：一次升级改 5 个属性的基础值，逐条立即结算 = 5 次全图重算 + 5 次广播；批内只算一次、每属性最多广播一次。

> **Why Not 把重算推到帧末**：链步骤是**同步执行**的（`ApplyAttributeSet` → 触发行求值 → 行为链再入起链，全在同一帧同一调用栈内）。重算若推迟到帧末，**同一条链里「先写、后读」就会读到旧值**：例如「先给目标加 50 攻击力，紧接着按攻击力算伤害」，伤害会用加之前的攻击力。**批的边界由调用方显式声明，帧末不是边界**。

> **Why Not 读时惰性重算**：`AttributeScaled` 会跨实体读（`Instigator` / `Target` 两侧），读路径触发重算等于让一次读取连锁唤醒另一侧的求值，递归深度不可控；且 `Evaluate` 可能落到宿主源（`HostDelegate`），宿主在回调里再读属性就会**求值中途重入**。计算成本放在写侧，读侧保持纯内存返回。

---

## 3. 聚合接线（对接《数值聚合器》）✅

### 3.1 两段 + 全局层

```cpp
// 段 = 基值所在的槽位。只有两段，没有 Global（全局层没有基值）
UENUM()
enum class ETcsAttrSegment : uint8
{
	Basic,   // BasicValue
	Extra,   // ExtraValue
};
```

> **与 §3.2 的 `ETcsAttrLayer` 不是一个东西**：`Layer` 是**修正器落在哪**（三段，含 `Global`），`Segment` 是**基值存在哪**（两段）。`Global` 层没有基值，所以 `SetBaseValue`（§7.6）不收 `Layer`。

| 层 | 对应 |
|---|---|
| `Basic` 段 | `Segments[0]`，基值 = `BasicValue` |
| `Extra` 段 | `Segments[1]`，基值 = `ExtraValue` |
| 全局层 | `GlobalEntries` |

**段数定死为 2**，不开放多段：LAC 的 ARPG 公式只需要「基础 / 额外」两段，多段没有真实需求。**段序无语义**——段之间是求和。

### 3.2 修正器的落点：`ETcsAttrLayer`

```cpp
UENUM()
enum class ETcsAttrLayer : uint8
{
	Basic  UMETA(DisplayName = "基础段"),
	Extra  UMETA(DisplayName = "额外段"),
	Global UMETA(DisplayName = "全局层"),
};
```

**为什么需要它**：`PercentAdd` 在段内与全局**都合法**，光看 `Op` 分不出落点。

### 3.3 合法矩阵（`IsDataValid` 直接查）

| Layer | 允许的 `Op` |
|---|---|
| `Basic` / `Extra` | `Add`、`PercentAdd`、`OverrideBase`、`OverrideSegment` |
| `Global` | `PercentAdd`、`Mul`、`FlatAdd`、`OverrideGlobal` |

矩阵外 ⇒ 报 `Error`，判定逻辑随 `UTcsAttributeDef` / `UTcsAttrModDef` 各自实现。

> **段保持同构，`Mul` 只留全局层**：调研 LoL 与 War3 后确认——两家的百分比加成一律作用于**总值**（`maximum health`、`total AP`），不存在「只放大 bonus 部分」的属性加成。base/bonus 之分的用途是**被技能引用**（「60% bonus AD」），那是 §8.2 的读口，不是运算。

### 3.4 收集与求值

**本节只讲「怎么把修正器交给聚合器」，不讲「什么时候算」。** 求值时机与逐节点调度在 §5.3——**那里是唯一实现**，本节不重复给第二套代码。

#### 收集：按层分组

```cpp
// 收集某一层的聚合条目。Global 层单独一组
void CollectLayerEntries(const FTcsAttributeStore& Store, int32 Slot, ETcsAttrLayer Layer,
	TArray<FTcsAggregateEntry>& OutEntries)
{
	const FTcsAttributeInstance& Inst = Store.Instances[Slot];

	for (int32 ModIndex = 0; ModIndex < Inst.Mods.Num(); ++ModIndex)
	{
		const FTcsAttrModInstance& Mod = Inst.Mods[ModIndex];
		if (Mod.Def->Layer != Layer) { continue; }

		// 值已由 §5.3 的修正器节点算好，这里只取，不重算
		// 节点下标由「槽位 + 该条在 Mods 里的下标」定位（§5.2 BuildNodes 的编号规则）
		const double V = Store.NodeValues[FindNode(Store, Slot, ETcsEvalNodeKind::Modifier,
			ETcsAttrSegment::Basic, ModIndex)];
		OutEntries.Add({ Mod.Def->Op, V, Mod.Def->Priority, Mod.Source });
	}
}
```

> **收集阶段只读节点值**：修正器的值本身是一个图节点（§1.3），由 `EvaluateNode` 在拓扑序里先行算完。收集阶段 MUST NOT 再调一次 `Eval`——否则同一条修正器在一次结算里会被算两遍，而 `AttributeScaled` 的读取不是免费的。

#### 折叠：两段 + 全局层

```cpp
// 段内折叠：段基值 + 该段全部条目（《数值聚合器》§3.1 的 N 段折叠）
double FoldSegment(const FTcsAttributeStore& Store, int32 Slot, ETcsAttrSegment Seg)
{
	TArray<FTcsAggregateEntry> Entries;
	CollectLayerEntries(Store, Slot, SegmentToLayer(Seg), Entries);

	const double Base = Store.NodeValues[FindSegmentBaseNode(Store, Slot, Seg)];

	// 只喂一段、全局层给空 ⇒ 全局层乘积为 1、平坦加为 0，
	// 返回值即该段值（含段级 OverrideBase / OverrideSegment，不含全局层、不含值域收口）
	const FTcsAggregateSegment One[1] = { { Base, Entries } };
	return EvaluateAggregatedValue(One, /* GlobalEntries = */ {}, Store.Instances[Slot].Def->Strategy);
}

// 总值：两段结果 → 全局层 → 值域收口
double EvaluateTotal(const FTcsAttributeStore& Store, int32 Slot)
{
	const FTcsAttributeInstance& Inst = Store.Instances[Slot];

	// 两段的值直接从 Segment 节点取——它们在拓扑序里已算完，**不重折叠**
	// Entries 留空：段值已是折叠结果，再传条目会被折叠第二遍（《数值聚合器》§3 的段基值语义）
	const FTcsAggregateSegment Segs[2] = {
		{ Store.NodeValues[FindNode(Store, Slot, ETcsEvalNodeKind::Segment, ETcsAttrSegment::Basic)], {} },
		{ Store.NodeValues[FindNode(Store, Slot, ETcsEvalNodeKind::Segment, ETcsAttrSegment::Extra)], {} },
	};

	TArray<FTcsAggregateEntry> GlobalEntries;
	CollectLayerEntries(Store, Slot, ETcsAttrLayer::Global, GlobalEntries);

	// 值入口与明细入口共用同一个实现体，见《数值聚合器》§3
	const double Folded = EvaluateAggregatedValue(Segs, GlobalEntries, Inst.Def->Strategy);
	return ApplyValueDomain(Folded, Inst);   // §4
}
```

> **段值 MUST 从 `Segment` 节点取，不能现算**：现算会让同一次结算里 `Segment` 被算两遍（节点一次、总值一次），而且两处若用了不同的条目筛选，缓存与总值就会不一致。

> **为什么 `FoldSegment` 用分段入口而不新增一个「段内折叠」函数**：聚合器没有单独的段内入口——段级覆盖（`OverrideBase` / `OverrideSegment`）与全局层在**同一个实现体**里处理（《数值聚合器》§3「分段求值：唯一实现体」）。喂一段 + 空全局层，全局层的 `ΠMul = 1`、`ΣFlatAdd = 0`，结果**恰好**就是段值。**新增一个 `FoldSegmentEntries` 会让段级覆盖有两套实现，早晚分叉。**

#### 缓存与节点值的关系

| 字段 | 与 `NodeValues` 的关系 |
|---|---|
| `CachedSegment[0]` / `[1]` | = `Segment` 节点（`Basic` / `Extra`）的值 |
| `CachedCurrent` | = `Total` 节点的值 |
| `NodeValues[i]` | **唯一存放处**——所有节点值都在这 |

> **`CachedSegment` / `CachedCurrent` 是读口的具名视图，不是第二份缓存**：它们由节点值同步而来（或直接写成按下标取的 inline 访问器），存在的唯一理由是「读口不该把节点下标这种实现细节暴露出去」。**MUST NOT 出现「节点值变了、Cached 没变」**——要么同批写，要么前者是后者的访问器。

### 3.5 三种读取粒度

`AttributeScaled` 可以只读某个属性的一部分。这不是可选的优化——**「每 40 额外生命 +1 法强」读的必须是 Extra 段**，读总值会算进基础生命。

```cpp
UENUM()
enum class ETcsAttrReadPart : uint8
{
	Total,   // 总值：两段之和 → 全局层折叠 → 值域收口后的 CachedCurrent
	Basic,   // Basic 段值：CachedSegment[0]，不含全局层、不含值域收口
	Extra,   // Extra 段值：CachedSegment[1]，同上
};
```

| 粒度 | 值 = | 用在哪 |
|---|---|---|
| `Total` | `CachedCurrent` | 绝大多数引用（「攻击力的 120%」） |
| `Basic` / `Extra` | `CachedSegment[i]` | 「每 40 **额外**生命」「60% **bonus** AD」这类分基值引用 |

> **两种粒度的值不同，且都正确**：`(Basic段值 + Extra段值) × 全局系数 + 全局平坦加 ≠ Extra段值`。挑哪个由配置的语义决定，聚合器 MUST NOT 代为推断。

> **段值不含值域收口**：`CachedSegment[i]` 是段内公式结果，`Clamp` / `Wrap` 只作用于总值（§4.3）。所以段值读口可能给出**超出边界**的数——这是设计，不是缺陷（§8.2 的「60% bonus AD」不该被总边界钳住）。

> **落地时 MUST 在 `CachedSegment` 的声明处写明这一点**：它存的是段内公式结果，**未经值域收口**，与 `CachedCurrent` 不是同一层的量。少了这条注释，读代码的人会以为「总值被钳过、段值也一定在界内」，从而在「额外生命算出来是负数」这类情形下无从解释。

---

## 4. 值域收口 ✅

### 4.1 边界：`FTcsAttributeBounds`

```cpp
enum class ETcsBoundMode : uint8 { None, Static, Dynamic };

struct FTcsAttributeBound
{
	ETcsBoundMode Mode = ETcsBoundMode::None;
	double        StaticValue = 0.0;         // Static 用；Dynamic 时是解析失败的回退值
	FGameplayTag  TargetAttribute;          // Dynamic 用：读同实体另一属性
};

struct FTcsAttributeBounds
{
	FTcsAttributeBound Min;
	FTcsAttributeBound Max;
};
```

**`Dynamic` 只指向同一实体的另一属性**，解析结果缓存在 `DynamicTargetSlot`（重建依赖图时填）。

### 4.2 越界语义：只两种

```cpp
UENUM()
enum class ETcsValueDomainMode : uint8
{
	Clamp = 0  UMETA(DisplayName = "钳制"),   // 越界钳到边界值
	Wrap  = 1  UMETA(DisplayName = "循环"),   // 按值域跨度循环回卷
};
```

> **Why Not 保留 `Custom`**：V1 的 `AVD_Custom` 从来没实现过——`IValueDomainPolicy` 全仓零实现者，命中时 `ensure` + 降级 Clamp。而「特殊映射」（非线性曲线、条件映射）的正确位置是**产生这个数的地方**（`FTcsParamValue` 的源），不是**收口的地方**。值域层开插点，等于让框架在求值路径上回调宿主代码。

### 4.3 钳制时机：写时收口

收口发生在**重算路径**（§3.4 最后一步），结果写进 `CachedCurrent`。

> **`Dynamic` 边界的读取会产生依赖边**：`A` 的上界依赖 `B` ⇒ `B` 变化时 `A` 必须重算。这与 §5 的依赖图是同一张图。

---

## 5. 依赖图 ✅

### 5.1 依赖只有两个来源

| 来源 | 语义 | 声明位置 |
|---|---|---|
| `Bounds.Dynamic` | **边界依赖**——本属性的上下界读另一个属性 | Def 上 |
| `AttributeScaled` | **值依赖**——本条修正器的值 = 系数 × 另一属性 | 修正器的 `FTcsParamValue` 里 |

**两者都是配置期确定的**，所以依赖图是静态的。

> **派生属性不构成第三种来源**：`DeriveExpression` 会被展开成一条内置修正器（§7.4），它的每条 `AttributeScaled` 都属于第二种。

**依赖图是「单实体」的**：`FTcsAttributeStore` 属于一个实体，图也只描述该实体内部属性之间的关系。

| 读法 | 进图吗 | 为什么 |
|---|---|---|
| `Side == Target` | ✅ 进 | 被读的是**本实体**的属性 ⇒ 是同一次结算内部的节点依赖 |
| `Side == Instigator` | ❌ **不进** | 跨实体。施法者的属性变化不会、也不该触发被施加者重算（§5.2 的 `BuildModifierOutEdges` 直接 `return`） |

**因此「施法者属性变化 ⇒ 本实体跟随更新」必须走事件订阅**（D15），不是依赖图。两条路径的分工是：

| 需求 | 路径 |
|---|---|
| 同实体内部 A 变 ⇒ B 变 | **依赖图**（§5.3 的拓扑序） |
| 施法者 A 变 ⇒ 我的修正器要改 | **订阅变更事件**，在自己的回调里 `DetachMod` + `AttachMod` |

> **Why Not 把跨实体也建成边**：那会让一次属性写入连锁唤醒**另一个实体**的结算，而对方可能又在回调里写回本实体——递归深度不可控（§2.2 已就「读时惰性重算」否决过同一个风险）。依赖图 MUST 停在实体边界内。

### 5.2 建图：先建节点，再连边

**不变式**：依赖图 MUST 在每次「槽位集合或修正器集合发生变化」之后重建，且 MUST 在下次求值前完成。

**触发条件**（凡改这两样之一的入口，都必须置 `Graph.bDirty`）：

| 入口 | 改了什么 |
|---|---|
| `ApplyAttributeSet` / `RemoveAttributeSet` | **槽位集合** + 修正器集合（挂/摘内置派生条目） |
| `AttachMod` / `DetachMod` | **修正器集合**（不动槽位） |

> **Why 用不变式表述而不是列举触发点**：列举式每加一个写入口就漏一次。V1 形态下 `AttachMod` 就漏了——运行时挂的 `AttributeScaled` 修正器**静默不建边**，属性变化后不重算、不报错。

**落点 = 批归零点**（`CommitBatch`）。写入口一律不直接重建，只置脏位：

```cpp
void UTcsAttributeSubsystem::CommitBatch(FTcsCombatEntityHandle Entity)
{
	FTcsAttributeStore& Store = Stores[Entity];
	if (--Store.BatchDepth > 0) { return; }

	// ① 结构变了 ⇒ 重建。**重建会换掉整张节点表**（数量与顺序都变）
	if (Store.Graph.bDirty)
	{
		RebuildGraph(Entity, Store);
		Store.Graph.bDirty = false;

		// ⇒ 批内登记的节点下标**全部失效**，MUST 丢弃
		//    换成「全量重算」：结构变了本来就意味着很多节点要变，且正确性无需证明
		Store.BatchWrittenNodes.Reset();
		SettleWrites(Entity, AllNodeIndices(Store.Graph));
		return;
	}

	// ② 结构没变 ⇒ 按登记的节点结算
	if (Store.BatchWrittenNodes.Num() > 0)
	{
		SettleWrites(Entity, Store.BatchWrittenNodes);
		Store.BatchWrittenNodes.Reset();
	}
}
```

> **顺序 MUST 是先建图、后结算**：反过来 `SettleWrites` 会用旧图，新建的边这一轮不生效。

> **`AllNodeIndices(Graph)` 是「按 `EvalOrder` 生成 0..N-1 的全集视图」**：它不需要真实数组，`SettleWrites` 收到的是一份「全部节点都要算」的意图，实现上可直接走一个 `bAll = true` 的快路径。

> **Why 重建后必须丢弃 `BatchWrittenNodes`**：`FTcsEvalNode` 带 `ModIndex`，而重建会按 `Inst.Mods` 的**当前顺序**重新编号。批内先摘一条早期修正器、再挂一条新的，旧下标就指向了别的节点——**用旧下标索引新节点表 = 算错对象**，且没有任何报错。

> **Why Not 批内改用「槽位 + 层」记录**：那是粗粒度，会把与本批无关的节点也拽进重算，而结构变化后全量重算本来就是 O(节点数)，省不下来。

#### 第一段：建节点

```cpp
void BuildNodes(const FTcsAttributeStore& Store, TArray<FTcsEvalNode>& OutNodes)
{
	OutNodes.Reset();

	for (int32 Slot = 0; Slot < Store.Instances.Num(); ++Slot)
	{
		const FTcsAttributeInstance& Inst = Store.Instances[Slot];
		if (!Inst.bActive) { continue; }

		// 两个段：各一个基值节点 + 一个折叠节点
		OutNodes.Add({ ETcsEvalNodeKind::SegmentBase, Slot, ETcsAttrSegment::Basic, INDEX_NONE });
		OutNodes.Add({ ETcsEvalNodeKind::SegmentBase, Slot, ETcsAttrSegment::Extra, INDEX_NONE });
		OutNodes.Add({ ETcsEvalNodeKind::Segment,     Slot, ETcsAttrSegment::Basic, INDEX_NONE });
		OutNodes.Add({ ETcsEvalNodeKind::Segment,     Slot, ETcsAttrSegment::Extra, INDEX_NONE });

		// 总值节点
		OutNodes.Add({ ETcsEvalNodeKind::Total, Slot, ETcsAttrSegment::Basic, INDEX_NONE });

		// 每条修正器各一个节点
		for (int32 ModIndex = 0; ModIndex < Inst.Mods.Num(); ++ModIndex)
		{
			OutNodes.Add({ ETcsEvalNodeKind::Modifier, Slot, ETcsAttrSegment::Basic, ModIndex });
		}
	}
}
```

#### 第二段：连边

四条规则，缺一不可：

| # | 边 | 说明 |
|---|---|---|
| ① | **段基值节点 → 段节点** | `BasicValue` 变 ⇒ `Basic` 段重算；`ExtraValue` 同理 |
| ② | 修正器节点 → 它所在层的段节点 | `Basic` / `Extra` 层的修正器进对应段；`Global` 层的修正器进总值 |
| ③ | 两个段节点 → 总值节点 | 段变了总值必变 |
| ④ | 边界属性 → 总值节点 | `Bounds.Dynamic` 只影响值域收口（只作用于总值） |

**基值也必须是节点，不能只是「段节点的一个输入参数」**：过滤读要绕开段节点直连它的组成部分，就必须有一个**可被单独连边的基值节点**。若基值只是段节点内部读的字段，过滤读就只能连段节点——环就又回来了。

**修正器的出边由它的 Operand 决定**——这是标签筛选生效的地方：

```cpp
// 一条修正器节点往外连：它读了什么，就连到什么
void BuildModifierOutEdges(
	const FTcsAttributeStore& Store,
	int32 ModNode, const FTcsAttrModInstance& Mod,
	TArray<FTcsEvalEdge>& OutEdges)
{
	ForEachAttributeRead(Mod.Operand, [&](FGameplayTag SrcTag, ETcsAttrReadSide Side,
		ETcsAttrReadPart Part, const FTcsTagRequirements& Filter)
	{
		if (Side != ETcsAttrReadSide::Target) { return; }   // 跨实体不入本实体的图（§5.1）

		const int32* FromSlot = Store.TagToSlot.Find(SrcTag);
		if (!FromSlot) { /* 目标不存在：报错（§5.4 ③） */ return; }

		// ── 关键：按 Part 与 Filter 决定连到哪个节点 ──
		if (Part == ETcsAttrReadPart::Total)
		{
			// 读总值：连 Total 节点（过滤不适用——总值是聚合结果，无法按修正器拆）
			OutEdges.Add({ FindNode(Store, *FromSlot, ETcsEvalNodeKind::Total), ModNode });
		}
		else
		{
			const ETcsAttrSegment Seg = (Part == ETcsAttrReadPart::Basic)
				? ETcsAttrSegment::Basic : ETcsAttrSegment::Extra;

			if (Filter.IsEmpty())
			{
				// 不过滤：连该段的折叠节点
				OutEdges.Add({ FindNode(Store, *FromSlot, ETcsEvalNodeKind::Segment, Seg), ModNode });
			}
			else
			{
				// 过滤读：**跳过段节点，直连「被计入的」修正器节点与段基值**
				// ⇒ 被排除的修正器不在依赖链上 ⇒ 环消失（§1.3）
				OutEdges.Add({ FindSegmentBaseNode(Store, *FromSlot, Seg), ModNode });

				for (int32 OtherIdx = 0; OtherIdx < Store.Instances[*FromSlot].Mods.Num(); ++OtherIdx)
				{
					const FTcsAttrModInstance& Other = Store.Instances[*FromSlot].Mods[OtherIdx];
					if (Other.Def->Layer != SegmentToLayer(Seg)) { continue; }        // 别的层不参与本段
					if (!Filter.RequirementsMet(Other.Tags)) { continue; }              // 被排除 ⇒ 不连
					OutEdges.Add({ FindNode(Store, *FromSlot, ETcsEvalNodeKind::Modifier, Seg, OtherIdx), ModNode });
				}
			}
		}
	});
}
```

**四条规则的主循环**（规则 ②③④ 在此，规则 ② 的修正器出边另由 `BuildModifierOutEdges` 处理）：

```cpp
void BuildEdges(const FTcsAttributeStore& Store,
	TConstArrayView<FTcsEvalNode> Nodes,
	TArray<FTcsEvalEdge>& OutEdges,
	TArray<FGameplayTag>& OutMissingTags)
{
	OutEdges.Reset();

	for (int32 Slot = 0; Slot < Store.Instances.Num(); ++Slot)
	{
		const FTcsAttributeInstance& Inst = Store.Instances[Slot];
		if (!Inst.bActive) { continue; }

		const int32 SegBasic = FindNode(Store, Slot, ETcsEvalNodeKind::Segment, ETcsAttrSegment::Basic);
		const int32 SegExtra = FindNode(Store, Slot, ETcsEvalNodeKind::Segment, ETcsAttrSegment::Extra);
		const int32 Total    = FindNode(Store, Slot, ETcsEvalNodeKind::Total);

		// ① 段基值 → 段节点
		OutEdges.Add({ FindSegmentBaseNode(Store, Slot, ETcsAttrSegment::Basic), SegBasic });
		OutEdges.Add({ FindSegmentBaseNode(Store, Slot, ETcsAttrSegment::Extra), SegExtra });

		// ② 修正器 → 它所在层的段节点（Global 层直接进总值）
		for (int32 ModIndex = 0; ModIndex < Inst.Mods.Num(); ++ModIndex)
		{
			const FTcsAttrModInstance& Mod = Inst.Mods[ModIndex];
			const int32 ModNode = FindNode(Store, Slot, ETcsEvalNodeKind::Modifier,
				ETcsAttrSegment::Basic, ModIndex);   // Modifier 节点的 Segment 字段不参与定位

			switch (Mod.Def->Layer)
			{
			case ETcsAttrLayer::Basic:  OutEdges.Add({ ModNode, SegBasic }); break;
			case ETcsAttrLayer::Extra:  OutEdges.Add({ ModNode, SegExtra }); break;
			case ETcsAttrLayer::Global: OutEdges.Add({ ModNode, Total });    break;
			}

			// 它读了什么，就连到什么（含标签筛选，见下一段）
			BuildModifierOutEdges(Store, ModNode, Mod, OutEdges);
		}

		// ③ 两个段 → 总值
		OutEdges.Add({ SegBasic, Total });
		OutEdges.Add({ SegExtra, Total });

		// ④ 边界属性 → 总值（Bounds.Dynamic 只影响值域收口，只作用于总值）
		if (Inst.Def->Bounds.Mode == ETcsBoundMode::Dynamic)
		{
			const int32* BoundSlot = Store.TagToSlot.Find(Inst.Def->Bounds.TargetAttribute);
			if (BoundSlot)
			{
				OutEdges.Add({ FindNode(Store, *BoundSlot, ETcsEvalNodeKind::Total), Total });
			}
			else
			{
				OutMissingTags.Add(Inst.Def->Bounds.TargetAttribute);   // 交给 §5.4 ③ 报错
			}
		}
	}
}
```

> **`Modifier` 节点的 `Segment` 字段不参与定位**：它的身份是 `(Slot, ModIndex)`，层信息在 `Mod.Def->Layer` 里。`FindNode` 收 `Segment` 只是为了让四种节点共用签名（见下）。
> **过滤读 MUST NOT 连段节点**：段节点是「该段全部修正器」的折叠结果，连上它等于把被排除的修正器也拽进依赖链——环就回来了。**直连被计入的每个修正器节点**，才是真正的「只依赖我读到的那部分」。

> **这一条的代价是边数变多**：不过滤读是 1 条边，过滤读是 `1 + 该段被计入的修正器数` 条。过滤读本身稀少（转换类配置），可接受。

#### `RebuildGraph` = 建节点 + 连边 + 交给图

```cpp
void UTcsAttributeSubsystem::RebuildGraph(FTcsCombatEntityHandle Entity, FTcsAttributeStore& Store)
{
	TArray<FTcsEvalNode> Nodes;
	TArray<FTcsEvalEdge> Edges;
	TArray<FGameplayTag> MissingTags;

	BuildNodes(Store, Nodes);
	BuildEdges(Store, Nodes, Edges, MissingTags);

	// 交图：只排序 + 检环，不认识属性
	Store.Graph.Rebuild(Nodes, Edges);

	// 缓存数组与节点表对齐（值在结算时填）
	Store.NodeValues.SetNumZeroed(Nodes.Num());

	for (const FGameplayTag& Miss : MissingTags)
	{ /* Error 日志，具名 tag */ }
}
```

**两个查节点下标的辅助函数**（节点表由 `BuildNodes` 生成，查法是纯扫，不进热路径）：

```cpp
// 一个属性槽位的某类节点。Segment / SegmentBase 还要指定是哪一段；Modifier 还要指定 ModIndex
int32 FindNode(const FTcsAttributeStore& Store, int32 Slot, ETcsEvalNodeKind Kind,
	ETcsAttrSegment Segment = ETcsAttrSegment::Basic, int32 ModIndex = INDEX_NONE);

// 段基值节点（连边规则 ① 与过滤读都要用它）——它是 FindNode(..., SegmentBase, Seg) 的简写
int32 FindSegmentBaseNode(const FTcsAttributeStore& Store, int32 Slot, ETcsAttrSegment Segment);
```

> **基值节点存在的唯一理由是「可被单独连边」**——见上表 ① 的注。它在 `BuildNodes` 里与段节点同批创建，数量固定为「活跃属性数 × 2」。

---

### 5.3 结算：沿反向边扩散，按拓扑序重算

```cpp
// 结算：收集受影响**节点**，按拓扑序重算。
// 调用点只有两个：MarkWrittenNodes（批外）/ CommitBatch（批归零）
void SettleWrites(FTcsCombatEntityHandle Entity, TConstArrayView<int32> WrittenNodes)
{
	FTcsAttributeStore& Store = Stores[Entity];
	const FTcsAttributeGraph& Graph = Store.Graph;

	// ① 扩散：沿反向边收集全部受影响节点
	ScratchAffected.Reset();
	for (int32 Node : WrittenNodes) { ScratchAffected.AddUnique(Node); }
	for (int32 i = 0; i < ScratchAffected.Num(); ++i)
	{
		for (int32 Dep : Graph.Dependents[ScratchAffected[i]])
		{ ScratchAffected.AddUnique(Dep); }
	}

	// ② 按拓扑序重算，顺序 MUST 稳定
	for (int32 Node : Graph.EvalOrder)
	{
		if (!ScratchAffected.Contains(Node)) { continue; }
		Store.NodeValues[Node] = EvaluateNode(Entity, Store, Graph.Nodes[Node]);
	}

	// ③ 总值节点变了才记变更（事件按属性合并，§6.4）
	RecordDeltasForTotalNodes(Entity, Store, ScratchAffected);
}
```

**三段求值各自只看自己的输入**：

```cpp
double EvaluateNode(Entity, Store, const FTcsEvalNode& Node)
{
	const FTcsAttributeInstance& Inst = Store.Instances[Node.Slot];

	switch (Node.Kind)
	{
	case ETcsEvalNodeKind::SegmentBase:
	{
		// 段基值：无输入，直接就是实例上的权威状态
		return (Node.Segment == ETcsAttrSegment::Basic) ? Inst.BasicValue : Inst.ExtraValue;
	}

	case ETcsEvalNodeKind::Modifier:
	{
		const FTcsAttrModInstance& Mod = Inst.Mods[Node.ModIndex];
		return EvalModValue(Entity, Store, Mod);   // §7.2；过滤读在此按 Tag 排除（§5.2 的边已保证无环）
	}

	case ETcsEvalNodeKind::Segment:
	{
		// 折叠：段基值节点 + 该段全部修正器节点（全局层**不属于本段**）
		return FoldSegment(Store, Node.Slot, Node.Segment);   // §3.4
	}

	case ETcsEvalNodeKind::Total:
	{
		// 两段之和 → 全局层 → 值域收口
		return EvaluateTotal(Store, Node.Slot);   // §3.4
	}
}

	return 0.0;
}
```

> **拓扑序保证「读到的依赖已是新值」**：每条边 `From → To` 在 `EvalOrder` 里 `From` 必在 `To` 之前。这是修正器级粒度的直接收益——弗拉米尔的 `M1,M2 → M4 → AP段 → M3 → HP段 → HP总值` 会自动排出正确顺序。

> **每个节点只算一次**：`ScratchAffected` 用 `AddUnique` 保证唯一，② 又只遍历一次 `EvalOrder` ⇒ 菱形依赖下下游节点只算一次，且在它的全部输入之后。

> **同一节点被多处依赖不会重复求值**：`NodeValues` 按节点下标存，谁读都是从数组里取。

#### 写入口登记的是**节点**，不是槽位

```cpp
// 按「哪个属性的哪个节点」登记
void MarkWrittenSegment(Entity, int32 Slot, ETcsAttrSegment Segment);   // 段基值被改：登记该段的**基值节点**（下游经反向边覆盖段节点与总值）
void MarkWrittenModifier(Entity, int32 Slot, int32 ModIndex);            // 单条修正器增删
void MarkWrittenTotal(Entity, int32 Slot);                               // 边界变更（值域收口）
```

> **改成节点级后，`ETcsAttrWritePart` 可以删掉**：它原本是「槽位 + 层」的粗粒度替代品，现在直接点名是哪个节点，精度更高且没有近似。

> **批内合并**：`BatchWrittenNodes` 用 `AddUnique(int32)` 即可——节点下标天然唯一，不需要再合成「最宽层」。
### 5.4 环检测与拓扑序

**环的判定不在运行期**——它由编辑期校验承担，运行期只按不变式断言。

| # | 规则 |
|---|---|
| ① | **环 MUST 由 `UTcsDefValidationSubsystem`（editor-only）在定义期检出**，配置保存即报 `Error` |
| ② | **`EvalOrder` MUST 稳定**：Kahn 算法在候选集里取节点下标最小者，不依赖哈希遍历顺序。否则浮点求值顺序会变、事件顺序会漂、回放会分叉 |
| ③ | **`AttributeScaled` 的目标不存在**：报错。这不是兜底——该属性确实算不出值 |
| ④ | **运行期图不可能有环**：属性定义是全项目静态集合，全局无环 ⇒ 任何实体的子集都是它的子图 ⇒ 子图无环 |

> **「环即失败」在 `ApplyAttributeSet` 里不成立**——那时槽位已建、`RefCount` 已加，检出环也回滚不了。**所以环检测在整个运行期都不做**：它被前置到编辑期，见 ①。

> **修正器级粒度下，弗拉米尔这类配置本就无环**（§1.3 的图）——`M4` 过滤读 HP 的 Extra 段时只连 `M1`/`M2`/段基值，不连 `M3`。**编辑期确实会报的环，是「过滤读写成读总值」那种真错配**（`Part = Total` 或 `ModifierTagRequirements` 为空）。

**编辑期环检测的判据与运行期建图 MUST 一致**：都用 §5.2 的 `BuildEdges`（含标签过滤）产出边，再跑拓扑排序。两处判据若分叉，就会出现「编辑期放行、运行期报环」或反之。

#### 运行期兜底：断言 + 补齐，不阻塞

运行期唯一可能遇到环的路径是**未经编辑期校验的定义**（热重载、脚本生成、调试代码直接注册）。图仍要给出一条不静默错值的行为：

```cpp
bool FTcsAttributeGraph::Rebuild(TConstArrayView<FTcsEvalNode> InNodes, TConstArrayView<FTcsEvalEdge> InEdges)
{
	Nodes = InNodes;
	// …建 Dependents + Kahn 拓扑排序…

	if (EvalOrder.Num() < Nodes.Num())
	{
		// 有节点排不进拓扑序 ⇒ 存在环（未经编辑期校验的定义）
		ensureMsgf(false, TEXT("属性依赖图检出环：%d / %d 个节点无法排序"),
			Nodes.Num() - EvalOrder.Num(), Nodes.Num());

		// 兜底：未排序的节点按升序追加到末尾
		// ⇒ 每个节点每轮仍被求值（不变式），有环部分结果不确定但**不静默漏算**
		for (int32 Node = 0; Node < Nodes.Num(); ++Node)
		{
			if (!EvalOrder.Contains(Node)) { EvalOrder.Add(Node); }
		}
		return false;
	}

	return true;
}
```

> **Why 补齐而不是丢弃**：`SettleWrites` 只遍历 `EvalOrder`（§5.3）。若某槽位不在里面，它**永远不被求值**——读到的是上一次的缓存值，无日志、不断错、只让数值悄悄错。补齐保证「每个槽位每轮都被求值」这条不变式。

> **`Rebuild` 的 `bool` 返回值只服务这一个兜底分支**：调用方（`CommitBatch`）不因它中止。

---

## 6. 存在性与变更通知 ✅

### 6.1 属性的「存在」是引用计数

```cpp
int32 RefCount = 0;      // 有多少活跃的 ApplyAttributeSet 需要它
bool  bActive  = false;   // RefCount > 0
```

**为什么需要计数**：两件装备都提供「暴击率」属性，卸下其中一件时它不该消失。V1 靠冻结缓解这个问题的后果，V2 直接解决。

| `bActive` | 聚合 | 读口 | 修正器 | `BasicValue` / 缓存 |
|---|---|---|---|---|
| `true` | 参与 | 可读 | 生效 | 保留 |
| `false` | 不参与 | 读不到（对调用方而言「该实体没有这个属性」） | **保留在实例上**，重新激活后原样生效 | 保留 |

### 6.2 `bRemovable`：常驻属性

`UTcsAttributeDef::bRemovable == false` ⇒ 该属性**永远不能被减到 0**：

- 可以 `ApplyAttributeSet` 多次（`RefCount` 正常累加）
- `RemoveAttributeSet` 若会让它归零 ⇒ **整次操作失败**（前置校验③，见 §7.1）

**判定口径（实现必读）**：逐个检查本 Set 里「`!bRemovable` 且当前 `RefCount == 1`」的属性——它们这次会被减到 0 ⇒ **整次拒绝**，不做任何改动。

> **MUST NOT 部分应用**：不能「能减的减掉、不能减的留着」。半应用会让实体的属性集合停在一个配置者没有声明过的状态，而且没有任何地方能查出它处于这个状态。

**用途**：被 `Bounds.Dynamic` 引用或被 `AttributeScaled` 读取的属性必须是常驻的，否则依赖它的属性会悬空。

### 6.3 存在性变化：`FTcsAttributePresenceChangedEvent`

```cpp
struct FTcsAttributePresenceChangedEvent
{
	FTcsCombatEntityHandle Entity;
	FGameplayTag           Attribute;
	bool                   bActive;   // 现在在不在
};
```

**查询口**：`bool IsAttributeActive(Entity, Tag) const;`

**频道**：`Event.Attribute.Presence`（固定频道，payload 带身份，订阅者自行过滤）。

**为什么必须单开事件、不复用值变更事件**：值变更的语义是「当前值变了」（`Old → New`），而存在性变化没有「旧值 → 新值」这回事——硬塞要么让 `Old` 填 NaN，要么给理由字段加一位，两种都是污染。

**发包顺序**：一次形态切换涉及多个属性时，MUST **先停用后激活**、逐条发。交错会让「按存在性计数」的 UI 看到中间态。

### 6.4 值变更：`FTcsAttributesChangeEvent`（按实体合批）

```cpp
struct FTcsAttributeDelta
{
	FGameplayTag Attribute;
	double       OldValue;
	double       NewValue;
};

struct FTcsAttributesChangeEvent
{
	FTcsCombatEntityHandle     Entity;
	TArray<FTcsAttributeDelta> Changed;   // 本帧该实体的全部变化
};
```

**产生**：写入路径重算后，`New != Old`（阈值 `1e-5`）⇒ 按 `(Entity, Attribute)` 记入待发表。**同一帧内同一属性多次变化合并为一条**，保留首次 `Old`、末次 `New`。

**派发**：帧末统一派发，按实体归组，**每个有变化的实体一条事件**。

**派发前 MUST 先查订阅者**：`Bus->HasSubscribers(Event.Attribute.Changed)` 为假 ⇒ 连待发表都不记。

**`OldValue` 的语义**：上次**观察到的**缓存值，不是「重新算一遍再比较」的结果。

> **Why Not 逐属性一条事件**：5000 实体 × 每个几个属性 = 上万条事件。按实体归组让订阅者一次拿到「这个实体这帧变了什么」，既省派发也省它的分支。
> **Why Not 单属性精确订阅**：那需要在属性域内再造一套「谁关心哪个 (Entity, Attribute)」的订阅表，与事件总线并存且各自管生命周期。收益不足以抵掉这套机制——订阅者在 `Changed` 里按 tag 查找即可。

---

## 7. 修正器的施加与撤销 ✅

### 7.0 五个写入口

会改变属性状态的路径只有这五个。**它们都不直接调 `SettleWrites`**，只调 `MarkWritten`（§5.3），由批机制决定立刻结算还是攒到 `CommitBatch`。

| 入口 | 改什么 | 结算面 |
|---|---|---|
| `ApplyAttributeSet` | 建槽位 / `RefCount++` / 挂内置派生条目 | **内部开批**，整批算一次 |
| `RemoveAttributeSet` | `RefCount--` / `bActive = false` | **内部开批**，整批算一次 |
| `AttachMod` | 挂一条修正器 | 目标槽位 |
| `DetachMod` | 按 `Source` 摘除 | **该 `Source` 触及的全部槽位**（可跨多个属性） |
| `SetBaseValue` | 直写基值（§7.6） | 目标槽位 |

> **为什么这张表必须完整**：`SettleWrites` 的正确调用时机是「一次逻辑写操作结束」，而「一次写操作」的边界由入口定义。多一个未登记的入口，就多一个漏结算的地方。

---

### 7.1 属性集：唯一的增删入口

```cpp
// 增量激活：只激活 Set 里的，不碰其他。RefCount++
bool ApplyAttributeSet(FTcsCombatEntityHandle Entity, const UTcsAttributeSetAsset& Set);

// 反向停用：RefCount--，归零才 bActive = false
bool RemoveAttributeSet(FTcsCombatEntityHandle Entity, const UTcsAttributeSetAsset& Set);
```

**`MUST NOT` 提供单个属性的 `AddAttribute` / `RemoveAttribute`**：属性集是一次形态切换的完整语义，逐个加会引入「先加谁后加谁」的顺序问题。

**两条都原子，且没有回滚路径**：校验**全部前置**，通过即全量应用，中途不设任何可能失败的步骤。

**前置校验（四类，缺一不可）**：

| # | 校验 | 失败返回 |
|---|---|---|
| ① | 实体已注册 | `false` |
| ② | 资产有效（`Set` 非空、每个 `Def` 非空、tag 有效） | `false` |
| ③ | **`bRemovable == false` 的属性不会归零**（§6.2） | `false` |
| ④ | 环（由编辑期承担，§5.4 ①） | — 不在此校验 |

**`ApplyAttributeSet` 流程**：

```text
① 前置校验 ①②③ —— 任一失败即返回 false，一个字节都不写
② 内部开批 BeginBatch
③ 逐 tag：表里没有 → 建槽位、从 Def 拷基值、挂派生内置条目（**不在此求值**）
④ 逐 tag：RefCount++
⑤ 置 `Graph.bDirty = true`（不在此重建）
⑥ 逐 tag `MarkWrittenSegment`，然后 `CommitBatch`（§2.2、§5.2）
   —— 归零点先重建依赖图，再整批结算一次；外层已开批时并入外层，不提前算
```

**返回值的含义（MUST 读）**：返回 `true` = **本次的属性集改动已完整应用、值已结算**。

> **它不含「图无环」的保证**：环检测在编辑期（§5.4 ①），运行期遇到未经校验的定义时只 `ensure` + 补齐 `EvalOrder`，**不失败、不回滚**。那是有环部分值不确定的已知降级，不是应用失败。

> **`AttributeScaled` 的目标属性不存在也不失败**：那属于配置错误（§5.4 ③），运行期只报 `Error`，MUST 由编辑期资产校验拦截。**让它失败就得回滚已建的槽位——而回滚逻辑只为一条本可在作者期拦住的错误存在，不值得。**

> **环检测不在本流程里**：由编辑期校验承担（§5.4 ①）。

> **Why Not 在步骤 ③ 里就求值**：两个原因。① **依赖图还没建**（批归零点才建），若 A 依赖 B 而 B 排在后面的 tag，A 求值时 B 的槽位还不存在；② 步骤 ⑥ 反正要整体结算一遍，步骤 ③ 那次是白算——同一批里新建的槽位会被算两次。

> **Why Not 在步骤 ④ 逐个槽位直接结算**：那样「A 和其依赖 B 同批被写」时，A 会按调用次数重算，而且每次都要重扫一遍 `Dependents`。**用批而不是另造一个「批量 `MarkWritten`」的 API**——批已经能表达「这一段算一次」，再开第二条批量通道就是同一件事的两份实现。

### 7.2 修正器的值：`FTcsParamValue` 在实例上

```cpp
// 模板：纯配置，不含值来源
USTRUCT()
struct FTcsAttrModDef
{
	UPROPERTY(EditAnywhere) ETcsAttrLayer   Layer;
	UPROPERTY(EditAnywhere) ETcsAggregateOp Op;
	UPROPERTY(EditAnywhere) double          DefaultValue = 0.0;
	UPROPERTY(EditAnywhere) int32           Priority     = 0;
	UPROPERTY(EditAnywhere) ETcsParamMode   Mode         = ETcsParamMode::Live;   // 枚举归参数域
};

// 实例：携带实际的值来源
struct FTcsAttrModInstance
{
	TObjectPtr<UTcsAttrModDef> Def;        // Layer / Op / Priority / Mode 都从这里读
	FTcsParamValue             Operand;    // 运算数（施加时定）
	FTcsSourceHandle           Source;     // 归属来源

	// 本条携带的标签（供读取方按标签筛选；空 = 无标签，永不被 IgnoreTags 排除）
	FGameplayTagContainer      Tags;

	// 冻结位（仅 Mode == Snapshot 时有效；何时冻结归参数域，冻在哪归本域）
	double SnapshotValue = 0.0;
	bool   bHasSnapshot  = false;
};
```

> **`FTcsAttrModDef` MUST NOT 持有 `FTcsParamValue`**：它被状态域、技能域、装备、纯 C++ 代码共同使用，绑死参数域会锁死泛用性。值由施加方填。

> **`Tags` 只由施加方在挂的当场写入**（`AttachMod` 的形参，§7.5），之后不再改。它不参与聚合运算，只被读取方按 `FTcsTagRequirements` 筛（§5.2）。**空标签的修正器永不被 `IgnoreTags` 排除**——这正是「派生属性 / 普通装备 / 符文」这些无标签修正器不被转换类读取误伤的原因。

> **`Source` 是本域持有的句柄，不交给聚合器**：收集条目时只填 `Entry.OrderSeq = Source.Id`（《数值聚合器》§3.2 已裁「聚合器只接 `.Id`」）。本域仍完整持有 `FTcsSourceHandle`，因为 `DetachMod` 要按它撤销、UI 要按它归因。

> **为什么成员叫 `Operand` 而不是 `Value`**：`FTcsParamValue` 自身有个 `Source` 成员（值从哪来，装 `FTcsParamSource_*` 族的实例），而本结构另有一个 `Source`（谁施加的，`FTcsSourceHandle`）。若成员叫 `Value`，同一结构体里就会出现 `Value.Source` 与 `Source` 并排、一词两义。`Operand` 与 `Op` 是本域的既有配对（运算带 + 运算数）。至于 `FTcsParamValue` 这个名字本身是否合适，归参数域裁决。

**求值**：

```cpp
// 求值一条修正器。
//   Entity / Store 只供 FTcsParamSource_AttributeScaled 读同实体的其它属性；
//   纯 Literal 源不碰它们（传 nullptr 亦可）。
double EvalModValue(FTcsCombatEntityHandle Entity, const FTcsAttributeStore& Store,
	const FTcsAttrModInstance& Mod)
{
	if (Mod.Def->Mode == ETcsParamMode::Snapshot && Mod.bHasSnapshot)
	{ return Mod.SnapshotValue; }

	FTcsParamEvalContext Ctx = MakeEvalContext(Entity, Store, Mod.Def->Mode);
	return Mod.Operand.Eval(Ctx);    // AttributeScaled 在此按 §5.2 的过滤规则读
}
```

### 7.3 求值时机：模式归参数域，冻结位归本域

`Live` / `Snapshot` 这对语义**归参数域**（`ETcsParamMode`），属性域只是消费者。判据来自 V1 自己的定性——`FTcsParamSnapshot` 的注释原文写着「它是**参数域**的概念（修正器物化、技能侧参数行都要读它），不是状态模块专属」（`TcsStateSnapshot.h:60-61`）。

| 模式 | 求值时机 | 是否跟随源变化 | 依赖边 |
|---|---|---|---|
| `Live` | 每次收集 | ✅ | 登记 |
| `Snapshot` | **施加时算一次**，存 `SnapshotValue` | ❌ | **不登记** |

**`Snapshot` MUST NOT 登记依赖边**——否则源属性变化会触发本属性重算、拿回同一个缓存值，白算一遍。

**分工边界**：

| 责任 | 归属 |
|---|---|
| 模式的枚举与语义（`ETcsParamMode`） | 参数域 |
| 「此刻该不该求值、求完要不要冻」的判定 | 参数域 |
| **冻结值存在哪、存多久** | **本域**（`FTcsAttrModInstance::SnapshotValue`） |

> **默认值**：属性域默认 `Live`（`FTcsAttrModDef::Mode = ETcsParamMode::Live`），**与参数行的默认 `Snapshot` 相反**。依据是 V1 的实测口径——修正器路径上两者本来就同解（`TcsCastOps_Snapshot.cpp` 注释原话：「`EPM_Live` / `EPM_Snapshot` 在本路径上同解：两者都走实时折叠」），属性修正器从一开始就是实时折叠的。

> **Why Not 本域自建枚举**：同一对语义若在属性域另立一个 `ETcsValueMode`，属性域就从「消费者」变成了「并列的语义拥有者」——技能域、状态域将来各立一个，同一件事就有三份定义。V1 的参数行已经有 `ETcsParamMode`，那是唯一的一份。

**`Mode` 是 `FTcsAttrModDef` 的配置字段，运行期只读**。要改语义只能改资产、重新施加。

> **本域不另存「值的来源副本」**：V1 的快照条目里有个 `FInstancedStruct SourceRef`，注释标着「Debug 可见取值来源 / Live 化预留」，**无消费者**。本域不预建它。

### 7.4 派生属性 = 内置修正器

`UTcsAttributeDef::bDerived == true` 时，`ApplyAttributeSet` 建槽位后**自动挂一条修正器**：

```cpp
if (Def->bDerived && Def->DeriveExpression.IsValid())
{
	FTcsAttrModInstance Derived;
	Derived.Def     = GetBuiltinDerivedModDef();   // Layer=Extra, Op=Add, Priority=0, Mode=Live
	Derived.Operand = Def->DeriveExpression;
	Derived.Source  = SourceRegistry.Allocate();   // **正常发号**，`运行时实例.md` §1.3.3
	Inst.Mods.Add(MoveTemp(Derived));
}
```

| 属性 | 值 |
|---|---|
| `Source` | **正常发号**（`Allocate()`）。这条修正器不响应外部撤销，**因为调用方拿不到这个号**——不是因为它有一个特殊值 |
| 可否被 `RemoveAttributeSet` 摘除 | **否**——它随实例生死（实例释放时一并清） |
| UI 列出修正器时 | **跳过**——判据是「这条用的是内置派生 Def」（`Def->bDerived`），**不是查 `Source`** |

> **为什么不用保留值**（2026-11-11 订正）：原设计用 `FTcsSourceHandle::MakeBuiltin()`（`Id = UINT64_MAX`）表达「框架自己挂的」。**该保留值已从 `运行时实例.md` 删除**，理由：
>
> | # | 理由 |
> |---|---|
> | ① | **「没有外部施加者」不是句柄该表达的事**——号只是一个号，「谁发的」由持有者决定 |
> | ② | **V1 就是正常发号的**（`TcsBuffOps.cpp:143`），全仓 0 处 `MakeBuiltin` |
> | ③ | **`UINT64_MAX` 会污染 `OrderSeq`**——`LatestApplied` 下派生修正器恒赢、`EarliestApplied` 下恒输，一个本该中性的值变成极端值 |
> | ④ | **UI 过滤不需要它**——查 `Def->bDerived` 比查 `Source` 更直接 |

> **派生修正器的 `OrderSeq` 是真实号**：它在 `ApplyAttributeSet` 那一刻发出，所以「比之前挂的修正器更晚」这个判定**是真的**，不是靠哨兵值伪造的。

> **发号时机只在「真正新建槽位」那一次**：`ApplyAttributeSet` 步骤 ③ 是「表里没有 → 建槽位」（§7.1）。`bActive == false` 但槽位还在的实例**不重建**（§6.1 表：修正器保留在实例上，重新激活后原样生效）⇒ **不会重复发号、不会挂第二条派生修正器**。`RefCount` 归零只把 `bActive` 置 false，不动 `Mods`。

**多源派生**直接用 `Composite`：

```cpp
// Attack = Strength × 2 + Agility × 0.5
DeriveExpression = {
	Source = FTcsParamSource_Composite{
		Op = Add,
		Terms = {
			{ Source = FTcsParamSource_AttributeScaled{ Target, Attr.Strength, 2.0 } },
			{ Source = FTcsParamSource_AttributeScaled{ Target, Attr.Agility,  0.5 } },
		}
	}
};
```

### 7.5 单条修正器：`AttachMod` / `DetachMod`

```cpp
// 施加一条修正器。返回是否成功（实体未注册 / 目标属性缺失 ⇒ false）
bool AttachMod(FTcsCombatEntityHandle Entity, const FTcsAttrModDef& Def,
	const FTcsParamValue& Value, FTcsSourceHandle Source,
	const FGameplayTagContainer& Tags = {});        // 本条携带的标签（供读取方筛选，§7.2）

// 按撤销锚点级联摘除。一个来源可施加多个定义 ⇒ **可能触及多个属性**，故返回条数而非布尔
int32 DetachMod(FTcsCombatEntityHandle Entity, FTcsSourceHandle Source);
```

> **`Tags` 是形参、不是返回值**：它是**这条修正器自己的属性**（「我是转换产物」），由施加方在挂的当场声明。若让 `AttachMod` 返回句柄再补标签，就多出一个「已挂上但还没贴标签」的中间态——那段窗口里若有读取发生，读到的是**不含该标签**的修正器，消环随之失效。

**两条都改三样东西**（顺序固定）：

| # | 动作 | 为什么 |
|---|---|---|
| ① | 改 `Inst.Mods`（增/删条目） | 这是**结构变化**——修正器集合变了 |
| ② | 置 `Graph.bDirty = true` | 结构变了 ⇒ 依赖图必须重建（§5.2 的不变式） |
| ③ | 调 `MarkWrittenModifier(Entity, 槽位, 该条在 Mods 里的下标)`（§5.3） | 登记待重算**节点** |

**两条都 MUST NOT 直接调 `SettleWrites`，也 MUST NOT 直接调 `RebuildGraph`**——两者都只在批机制的落点触发。`AttachMod` 登记目标槽位；`DetachMod` 把该 `Source` 触及的**每一个**槽位逐个登记——一次 `DetachMod` 可能跨多个属性，但重建与结算各由批机制统一触发一次。

> **② 是 §5.2 补上的**：原稿写「依赖图只在 `ApplyAttributeSet` 之后重建」，`AttachMod` 漏了置脏。后果是**运行时挂的 `AttributeScaled` 修正器不建边**——属性变化后不重算、不报错、无日志。

> **连续给同一属性挂多条，仍会重算多次**：虽然重建在批归零点只做一次，但批外连挂 5 条仍是 5 次结算。这是「属性集是唯一增删入口」（§7.1）的理由——批量施加走 `ApplyAttributeSet`，整批重建与结算各一遍。

`DetachMod` 按 `Source` 摘除该锚点下的全部条目（一个来源可施加多个定义，见 `运行时实例.md` §1.3.3）。

---

### 7.6 直写基值：`SetBaseValue`

```cpp
// 只改一个段的基值，不动修正器集合
bool SetBaseValue(FTcsCombatEntityHandle Entity, FGameplayTag AttrTag,
	double NewValue, ETcsAttrSegment Segment = ETcsAttrSegment::Basic);
```

**用在哪**：

| 场景 | 为什么走这里 |
|---|---|
| **资源属性**（当前生命 / 当前法力） | 它们 MUST NOT 挂修正器（§9.1），唯一写路径就是它 |
| **等级成长** | 宿主升级改基础值；多个属性一起改时用批（§2.2）包住 |
| **伤害 / 治疗的落点** | 伤害流程算完直接写当前生命，不产生修正器条目（§9.1） |

> **它与 `AttachMod` 的分工**：`SetBaseValue` 改的是「属性自己的状态」，`AttachMod` 改的是「外部对它的影响」。两者都进权威面（§2.1），但前者直接、不可撤销，后者可按住 `Source` 撤销。

> **写入的节点由 `Segment` 直接决定**：`Basic` ⇒ `MarkWrittenSegment(…, ETcsAttrSegment::Basic)`；`Extra` ⇒ `Extra`。**只影响该段节点及其下游**——改 Extra 基值不会唤醒依赖 Basic 段节点的修正器。

> **MUST NOT 用它写「某个 Buff 加的值」**：那是修正器，写成基值会让它无法被移除，且 `RemoveAttributeSet` 之后依然残留。

**升级事务的完整写法**：

```cpp
BeginBatch(Target);                              // 一次批
SetBaseValue(Target, Attr.Strength,  12.0);
SetBaseValue(Target, Attr.Agility,    8.0);
SetBaseValue(Target, Attr.MaxHealth, 350.0);
CommitBatch(Target);                             // 归零 → 一次全图重算 + 每属性最多广播一次
```

> **不开批会怎样**：三次 `SetBaseValue` = 三次独立结算，`Attack`（派生自 `Strength`）会被重算两次以上。**功能正确，只是白算**——这就是批存在的全部理由。

---

## 8. 读口 ✅

### 8.1 总值

```cpp
// 返回 CachedCurrent。不重算、不发事件
bool TryGetValue(FTcsCombatEntityHandle Entity, FGameplayTag Attribute, double& OutValue) const;
```

### 8.2 段值（供技能系数引用）

```cpp
// 返回某段的段内公式结果（不含全局层）。Part MUST 为 Basic 或 Extra
bool TryGetSegmentValue(FTcsCombatEntityHandle Entity, FGameplayTag Attribute,
	ETcsAttrReadPart Part, double& OutValue) const;
```

**用途**：LoL 的「造成 60% bonus AD 伤害」。段值 = `(Base + ΣAdd) × (1 + ΣPercentAdd)`，不含 `ΠMul` / `ΣFlatAdd` / 全局覆盖。

**为什么不用明细入口**：明细要重新摊平并构造 `TArray`，且被明令「不进热路径」；技能伤害计算是热路径。

### 8.3 明细（UI / 调试）

直接调《数值聚合器》§3.3 的 `EvaluateAggregatedDetail`。属性域的义务是：

> **MUST 提供「按当前状态重新摊平 + 调明细入口」的路径，且与缓存值走同一条 `CollectLayerEntries`**（§3.4），否则面板显示的和实际算的是两个数。

明细 **MUST NOT** 进热路径、**MUST NOT** 进网络复制，只用于本地展示与调试。

---

## 9. 边界清单 ✅

### 9.1 资源属性 MUST NOT 挂修正器

聚合公式是 `(Base + ΣAdd) × …`。若「当前生命值」的基值是当前血量，再挂一条 `+50`，结果是「凭空涨血」——语义崩坏。

| 类 | Base 语义 | 能否挂修正器 |
|---|---|---|
| **数值属性**（力量、最大生命、防御、暴击率…） | 基础值，由配置/等级决定 | ✅ |
| **资源属性**（当前生命、当前法力…） | **当前存量**，由游戏逻辑高频改写 | ❌ |

**伤害/治疗走什么**：伤害流程直接 `SetBaseValue`（高频写入不能产生修正器条目）。「受到的治疗 +20%」这类作用于**治疗事件**，不作用于资源属性本身。

**判定**：`UTcsAttributeDef` 上 MUST 有一个标记区分两类；`IsDataValid` 拒绝给资源属性配修正器行。

### 9.2 它不做什么

| 不做 | 谁做 |
|---|---|
| 折叠公式 | 《数值聚合器》 |
| 决定「谁该进聚合」 | 调用方（状态域 / 装备 / 伤害流程） |
| 求值修正器的数值 | 参数域（`FTcsParamValue` 的源） |
| 决定属性集包含哪些属性 | `UTcsAttributeSetAsset`（配置） |
| 网络复制 | 未定（§10） |

### 9.3 可重放约束

> **属性域 MUST NOT 引入不可重放的输入**（随机数、本地时间、客户端特有状态）。

这条不是为今天——是为将来加复制时留路。V1 的姿态是「操作复制 + 客户端重算」，它成立的前提就是这条。

---

## 10. 未决

| # | 事项 | 缺什么裁决 |
|---|---|---|
| 1 | **网络复制姿态** | 三个消费者里只有动作游戏需要复制，但它还没到实现阶段。按铁律 ① 不预建。见族外 `未决-核心基线.md` |
| 2 | **写时重算的性能边界** | **批外**成本 ∝「写入次数 × 依赖链深度」；**批内**压到一次全图重算。LAC（30–150）、动作游戏（2）无压力；**幸存者（1000–5000）MUST 压测后再定**是否需要改为帧末结算 |

**已评估、否决**：`ValueDomain.Custom`（零实现者，特殊映射归参数源）；单属性精确订阅（需在属性域内再造一套订阅表）；**读时惰性重算**（§2.2——跨实体读会连锁唤醒另一侧求值，且宿主源回调可导致求值中途重入）；完整定义拷贝进实例（Def 改后不跟随，且多一份真相）。
