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
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct TCSSTATE_API FTcsStateDefBase
{
	GENERATED_BODY()

// 词表身份
#pragma region Identity

public:
	// 状态词表条目（区分"同名不同态"的实例；也是发现期的身份校验项之一，见 state-def-asset 能力）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Def")
	FGameplayTag StatusTag;

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
