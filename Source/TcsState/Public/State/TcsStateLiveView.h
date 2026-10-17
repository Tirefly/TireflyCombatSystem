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
 * | **事实** | `CategoryTags` | 这个在跑实例**是什么** | 判不了「在跑状态是否落进我的 `ForbiddenTags` /
 * |   |   |   | 是否满足我的 `RequiredTags`」——只能落成"未落面" |
 * | **声明** | `Row` | 这个在跑实例**约束谁** | 判不了「在跑状态是否用 `TagsToBlock` 覆盖我」 |
 *
 * **行与词面是两种语义，MUST NOT 混装进一个类型**：行答「我约束谁」（声明），
 * 词面答「我是什么」（事实）。把 `CategoryTags` 塞进 `FTcsRelationRow` 会让同一格
 * 同时承载两种语义——那是本仓反复拦的混装形态。故此处**并列**，不合并。
 *
 * **两个半都取自同一个定义**（`CategoryTags` 与四数组同住 Def），故一次遍历即可给出。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsLiveStateView
{
	GENERATED_BODY()

// 事实：我是什么
#pragma region Fact

public:
	// 该在跑实例所属定义的**类别词集**（声明面对手：消费者用层级匹配问"属不属于某类"）
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
