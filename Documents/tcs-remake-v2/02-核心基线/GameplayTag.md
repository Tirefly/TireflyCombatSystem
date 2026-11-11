# 核心基线 · 游戏标签（GameplayTag）

> **定位**：V2 核心基线之一。**本文只承载「标签的判定工具」这一个内容。**
> **回答什么问题**：**一组标签，是否满足某个要求**。

**审核状态图例**：✅ 已裁决并批准 ｜ ⚠️ 待审 ｜ ❓ 待调研

> **本文件的写法**：只写最终设计与已裁决的架构，不复述历史实现。

> **本文件预期会扩充**：裁决 State / Buff / Skill 三个域时，`CategoryTags` 的筛选规则大概率并入本节。**现在只有 `FTcsTagRequirements` 一项内容**——它已有真实消费者，不是占位。

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
| ① | 被检查的标签集为**空**，`IgnoreTags` 非空 | **通过**——`HasAny(空集)` 恒为 `false` |
| ② | 被检查的标签集为**空**，`RequireTags` 非空 | **不通过**——`HasAll(空集)` 恒为 `false` |
| ③ | 两个集合都空 | **恒通过**（`IsEmpty()` 为真，调用方可据此走快路径） |

> **② 是最容易踩的一脚**：给 `RequireTags` 填了值，就**等于排除所有无标签的对象**。配置时若只想「额外要求某些标签」，同时还要保留无标签的对象，必须把过滤做成 `IgnoreTags` 而不是 `RequireTags`。

> **① 是弗拉米尔那类转换需求成立的前提**：普通装备加成不带任何标签 ⇒ `IgnoreTags` 永远排除不掉它 ⇒ 它必然计入。

### 1.3 Why 自建而不直接用 GAS 的

**`FGameplayTagRequirements` 住在 `GameplayAbilities` 模块，而 TCS 不依赖 GAS。**

它的内核只是 `FGameplayTagContainer` 的两次判定（`HasAll` / `HasAny`），没有别的东西——自建成本约二十行，换掉一条对 GAS 的模块依赖。

### 1.4 Why Not 搬 `FGameplayTagQuery` 位

GAS 的 `FGameplayTagRequirements` 还有第三个字段 `TagQuery`（`GameplayEffectTypes.h:1543`）。**本基线不建它**，理由两条：

1. **本仓既有裁决对它有明确保留**：`TcsStateRelation.h:18` 原文「故本仓 **MUST NOT 引入 `FGameplayTagQuery`**」，理由是**容器 + 层级匹配**（`HasAny` 对父词天然覆盖一族）已经能表达类目筛选，多引入一层查询文法只会多一个不可静态校验的配置面；
2. **零消费者**：`RequireTags` + `IgnoreTags` 已够表达今天全部真实需求，`TagQuery` 能表达而它们不能表达的形态，今天没有任何一处需要。

> **重开条件 = 出现一个 `RequireTags` / `IgnoreTags` 表达不了、且真实存在的需求**（例如「A 与 B 至少一个」这种或语义）。到那时按铁律 ① 补，不是现在。

---

## 2. 消费者（铁律 ①）

| 消费者 | 用途 | 状态 |
|---|---|---|
| **Attribute 域**的 `FTcsParamSource_AttributeScaled::ModifierTagRequirements` | 读某属性时按标签筛掉部分修正器（弗拉米尔类转换需求） | ✅ **已落地依据** |
| **State / Buff / Skill 三域的类目筛选** | `CategoryTags` 匹配（关系表族规则 / 条目选择器） | ✅ **需求已确定**，机制待那三域裁决时细化 |

**「≥2 个域依赖」已成立**：Attribute 已是第一个，State / Buff / Skill 的类目筛选是既定的第二、三、四个消费者——三域都已有 `CategoryTags` 字段或选择器需求（V1 实证：`FTcsStateDefBase.CategoryTags`、`FTcsSkillEntrySelector.ByCategoryTags`、关系表族规则），它们要做「按类目筛选」就必须有本类型的判定能力。

> **为什么现在不写它们的具体规则**：那三域各自裁定 `CategoryTags` 的匹配语义（层级？精确？）时才有定论。本文件只提供判定外壳，规则随域落。**这不是「预建插槽」——是「消费者已确定、形态由第一个消费者（Attribute）定下」。**

---

## 3. 归属

**核心基线**。判据（[01-定位与判据](../01-定位与判据.md)）：

| 判据 | 满足 |
|---|---|
| 不含领域词汇 | ✅ 只有 `RequireTags` / `IgnoreTags` / 两个判定函数，无任何域词 |
| 被 ≥2 个领域依赖 | ✅ **Attribute（已落地）+ State / Buff / Skill（需求已确定，见 §2）** |

> **铁律 2 的门槛管的是接口，不是值类型**：`FTcsTagRequirements` 是「一个值类型 + 两个纯函数」，没有「实现者」这个概念，故不适用「2 个实现者」这条。它的归属由**消费者数量＋零领域词汇**判定。

> **模块层未定**：V2 的模块拆分要等所有域裁完再判（[INDEX](../INDEX.md) §2.4）。本文只裁定「它属于核心基线这一层」，**MUST NOT** 据此推断它会独立成一个模块。

---

## 4. 未决

**无。**
