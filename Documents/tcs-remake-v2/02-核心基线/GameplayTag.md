# 核心基线 · 游戏标签（GameplayTag）

> **定位**：V2 核心基线之一。**本文只承载「标签的判定工具」这一个内容。**
> **回答什么问题**：**一组标签，是否满足某个要求**。

**审核状态图例**：✅ 已裁决并批准 ｜ ⚠️ 未定 ｜ ❓ 待调研

> **本文件的写法**：只写最终设计与已裁决的架构，不复述历史实现。

> **本文件范围**：后续领域裁决若出现新的零领域词汇标签判定工具，可并入本文件。**现在只有 `FTcsTagRequirements` 一项内容**；待全部正式领域定稿后按真实消费者域数量复核归属（见 §2、§3）。

---

## 0. 本文件只回答一件事

**给定一组标签，判断它是否满足某个要求。**

它**不**负责：

| 不做 | 归谁 |
|---|---|
| 标签的根段划分、声明位置、改名重定向 | 工程规范（`unreal-gameplay-tags`），不是运行时机制 |
| 标签的存储与查询（`FGameplayTagContainer` 本身） | 引擎 `GameplayTags` 模块 |
| 「某个域的类目筛选规则」（`CategoryTags` 怎么用） | 那个域（State / Skill） |

> **判据**：本文只住**零领域词汇**的判定工具。一旦某个函数的签名里出现「状态」「技能」「修正器」这类词，它就属于那个域，不属于本基线。

---

## 1. `FTcsTagRequirements` ✅

### 1.1 形状

```cpp
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsTagRequirements
{
	GENERATED_BODY()

// 要求
#pragma region Requirements

public:
	// 必须有这些标签（空 = 不要求）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag Requirements")
	FGameplayTagContainer RequireTags;

	// 有其中任意一个就不通过（空 = 不排除）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag Requirements")
	FGameplayTagContainer IgnoreTags;

#pragma endregion


// 判定
#pragma region Query

public:
	/**
	 * 给定标签集是否满足本要求。
	 *
	 * @param Tags 待检查的标签集。
	 * @return 返回是否满足（`RequireTags` 全部命中，且 `IgnoreTags` 一个都没命中）。
	 */
	bool RequirementsMet(const FGameplayTagContainer& Tags) const
	{
		if (!Tags.HasAll(RequireTags)) { return false; }
		return !Tags.HasAny(IgnoreTags);
	}

	// 两个集合都空（= 不设任何要求，任何标签集都通过）
	FORCEINLINE bool IsEmpty() const
	{
		return RequireTags.IsEmpty() && IgnoreTags.IsEmpty();
	}

#pragma endregion
};
```

**判定只有两行**：`RequireTags` 必须全部命中，`IgnoreTags` 一个都不能命中。**没有第三种语义**。

### 1.2 三条语义边界（MUST 读）

| # | 情形 | 结果 |
|---|---|---|
| ① | 待检查的 `Tags` 为**空**，`IgnoreTags` 非空（且 `RequireTags` 为空） | **通过**——空集合不含任何标签，`Tags.HasAny(IgnoreTags)` 为 `false`（同理，当参数 `IgnoreTags` 为空时 `Tags.HasAny(空集)` 亦恒为 `false`） |
| ② | 待检查的 `Tags` 为**空**，`RequireTags` 非空 | **不通过**——空集合无法覆盖非空要求，`Tags.HasAll(RequireTags)` 为 `false`（注：当参数 `RequireTags` 为空时 `Tags.HasAll(空集)` 恒为 `true`，不要把调用者为空与参数为空混淆） |
| ③ | `RequireTags` 与 `IgnoreTags` 都空 | **恒通过**（`IsEmpty()` 为真，调用方可据此走快路径） |

> **② 是最容易踩的一脚**：给 `RequireTags` 填了值，就**等于排除所有无标签的对象**。配置时若只想「排除某些标签」，同时还要保留无标签的对象，必须把过滤做成 `IgnoreTags` 而不是 `RequireTags`。

