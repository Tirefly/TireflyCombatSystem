// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Def/TcsDescriptionEntry.h"
#include "Def/TcsParamRow.h"

#include "TcsStateDefBase.generated.h"



class UTcsAttrModDef;



/**
 * 状态定义基类（`FStateDefBase` 家族的数据形状）：**抽象 + 编辑器隐藏**——`FTcsBuffDef`（本模块）
 * 与 `FSkillDef`（TcsSkill，R6）各自继承它。
 *
 * **字段划分纪律（R6 继承契约，MUST 守死）**：基类 MUST NOT 出现任何**时值 / 堆叠**字段——
 * R6 的 `FSkillDef` 要按继承白拿"参数行 + 描述 + 修正器行"，而它无时值、无堆叠（施法时段另有一套，
 * 见 `SPEC-05-skill`）。判据：**无时值 / 无堆叠 = 基类；有时值 / 有堆叠 = 派生**。
 *
 * **抽象手法**：`meta = (Hidden)` 把基类从结构体选择器里摘出去（编辑器只该暴露 `FTcsBuffDef` / `FSkillDef`）；
 * 数据结构无策略，故 MUST NOT 用 `=0` 或 `PURE_VIRTUAL` 制造"抽象"。
 *
 * **本轮 MUST NOT 预建的字段**：设计 §2 提到的"生命周期事件词汇"字段**零消费者**
 * （行为 Fragment 的兴趣 Tag 已是声明面）⇒ 等真实消费者（`PLN-R5` Task 1 非目标）。
 *
 * **字段沿革（2026-10-15，MUST 读，防按旧名找）**：本类原持 `FGameplayTag StatusTag`
 * （单 tag），已**删除**并代之以 `FGameplayTagContainer CategoryTags`。
 * 两者是**同一件事的单值形态与容器形态**——容器是超集，故合并不是"新增一个字段"，
 * 而是"删掉冗余的那个"。改动理由与资产迁移见变更 `merge-status-tag-into-category-tags`。
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct TCSSTATE_API FTcsStateDefBase
{
	GENERATED_BODY()

// 类别标识
#pragma region Identity

public:
	/**
	 * **定义级类别容器**：声明"**我是什么**"（语义分类），**不是**"我要匹配谁"
	 * （后者是消费者侧的查询——如条目选择器的 `ByCategoryTags`）。
	 *
	 * **为什么是容器而不是单个 tag**（2026-10-15 由 `StatusTag` 合并而来）：
	 * ① 容器 ⊇ 单值——单个词能表达的一切用单元素容器表达，反向不成立；
	 * ② 外部先例同向——GAS 的 `UGameplayEffect::InheritableGameplayEffectTags` 就是容器
	 *    （`GameplayEffect.h`，类型 `FInheritedTagContainer`）；
	 * ③ 设计要的是"**行 = 单个或一类** StateDef"（`SPEC-02-states` §3.4），
	 *    而单值表达不了"一类"。
	 *
	 * **非必填**：空容器是合法配置——它是**筛选依据**而非身份，有消费者筛选时
	 * 自然不命中任何查询（明确结果，不是缺省故障）。**MUST NOT** 在发现期或
	 * 作者期拦空值（那属误报）。原 `StatusTag` 的"非空"硬约束随本合并取消。
	 *
	 * **每词一义**：具体词 = 只作用于它自己；填**父词**则经层级匹配覆盖一族
	 * （`FGameplayTag::MatchesTag`，父命中子；先例 `TcsParamChain.cpp`）。
	 *
	 * **与 `DefTag` 的分工（两个独立查询通道，MUST NOT 混为并集）**：
	 * `DefTag` = **唯一键**（定义库的解析键与去重键，重复即报错），答"这是哪份资产"；
	 * 本字段 = **语义词**（不参与去重，多个资产可共享同值），答"这在语义上属于什么"。
	 * `DefTag` 表达不了"一类"，把它并进类别匹配会把"一族"退化成"一个资产"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	FGameplayTagContainer CategoryTags;

#pragma endregion


// 等级
#pragma region Level

public:
	// 等级下界（自 1 起；等级源按它推算档位下标——索引口径的唯一真相在源，PV-10）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	int32 LevelBase = 1;

	// 等级上界（0 = 无上界；等级增长策略归项目业务侧，引擎零内置）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	int32 MaxLevel = 0;

#pragma endregion


// 参数行
#pragma region Params

public:
	// 通用默认参数行（快照构建时逐行求值并冻结；施加上下文覆盖值优先，Def 默认兜底）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	TArray<FTcsNumericParamRow> Params;

#pragma endregion


// 描述
#pragma region Descriptions

public:
	// 描述配置组（D5-17 v3；本轮只落字段与作者期校验，渲染归 R8）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	TArray<FTcsDescriptionEntry> Descriptions;

#pragma endregion


// 修正器引用行
#pragma region ModifierRows

public:
	// 修正器模板引用行（D3-19：apply 时按模板物化进属性账本；**R5 只用属性行**——技能行归 R6）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows;

#pragma endregion
};
