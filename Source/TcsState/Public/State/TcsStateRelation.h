// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "TcsStateRelation.generated.h"



/**
 * **关系行**（2026-10-16 `R5.5-e1` 立）：一行 = 一个**主体词** + 四个容器。
 *
 * **形态来源**：吸收 Lyra 的 `ULyraAbilityTagRelationshipMapping` 行形态
 * （见 `DEC-2026-10-13-relation-row-shape` §4.1）。**为什么不是「Query × Query 的族规则行」**：
 * Lyra 以同一需求证明「容器 + 层级匹配」已足够——主体词填**父词**即覆盖一族，
 * 故本仓 **MUST NOT 引入 `FGameplayTagQuery`**。
 *
 * **两个方向 MUST 分开（本类型的核心判据）**：
 *
 * | 方向 | 字段 | 谁被约束 |
 * |---|---|---|
 * | **受动**（我自己的门禁） | `RequiredTags` / `ForbiddenTags` | **我** |
 * | **施动**（我对外的影响） | `TagsToBlock` / `TagsToCancel` | **目标** |
 *
 * 两者可从**相反一侧**表达同一互斥——这不是冗余，而是**让作者选哪一侧拥有声明**：
 * - `A.TagsToBlock` 含 B = 「A 在跑期间 B 不能开始」
 * - `B.ForbiddenTags` 含 A = 「A 在时 B 不可施加」
 *
 * **⚠ 迁移陷阱（MUST 读，防把改名做成语义反转）**：旧的 `Blocks` 字段语义是
 * 「B 在时**我**不可施加」⇒ 它映射到 **`ForbiddenTags`**（受动），
 * **MUST NOT 映射到 `TagsToBlock`**（施动）。两格词形极近（`Blocks` 与 `TagsToBlock`）
 * 而**方向相反**，且该字段族当时**零消费者** ⇒ 错映射**编译器不报错**，是静默的语义反转。
 *
 * **本类型住 `TcsState`（共享层）**：两域（`TcsBuff` / `TcsSkill`）共用它，
 * 而 `TcsSkill` MUST NOT 依赖 `TcsBuff` ⇒ 它 MUST 保持**纯值**，
 * MUST NOT 引入任一域的 Def 类型或运行态记录类型。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsRelationRow
{
	GENERATED_BODY()

// 主体
#pragma region Subject
public:
	/**
	 * **主体词**：本行约束谁。
	 *
	 * 它 MUST 同时承载「具体定义」与「类别」两层含义，靠**层级匹配**统一
	 * （`FGameplayTag::MatchesTag`：父词命中子词、反向不成立）——
	 * 填**具体词**只作用于它自己，填**父词**覆盖一族。**MUST NOT** 为此发明通配语法。
	 *
	 * **★ 匹配面 = `CategoryTags` ∪ `{DefTag}`**（2026-10-24 用户裁定，**推翻**原「不做并集」口径）：
	 * 判据 = 「查询的核心不是查一类还是查一个，而是要**查询有明确结果**」——
	 * 达成「明确结果」的手段**不是**把 `DefTag` 排除出去，而是**一次 `HasTag` 层级匹配
	 * 作用于并集**：`DefTag` 命中它自己（精确到一个资产）、`CategoryTags` 的父词覆盖一族，
	 * 两者**在同一路径上各得其果**。
	 *
	 * **原文的误推 MUST 记录以免重犯**：本条曾写「`DefTag` 表达不了『一类』，并进来会把
	 * 『一族』退化成『一个资产』」——**这是错的**：并集**不会**退化，因为父词仍在
	 * `CategoryTags` 里照常覆盖一族，`DefTag` 只是**额外**多给了「精确到这一个资产」的能力。
	 * **真正的风险是「两条寻址路径」**（先按 `DefTag` 查一遍、再按 `CategoryTags` 查一遍），
	 * 那才产生不明确——**MUST NOT 实现成两条路径**。
	 *
	 * **客体面同口径**：四个容器**比对用的客体词面**（在跑实例贡献的词集）同样是
	 * `CategoryTags` ∪ `{DefTag}`——本裁定不分主体面/客体面，因为判据是「该资产**拥有的
	 * 全部词**」（与 GAS 的 `AssetTags` 集合同构），与该词用在匹配的哪一侧无关。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relation Row|Subject")
	FGameplayTag AppliesToTag;
#pragma endregion


// 受动：约束我自己
#pragma region Passive
public:
	/**
	 * **前置·必须存在**：这些词必须已存在，本定义才可施加。
	 *
	 * 与 `ForbiddenTags` 构成**前置条件的正反两面**（必须存在 / 必须缺席）——
	 * 它们才是真正的一对（**不是** `TagsToBlock`，后者是施动侧、方向相反）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relation Row|Passive")
	FGameplayTagContainer RequiredTags;

	/**
	 * **前置·必须缺席**：这些词存在时，本定义不可施加。
	 *
	 * **原 `Blocks` 字段的语义映射目标**（见类型注释的迁移陷阱）：
	 * 「B 在时我不可施加」= 约束**我自己** ⇒ 住受动侧。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relation Row|Passive")
	FGameplayTagContainer ForbiddenTags;
#pragma endregion


// 施动：我对外的影响
#pragma region Active
public:
	/**
	 * **施动·预防**：我在跑期间，命中的目标**无法开始且保持不能开始**。
	 *
	 * **与 `TagsToCancel` 的分工**：本字段是**事前**（让它不能开始），
	 * `TagsToCancel` 是**事后**（把已在跑的终止掉）。
	 *
	 * **两处检查时机 MUST 都覆盖**（只做前者会留下「我在跑、目标也已在跑」的窗口）：
	 * ① 目标尝试开始时（apply 门禁）；
	 * ② **我开始时就已存在的目标**须被即时阻断。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relation Row|Active")
	FGameplayTagContainer TagsToBlock;

	/**
	 * **施动·终止**：我在跑时，把它们**已有的**实例终止掉。
	 *
	 * 移除原因传 `EBuffRemoveCause::EBRC_Cancelled`——**该枚举首次有内建产生者**
	 * （此前只有注释自述「由调用方显式传入」，框架从不产生它）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relation Row|Active")
	FGameplayTagContainer TagsToCancel;
#pragma endregion
};



/**
 * **关系检查器**（2026-10-16 `R5.5-e1` 立）：关系表族字段的**唯一判定入口**。
 *
 * **为什么是静态函数族而不是一个类实例**：判定是**无状态纯函数**——输入全是值、
 * 输出全是值，没有需要跨调用保持的东西（同 `FTcsStateOps` / `FTcsCastOps` 的既有形态）。
 *
 * **形参 MUST 全是值**：主体词集 + 客体词集 + 行。本类型住 `TcsState`（共享层），
 * 而共享层 MUST NOT 认识任一域的 Def / 运行态类型（那会把共享层绑到具体域，
 * 且制造包含环）⇒ 域侧负责把「我这条实例」翻译成**共享层认得的最小公共字段集**。
 * 同款先例 = `FTcsEntityAdmissionExisting`（该类型的注释把这条判据写得很清楚）。
 *
 * **判定机制 MUST 只用层级匹配**（`FGameplayTag::MatchesTag` +
 * `FGameplayTagContainer::HasAny`），**MUST NOT 引入 `FGameplayTagQuery`**。
 */