> **① 是弗拉米尔那类转换需求成立的前提**：在 `RequireTags` 为空、仅用 `IgnoreTags` 排除特定转换标签的前提下，普通装备加成不带任何标签 ⇒ `IgnoreTags` 永远排除不掉它 ⇒ 它必然计入。

### 1.3 Why 自建而不直接用 GAS 的

**`FGameplayTagRequirements` 住在 `GameplayAbilities` 模块，而 TCS 不依赖 GAS。**

它的内核只是 `FGameplayTagContainer` 的两次判定（`HasAll` / `HasAny`），没有别的东西——自建成本约二十行，换掉一条对 GAS 的模块依赖。

### 1.4 Why Not 搬 `FGameplayTagQuery` 位

GAS 的 `FGameplayTagRequirements` 还有第三个字段 `TagQuery`（`GameplayEffectTypes.h:1543`）。**本基线不建它**，理由两条：

1. **容器 + 层级匹配已经够用**：`HasAny` 对父词天然覆盖一族，已经能表达类目筛选，多引入一层查询文法只会多一个不可静态校验的配置面 ⇒ **MUST NOT 引入 `FGameplayTagQuery`**；
2. **零消费者**：`RequireTags` + `IgnoreTags` 已够表达今天全部真实需求，`TagQuery` 能表达而它们不能表达的形态，今天没有任何一处需要。

> **重开条件 = 出现一个 `RequireTags` / `IgnoreTags` 表达不了、且真实存在的需求**（例如「A 与 B 至少一个」这种或语义）。到那时按铁律 ① 补，不是现在。

---

## 2. 预期消费者与复核口径（铁律 ①）

> **领域层尚未逐项定稿**（见 [INDEX](../INDEX.md) §3–§4：七个正式领域当前均为占位文档，Attribute 草稿已退回族外，Buff 归属尚未裁决）。因此本节**不把未裁决的领域草稿或未定归属的 Buff 域写成已落地依据**。

| 预期消费面 | 典型用途 | 当前状态 |
|---|---|---|
| **属性 / 参数域** | 按标签筛选修正器或源条目（如弗拉米尔类排除特定转换标签） | 随领域裁决确认具体字段与接入点 |
| **State / Skill 等域的类目筛选** | 基于 `CategoryTags` 的包含 / 排除判定 | 随各领域裁决确认匹配规则与接入点 |

> **为什么先收口这一最小外壳**：`FTcsTagRequirements` 只是对 `FGameplayTagContainer::HasAll / HasAny` 的约二十行零领域词汇封装，用来替代对 GAS `GameplayAbilities` 模块的依赖；各域具体的筛选规则留在各域裁决，不提前写入本文件。

---

## 3. 归属与复核条件

**暂列核心基线（基础值工具），待全部正式领域裁决完成后按真实消费者域数复核。** 判据（[01-定位与判据](../01-定位与判据.md)、[INDEX](../INDEX.md) §2.2 & §2.5）：

| 判据 | 状态 |
|---|---|
| 不含领域词汇 | ✅ 只有 `RequireTags` / `IgnoreTags` / 两个纯判定函数，无任何域词 |
| 被 ≥2 个领域依赖 | ⏳ **待全部正式领域定稿后核实**——若定稿后确认有 ≥2 个正式域使用则留在核心基线；若仅单一域使用，则按 [INDEX](../INDEX.md) §2.5 降格进该领域 |

> **铁律 2 的门槛管的是策略插槽 / 接口，不是值类型**：`FTcsTagRequirements` 是「一个值类型 + 两个纯函数」，没有「实现者」概念，不适用「≥2 个实现者」判据；其留存仅由**零领域词汇 + 最终真实消费者域数**判定。

> **模块层未定**：V2 的模块拆分要等所有域裁完再判（[INDEX](../INDEX.md) §2.4）。本文只裁定「它属于核心基线这一层」，**MUST NOT** 据此推断它会独立成一个模块。

---

## 4. 未决

**无。**
