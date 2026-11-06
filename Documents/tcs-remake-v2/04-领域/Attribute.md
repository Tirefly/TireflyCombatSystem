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

### 1.3 容器：`FTcsAttributeStore`

```cpp
struct FTcsAttributeStore
{
	// 槽位 = 下标。只增不减（bActive=false 不删槽位）⇒ 下标在实体存活期内稳定
	TArray<FTcsAttributeInstance> Instances;

	// tag → 槽位。运行期唯一查找入口
	TMap<FGameplayTag, int32> TagToSlot;

	// 反向依赖：下标对齐 Instances。Dependents[i] = 依赖槽位 i 的那些槽位
	TArray<TArray<int32>> Dependents;

	// 拓扑序：谁该先算（重建依赖图时生成，MUST 稳定）
	TArray<int32> EvalOrder;

	// 批：嵌套计数 + 待结算槽位（§2.2）。批内只登记，CommitBatch 归零时一次结算
	int32         BatchDepth = 0;
	TArray<int32> BatchWritten;
};
```

> **Why Not 用 `TMap<FGameplayTag, FTcsAttributeInstance>`**：`TMap` 没有「槽位」概念，`Bounds.Dynamic` 只能存 tag（每次用都要再哈希）或指针（扩容会搬移元素地址）。`TArray` 的下标不受扩容影响，配合「槽位只增不减」即可当稳定句柄用。

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
| **批内**（`BatchDepth > 0`） | 只把槽位登进 `BatchWritten`，**不算**；等 `CommitBatch` 归零时一次结算 |

**读路径（§8）**：直接返回 `CachedCurrent`，**不重算、不发事件、零副作用**。批内读到的就是批开始前的值。

> **批内读到旧值是有意的**：批的语义是「这一批改动要么一起生效、要么都还没生效」。让外部读到「一半生效」的中间态，只会让调用方更难推理。

**事件路径**：结算时 `New != Old`（阈值 `1e-5`）⇒ 按 `(Entity, Attribute)` 记一条待发变更；所有待发变更在帧末按实体归组合并派发（§6.4）。批内对同一属性的多次改动合并成一条，保留首次 `Old`、末次 `New`。

```cpp
// 批：嵌套计数。真实消费者 = 等级成长这类宿主升级事务（§7.6）
void BeginBatch(FTcsCombatEntityHandle Entity);   // BatchDepth++
void CommitBatch(FTcsCombatEntityHandle Entity);  // BatchDepth--；归零时才 SettleWrites(全部 BatchWritten)
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

```cpp
void CollectEntries(const FTcsAttributeInstance& Inst,
	TArray<FTcsAggregateEntry>& OutBasic,
	TArray<FTcsAggregateEntry>& OutExtra,
	TArray<FTcsAggregateEntry>& OutGlobal)
{
	for (const FTcsAttrModInstance& Mod : Inst.Mods)
	{
		const double V = EvalModValue(Inst, Mod);   // §7.2：Literal 直取 / AttributeScaled 现场读
		const FTcsAggregateEntry Entry{ Mod.Def->Op, V, Mod.Def->Priority, Mod.Source };

		switch (Mod.Def->Layer)
		{
		case ETcsAttrLayer::Basic:  OutBasic.Add(Entry);  break;
		case ETcsAttrLayer::Extra:  OutExtra.Add(Entry);  break;
		case ETcsAttrLayer::Global: OutGlobal.Add(Entry); break;
		}
	}
}

