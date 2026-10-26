// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "State/TcsStateRelation.h"

#include "TcsStateLiveView.generated.h"



/**
 * **在跑实例的完整视图**（2026-10-16 `R5.5-e1` 立）：**「我是什么」+「我约束谁」**两半。
 *
 * **为什么必须是一对、而不是只用 `FTcsRelationRow`**（★ 这是本类型存在的唯一理由，
 * 也是本轮契约形状的第三次修正——前两版都少了其中一半）：
 *
 * | 半 | 字段 | 语义 | 缺了它会怎样 |
 * |---|---|---|---|
 * | **事实** | `CategoryTags` | 这个在跑实例**是什么**（= `CategoryTags` ∪ `{DefTag}`） | 判不了「在跑状态是否落进我的 `ForbiddenTags` /
 * |   |   |   | 是否满足我的 `RequiredTags`」——只能落成"未落面" |
 * | **声明** | `Row` | 这个在跑实例**约束谁** | 判不了「在跑状态是否用 `TagsToBlock` 覆盖我」 |
 *
 * **行与词面是两种语义，MUST NOT 混装进一个类型**：行答「我约束谁」（声明），
 * 词面答「我是什么」（事实）。把 `CategoryTags` 塞进 `FTcsRelationRow` 会让同一格
 * 同时承载两种语义——那是本仓反复拦的混装形态。故此处**并列**，不合并。
 *
 * **两个半都取自同一个定义**（`CategoryTags` 与四数组同住 Def），故一次遍历即可给出。
 *
 * **★ 事实半的词集是并集（2026-10-24 裁定）**：`CategoryTags` 装的是 **`Def` 的 `CategoryTags` ∪ `{DefTag}`**，
 * 判据原话 = 「这两个合起来，相当于 GAS 里 GA 和 GE 都有的 `AssetTags` 集合」——`AssetTags` 是该资产
 * **拥有的全部词**，不分身份词与类别词。⇒ **凡"拿一个定义的词去匹配"的地方，词集 MUST 是并集**。
 * 语义上：`DefTag` 命中它自己（精确到一个资产）、`CategoryTags` 的父词覆盖一族，
 * 两者在**同一次层级匹配**里各得其果，**MUST NOT 拆成两条寻址路径**。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsLiveStateView
{
	GENERATED_BODY()

// 事实：我是什么
#pragma region Fact

public:
	// 该在跑实例的**可寻址词集** = `CategoryTags` ∪ `{DefTag}`（2026-10-24 裁定）。
	// 消费者用层级匹配问「属不属于某类」或「是不是这个资产」——两者都在同一次 `HasTag` 里回答。
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Live View")
	FGameplayTagContainer CategoryTags;

#pragma endregion


// 声明：我约束谁
#pragma region Declaration

public:
	// 该在跑实例所属定义合成的**关系行**（主体词 = 它的 `DefTag`）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Live View")
	FTcsRelationRow Row;

#pragma endregion
};
