// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Attribute/TcsAttrModDef.h"
#include "Trigger/TcsEffectTrigger.h"

#include "Def/TcsBoolSwitchRow.h"
#include "Def/TcsCastAttrCapture.h"
#include "Def/TcsPhaseSpan.h"
#include "Def/TcsSkillEnums.h"
#include "Def/TcsStateDefBase.h"

#include "TcsSkillDefData.generated.h"



/**
 * 技能定义数据：继承状态族的身份词、等级、数值参数、描述与修正器引用行，只补施法语义。
 * 关系字段只落形状，检查器归 R5.5-e；内联触发行与查询片段的运行机制在后续任务接入。
 * 参数链行依赖 Task 4 的完整元素类型，故本批不声明；冷却与 Cost 配置归 R6.5。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillDefData : public FTcsStateDefBase
{
	GENERATED_BODY()

// 布尔参数表
#pragma region BoolSwitches

public:
	// 与继承的 Params 构成参数双表；本批只提供配置与作者期校验。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Bool Switches")
	TArray<FTcsBoolSwitchRow> BoolSwitches;

#pragma endregion


// 施法时段
#pragma region Phases

public:
	// 任意段数，瞬发必须使用空表；时段时长在激活时快照化。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Phases")
	TArray<FTcsPhaseSpan> Phases;

#pragma endregion


// 施法状态查询
#pragma region CastQuery

public:
	// 查询选择：定义开关、当前时段或宿主片段。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Cast Query")
	ECastQueryMode CastQueryMode = ECastQueryMode::CQM_DefSwitches;

	// 定义开关档的可打断值（宿主可覆盖）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Cast Query")
	bool bInterruptibleDefault = false;

	// 定义开关档的可移动值（宿主可覆盖）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Cast Query")
	bool bCanMoveDefault = false;

	// 自定义查询载荷：契约实现归后续任务，本批不预建策略基类。
	UPROPERTY(EditAnywhere, Category = "Skill Def|Cast Query")
	FInstancedStruct CastQueryFragment;

#pragma endregion


// 属性捕获
#pragma region AttrCapture

public:
	// 技能侧激活捕获声明，与伤害流程侧捕获独立。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Attr Capture")
	TArray<FTcsCastAttrCapture> AttrCaptureList;

#pragma endregion


// 主效果链
#pragma region MainChain

public:
	// 主效果链身份，由效果定义库解析。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Main Chain")
	FGameplayTag CastChainId;

	// 主效果链启动时点（默认在施法开始）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Main Chain")
	EMainChainStart MainChainStart = EMainChainStart::MCS_OnCastStarted;

	// 进入指定时段档所引用的时段标识。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Main Chain")
	FGameplayTag MainChainStartPhaseTag;

#pragma endregion


// 实例化
#pragma region Instancing

public:
	// 施法运行态的实例化方式。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Instancing")
	ECastInstancing Instancing = ECastInstancing::CI_InstancePerExecution;

#pragma endregion


// 关系
#pragma region Relations

public:
	// 互斥状态词列表（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	TArray<FGameplayTag> Blocks;

	// 必须存在的状态词列表（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	TArray<FGameplayTag> Requires;

	// 顶替优先级，与状态族同形（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	int32 Priority = 0;

	// 激活时取消的状态词列表（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	TArray<FGameplayTag> Cancels;

#pragma endregion


// 内联触发行
#pragma region Triggers

public:
	// 本技能的内联事件映射行；本批零内置运行期消费者。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Triggers")
	TArray<FTcsEffectTriggerDef> Triggers;

#pragma endregion
};
