// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsSkillEntrySelector.generated.h"



/**
 * 技能条目选择器四档（D5-9 的 `FEntrySelector.Mode`）。
 *
 * **枚举值前缀 ESS_ 是 (Skill)EntrySelectorMode 的缩写。**
 *
 * **命名史（2026-10-09 用户裁定改名，MUST 留痕）**：本类型原名 `ETcsEntrySelectorMode` +
 * `FTcsEntrySelector`（无 `Skill` 段）。改名的理由 = **仓内唯一性可读**：`TcsStateParam` 的先例
 * 表明"某个名字看起来是通用的、其实只有一个域在用"会在将来第二个域真出现时逼出改名
 * （那时改动面是整个公开面）。本类型**今天与可预见的将来都只服务技能账本条目**（状态侧没有
 * "已学条目"这一层）⇒ 名字里 MUST 带 `Skill`。
 */
UENUM(BlueprintType)
enum class ETcsSkillEntrySelectorMode : uint8
{
	ESS_All = 0			UMETA(DisplayName = "全部条目", ToolTip = "该单位的全部在册条目都命中（默认）"),
	ESS_ByDefTag = 1	UMETA(DisplayName = "按定义身份", ToolTip = "按条目的 DefTag 匹配——层级匹配，父 tag 命中其全部子技能"),
	ESS_ByCategoryTags = 2	UMETA(DisplayName = "按类别标识（未实现）", ToolTip = "按定义的类别标识容器筛选（HasAll / HasAny 等）——载体与规则未落地，本轮具名报出且零施加"),
	ESS_Custom = 3		UMETA(DisplayName = "自定义", ToolTip = "宿主自定义片段——契约未实现，本轮具名报出且零施加"),
};