double Evaluate(const FTcsAttributeInstance& Inst, /* edges */)
{
	TArray<FTcsAggregateEntry> Bands[3];   // Basic / Extra / Global
	CollectEntries(Inst, Bands[0], Bands[1], Bands[2]);

	const FTcsAggregateSegment Segs[2] = {
		{ Inst.BasicValue, Bands[0] },
		{ Inst.ExtraValue, Bands[1] },
	};

	// 段值单独留一份（§8.2 的读口要用）
	// 值入口与明细入口共用同一个实现体，见《数值聚合器》§3
	const double Folded = EvaluateAggregatedValue(Segs, Bands[2], Inst.Def->Strategy);
	return ApplyValueDomain(Folded, Inst);   // §4
}
```

> **`CachedSegment[]` 在求值时一并填**：段内公式结果本来就是折叠的中间量，顺手存下来，让 §8.2 的段值读口变成 O(1)。

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

### 5.2 建立时机

**只在 `ApplyAttributeSet` / `RemoveAttributeSet` 之后全量重建**，不在求值路径、也不在每个 `AttachMod` 之后。

```cpp
void RebuildGraph(FTcsAttributeStore& Store)
{
	for (TArray<int32>& Deps : Store.Dependents) { Deps.Reset(); }

	for (int32 Slot = 0; Slot < Store.Instances.Num(); ++Slot)
	{
		const FTcsAttributeInstance& Inst = Store.Instances[Slot];
		if (!Inst.bActive) { continue; }

		// ① 边界依赖
		ResolveBoundEdge(Store, Slot, Inst.Def->Bounds.Min);
		ResolveBoundEdge(Store, Slot, Inst.Def->Bounds.Max);

		// ② 值依赖：递归遍历每条修正器的取值子树
		for (const FTcsAttrModInstance& Mod : Inst.Mods)
		{
			ForEachAttributeRead(Mod.Operand, [&](FGameplayTag SrcTag, ETcsAttrReadSide Side)
			{
				if (Side != ETcsAttrReadSide::Target) { return; }   // Instigator 侧不是本实体的依赖
				if (const int32* SrcSlot = Store.TagToSlot.Find(SrcTag))
				{ Store.Dependents[*SrcSlot].AddUnique(Slot); }
				else { /* 目标不存在：配置错误，见 §5.4 */ }
			});
		}
	}

	// ③ 拓扑序 + 环检测
	BuildEvalOrder(Store);
}
```

> **`ForEachAttributeRead` 要递归**：`Composite` 可以嵌套 `AttributeScaled`，只看顶层字段会漏边。

### 5.3 结算：沿反向边扩散，按拓扑序重算

```cpp
// 结算：收集受影响槽位，按拓扑序重算。
// 调用点只有两个：MarkWritten（批外，单槽位）/ CommitBatch（批归零，整批）——见本节末
void SettleWrites(FTcsCombatEntityHandle Entity, TConstArrayView<int32> WrittenSlots)
{
	FTcsAttributeStore& Store = Stores[Entity];

	// ① 扩散：收集所有受影响的槽位（含被写的那些自己）
	ScratchAffected.Reset();
	for (int32 Slot : WrittenSlots) { ScratchAffected.AddUnique(Slot); }
	for (int32 i = 0; i < ScratchAffected.Num(); ++i)
	{
		for (int32 Dep : Store.Dependents[ScratchAffected[i]])
		{ ScratchAffected.AddUnique(Dep); }
	}

	// ② 按拓扑序重算，顺序 MUST 稳定
	for (int32 Slot : Store.EvalOrder)
	{
		if (!ScratchAffected.Contains(Slot)) { continue; }
		RecomputeAndRecordDelta(Entity, Slot);
	}
}
```

> **必须按拓扑序，不能深度优先递归**：菱形依赖（`Strength → {Attack, Defense} → PowerScore`）下，深度优先会把 `PowerScore` 算两次，且第一次算时 `Defense` 尚未更新。
> **`AddUnique` 去重**是菱形依赖正确的前提。

> **每个槽位只算一次**：`ScratchAffected` 用 `AddUnique` 保证集合内唯一，② 又只遍历一次 `EvalOrder` ⇒「A 依赖 B，A 和 B 同批被写」时 A **只算一次**，且在 B 之后算。

> **批量入口靠批，不靠「传全」**：`ApplyAttributeSet` / `RemoveAttributeSet` 内部自己开一个批（`BeginBatch` … `CommitBatch`），逐槽位 `MarkWritten` ⇒ 批外时整批只结算一次、外层已有批时并入外层。**不需要**另造一个「批量 `MarkWritten`」的 API。单属性写（`SetBaseValue` / `AttachMod` / `DetachMod`）走 `MarkWritten` 的单槽位路径。

> **跨操作不去重**：先 `SetBaseValue(B)`、再 `SetBaseValue(A)` 是两次写，A 算两次——这是对的，两次写各自都改变了 A 的输入，每次返回时值都必须已是最新（§2.2）。去重只发生在**同一次结算内部**。跨调用要去重，用批（§2.2 `BeginBatch`）。
**`SettleWrites` 的调用点只有两类**：

| 调用者 | 何时 | 传什么 |
|---|---|---|
| `MarkWritten`（写入口内部） | **批外**，每次写之后立刻一次 | 刚被写的那一个槽位 |
| `CommitBatch` | 批计数归零时 | **整批** `BatchWritten` |

**五个写入口（§7）都 MUST NOT 直接调 `SettleWrites`**——它们只调 `MarkWritten`，由批机制决定是立刻结算还是攒着。这样「批」只有一个开关注入口，不会出现某个写入口绕过批的情况。

```cpp
void MarkWritten(FTcsCombatEntityHandle Entity, int32 Slot)
{
	FTcsAttributeStore& Store = Stores[Entity];

	if (Store.BatchDepth > 0)
	{
		Store.BatchWritten.AddUnique(Slot);   // 批内：只登记
		return;
	}

	SettleWrites(Entity, MakeArrayView(&Slot, 1));   // 批外：隐式批，立即结算
}