struct TCSSTATE_API FTcsStateRelation
{
public:
	/**
	 * 该行是否作用于给定的主体词集（`HasTag` 层级匹配）。
	 *
	 * `param SubjectTags 候选实例所属定义的类别词集。
	 * `param Row 关系行。
	 * `return 主体命中则返回 true（该行的四个容器参与判定）。
	 */
	static bool AppliesTo(const FGameplayTagContainer& SubjectTags, const FTcsRelationRow& Row);

	/**
	 * **受动·前置（正）**：`RequiredTags` 是否全部已满足。
	 *
	 * 语义 = 「必须存在」：客体词集 MUST **覆盖** `RequiredTags` 的每一个词
	 * （逐词层级匹配：客体词集的任一词命中该要求词即可）。
	 *
	 * `param TargetTags 客体（当前存在的状态 / 运行态）的词集。
	 * `param Row 关系行。
	 * `return 全部满足返回 true；`RequiredTags` 为空时返回 true（无前置 = 恒满足）。
	 */
	static bool CheckRequired(const FGameplayTagContainer& TargetTags, const FTcsRelationRow& Row);

	/**
	 * **受动·前置（反）**：`ForbiddenTags` 是否命中。
	 *
	 * 语义 = 「必须缺席」：`TargetTags` 与 `ForbiddenTags` 有任一交集 ⇒ 命中（拒绝施加）。
	 *
	 * `param TargetTags 客体词集。
	 * `param Row 关系行。
	 * `return 命中返回 true（应拒绝）；`ForbiddenTags` 为空时返回 false。
	 */
	static bool CheckForbidden(const FGameplayTagContainer& TargetTags, const FTcsRelationRow& Row);

	/**
	 * **施动·预防**：`TagsToBlock` 是否覆盖给定词集。
	 *
	 * `param CandidateTags 候选目标的词集。
	 * `param Row 关系行（通常来自**已在跑**的实例）。
	 * `return 命中返回 true（该候选不可开始）。
	 */
	static bool CollectBlocks(const FGameplayTagContainer& CandidateTags, const FTcsRelationRow& Row);

	/**
	 * **施动·终止**：`TagsToCancel` 是否覆盖给定词集。
	 *
	 * `param CandidateTags 候选目标的词集。
	 * `param Row 关系行。
	 * `return 命中返回 true（该候选应被终止）。
	 */
	static bool CollectCancels(const FGameplayTagContainer& CandidateTags, const FTcsRelationRow& Row);
};