/**
 * 技能条目选择器（D5-9）：表达"**外部施加**的修正器作用到哪些已学条目"。
 *
 * **四档各有字段载体**（`Mode` 声明的每一档都有对应字段，MUST NOT 出现"枚举有档、字段缺失"）。
 *
 * **它只服务外部施加**：定义侧 `FTcsSkillDefData::ParamChainRows` **MUST NOT** 接受本类型——
 * 那些行的声明作用域恒为**本条目自身**（同一件事不许两个入口）。
 *
 * ## 两处 2026-10-09 用户裁定的形状修正（MUST 读，防后人"照着旧稿改回去"）
 *
 * **① `ByTag` → `ByDefTag`（改名，不是新增）**：原档名 `ByTag` 语义含混——它读的是条目的
 * `DefTag`（内容身份），而"Tag"一词在 TCS 里同时指属性词/状态词/事件词/参数键等**七八种**东西。
 * 改名后与字段名 `DefTag` 对齐，读代码即可知"按哪个 tag"。
 *
 * **② 原 `ById` 档整体删除，改为 `ByCategoryTags` 空壳（★ 本档的由来，MUST NOT 读成"漏做"）**：
 * 原 `ById` 档按 `FTcsSkillEntryHandle`（`Index` + `Generation`）精确命中。**该设计不成立**，
 * 判据三条：
 * - **它不可被作者配置**：`EntryHandle` 是**运行时产物**（桶内槽位下标 + 进程内发号代际），
 *   编辑器里没有、也无法预先填出一个合法值 ⇒ 该档**永远只能从代码里传入**，
 *   对"外部施加"这个以配置为入口的场景**等于没有这一档**；
 * - **它的"Id"是历史遗留**：所有 Def 资产的身份原为 `FName DefId`，2026-09-22 起**已全量迁移到
 *   `GameplayTag`（`DefTag`）** ⇒ "按 Id 筛"的正当形态就是**按 tag 筛**，而那已被 ① 的
 *   `ByDefTag` 覆盖（同一份内容身份，不需要两档）；
 * - **留一个只能由代码构造、且带代际的句柄过滤位**，会诱导后人用它做"长期引用的精确命中"，
 *   而**代际会让跨帧引用自然失效**（`Handle` 释放即 +1 置偶）⇒ 是"看起来能用、实际易碎"的形状。
 *
 * **本档的目标形态（用户裁定，待落地）**：按定义上的**类别标识容器**筛选
 * （`FGameplayTagContainer`：如一个 Buff 同时具"火属性伤害 + 异常状态"，一个技能同时具
 * "左手释放/右手释放/双手释放（三选一）+ 投掷类 + 引导类"）；筛选规则（`HasAll` / `HasAny` …）
 * **倾向直接复用引擎的 `FGameplayTagQuery`**（`GameplayTagContainer.h:737`，
 * `USTRUCT(BlueprintType)` + `AllTagsMatch()` / `AnyTagsMatch()` 表达式 + 自带编辑器定制）。
 *
 * **★ 本轮只留空壳，MUST NOT 落机制**（判据 = "零消费者不预建"）：
 * ① **载体不存在**——`StateDef` 侧的**类别标识容器**字段尚未新增（用户裁定"所有 StateDef 都应新增"，
 * 属**独立变更**，不在本 Task）；② **规则未拍板**——`HasAll` / `HasAny` 一类筛选规则尚无消费场景；
 * ③ **可能另立模块**——用户指出 GameplayTag 筛选机制在 TCS 里将是重要角色、
 * **甚至可能单独开一个 `TcsGameplayTag` 模块** ⇒ 现在把它写进 `TcsSkill` 内部，将来极可能要搬家。
 * ⇒ 本档今天的行为与 `Custom` 档**同款**：
 * **具名报出"未实现"且零修正被施加**（MUST NOT 静默当 `All`、MUST NOT 静默无操作）。
 * **台账登记 = `STAT-10`**（载体 + 规则 + 模块归属三项一并裁）。
 *
 * **词汇定案（2026-10-09 用户拍板）**：取 **`CategoryTags`**。判定时排除的强候选：
 * - **`SpecTag`（排除，仓内事实）**：`SPEC-02-states` / `SPEC-04-skill` / `SPEC-05-skill` /
 *   `SPEC-07-notation` 是本仓**设计文档的编号前缀** ⇒ `SpecTag` 会被读成"规格文档的 tag"；
 * - **`Descriptor`（排除）**：已被 openspec 能力名 `plugin-descriptor` 占用；
 * - `FacetTags`（分类学最精确、但可读性需解释）、`TraitTags`（与伤害分类拉开距离，但
 *   "左手/右手/双手释放"这类**档位**读感偏"特质"）留档备查。
 * **`Category` 有同族先例**：`DamageCategory` 根在 TCS 已是"供条件匹配的伤害分类集"
 * （触发侧 `FTcsCondition_HasAllTags`）——本字段是**同一形态扩到定义级**，不是新造机制。
 * 档名取复数 `ByCategoryTags`（筛的是一个容器/查询）；`ByDefTag` 保持单数（那里匹配单个身份词）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillEntrySelector
{
	GENERATED_BODY()

// 档位
#pragma region Mode

public:
	// 选择档（决定下面哪个字段被读取）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Entry Selector")
	ETcsSkillEntrySelectorMode Mode = ETcsSkillEntrySelectorMode::ESS_All;

#pragma endregion


// 各档载体
#pragma region Filters

public:
	/**
	 * 定义身份过滤（`ESS_ByDefTag` 档读取）：按条目的 `DefTag` **层级匹配**——
	 * 填父 tag 即命中其全部子技能（如填 `SkillDef` 命中 `SkillDef.Fireball`）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Entry Selector",
		Meta = (EditCondition = "Mode == ETcsSkillEntrySelectorMode::ESS_ByDefTag", EditConditionHides))
	TArray<FGameplayTag> DefTagFilter;

	/**
	 * 类别标识查询（`ESS_ByCategoryTags` 档读取）——**本轮预留字段，零消费者**。
	 *
	 * **为什么字段先落而机制不落**：`FGameplayTagQuery` 是引擎既有的可反射类型 ⇒ 落下它**零成本**
	 * （不需要新增任何自定义类型），而它让"这一档将来吃什么"在**形状上**立刻可见
	 * （否则后人只会看到一个空枚举值，无法判断该档的意图）。**MUST NOT** 在此实现任何筛选逻辑
	 * ——载体（StateDef 侧类别标识容器）与规则都未拍板，实现即"零消费者预建"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Entry Selector",
		Meta = (EditCondition = "Mode == ETcsSkillEntrySelectorMode::ESS_ByCategoryTags", EditConditionHides))
	FGameplayTagQuery CategoryTags;

	// 自定义片段（`ESS_Custom` 档读取；契约归 R6.5-g，本轮只报未实现）
	UPROPERTY(EditAnywhere, Category = "Skill Entry Selector",
		Meta = (EditCondition = "Mode == ETcsSkillEntrySelectorMode::ESS_Custom", EditConditionHides))
	FInstancedStruct CustomFragment;

#pragma endregion
};