void CommitBatch(FTcsCombatEntityHandle Entity)
{
	FTcsAttributeStore& Store = Stores[Entity];
	if (--Store.BatchDepth > 0) { return; }   // 还有外层批，不结算

	if (Store.BatchWritten.Num() > 0)
	{
		SettleWrites(Entity, Store.BatchWritten);
		Store.BatchWritten.Reset();
	}
}
```

### 5.4 环检测与拓扑序

- **环 MUST 在 `ApplyAttributeSet` / `RemoveAttributeSet` 时报出**：检测到环 ⇒ 整次操作失败，不做部分应用。
- **`EvalOrder` MUST 稳定**：Kahn 算法在候选集里取槽位下标最小者，不依赖哈希遍历顺序。否则浮点求值顺序会变、事件顺序会漂、回放会分叉。
- **编辑器期已有一道更早的关卡**（`UTcsDefValidationSubsystem`）：属性定义是全项目静态集合，全局无环 ⇒ 任何实体的子集都无环。运行期这次是**兜底**（应对热重载、运行时注册、脚本生成资产）。
- **`AttributeScaled` 的目标不存在**：报错。这不是兜底——该属性确实算不出值。

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
- `RemoveAttributeSet` 若会让它归零 ⇒ **整次操作失败**（全校验通过后才改）

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

**两条都原子**：全校验通过后才改，失败整次不应用。

**`ApplyAttributeSet` 流程**：

```text
① 逐 tag：表里没有 → 建槽位、从 Def 拷基值、挂派生内置条目（**不在此求值**）
② 逐 tag：RefCount++
③ 重建依赖图 + 拓扑序 + 环检测
④ 内部开一个批：逐 tag `MarkWritten`，然后 `CommitBatch`（§2.2）
   —— 批外时 = 整批一次结算；外层已开批时，嵌套计数并入外层，不提前算
```

> **Why Not 在步骤 ① 里就求值**：两个原因。① **依赖图还没建**（步骤 ③ 才建），若 A 依赖 B 而 B 排在后面的 tag，A 求值时 B 的槽位还不存在；② 步骤 ④ 反正要整体结算一遍，步骤 ① 那次是白算——同一批里新建的槽位会被算两次。

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

	// 冻结位（仅 Mode == Snapshot 时有效；何时冻结归参数域，冻在哪归本域）
	double SnapshotValue = 0.0;
	bool   bHasSnapshot  = false;
};
```

> **`FTcsAttrModDef` MUST NOT 持有 `FTcsParamValue`**：它被状态域、技能域、装备、纯 C++ 代码共同使用，绑死参数域会锁死泛用性。值由施加方填。

> **为什么成员叫 `Operand` 而不是 `Value`**：`FTcsParamValue` 自身有个 `Source` 成员（值从哪来，装 `FTcsParamSource_*` 族的实例），而本结构另有一个 `Source`（谁施加的，`FTcsSourceHandle`）。若成员叫 `Value`，同一结构体里就会出现 `Value.Source` 与 `Source` 并排、一词两义。`Operand` 与 `Op` 是本域的既有配对（运算带 + 运算数）。至于 `FTcsParamValue` 这个名字本身是否合适，归参数域裁决。

**求值**：

```cpp
double EvalModValue(const FTcsAttributeInstance& Inst, const FTcsAttrModInstance& Mod)
{
	if (Mod.Def->Mode == ETcsParamMode::Snapshot && Mod.bHasSnapshot)
	{ return Mod.SnapshotValue; }

	FTcsParamEvalContext Ctx = MakeEvalContext(Inst.DefTag, /* bSnapshot = */ false);
	return Mod.Operand.Eval(Ctx);    // AttributeScaled 在此登记依赖边
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
	Derived.Source  = FTcsSourceHandle::MakeBuiltin();
	Inst.Mods.Add(MoveTemp(Derived));
}
```

| 属性 | 值 |
|---|---|
| `Source` | `MakeBuiltin()`（保留值，`运行时实例.md` §1.3.1） |
| 可否被 `RemoveAttributeSet` 摘除 | **否**——它随实例生死 |
| UI 列出修正器时 | **跳过**（`Source.IsBuiltin()`） |

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
bool AttachMod(FTcsCombatEntityHandle Entity, const FTcsAttrModDef& Def,
	const FTcsParamValue& Value, FTcsSourceHandle Source);

// 按撤销锚点级联摘除。一个来源可施加多个定义 ⇒ **可能触及多个属性**，故返回条数而非布尔
int32 DetachMod(FTcsCombatEntityHandle Entity, FTcsSourceHandle Source);
```

**两条都只调 `MarkWritten`（§5.3）**，不直接调 `SettleWrites`。`AttachMod` 登记目标槽位；`DetachMod` 把该 `Source` 触及的**每一个**槽位逐个登记——一次 `DetachMod` 可能跨多个属性，但结算仍由批机制统一触发一次。

> **连续给同一属性挂多条，就要重算多次**：`AttachMod` 一次只结算一个槽位，挂 5 条就重算 5 次。这是「属性集是唯一增删入口」（§7.1）的又一个理由——批量施加走 `ApplyAttributeSet`，整批只结算一遍。

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
// 返回某段的段内公式结果（不含全局层）。Layer MUST 为 Basic 或 Extra
bool TryGetSegmentValue(FTcsCombatEntityHandle Entity, FGameplayTag Attribute,
	ETcsAttrLayer Layer, double& OutValue) const;
```

**用途**：LoL 的「造成 60% bonus AD 伤害」。段值 = `(Base + ΣAdd) × (1 + ΣPercentAdd)`，不含 `ΠMul` / `ΣFlatAdd` / 全局覆盖。

**为什么不用明细入口**：明细要重新摊平并构造 `TArray`，且被明令「不进热路径」；技能伤害计算是热路径。

### 8.3 明细（UI / 调试）

直接调《数值聚合器》§3.3 的 `EvaluateAggregatedDetail`。属性域的义务是：

> **MUST 提供「按当前状态重新摊平 + 调明细入口」的路径，且与缓存值走同一条 `CollectEntries`**，否则面板显示的和实际算的是两个数。

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
