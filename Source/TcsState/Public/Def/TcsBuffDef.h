// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"
#include "State/TcsStateEnums.h"
#include "State/TcsStateStackPolicy.h"
#include "Trigger/TcsEffectTrigger.h"

#include "Def/TcsStateDefBase.h"

#include "TcsBuffDef.generated.h"



/**
 * Buff 定义（施加态语义，D3-10 终定）：在 `FTcsStateDefBase`（词表身份 / 等级 / 参数行 / 描述 / 修正器行）
 * 之上补四组**施加态专属**字段——时值、堆叠、行为、关系。
 *
 * **为什么这些字段住派生而不进基类**：R6 的 `FSkillDef` 继承同一基类却无时值、无堆叠
 * （施法时段是另一套语义），字段划分判据见 `FTcsStateDefBase` 的说明。
 *
 * **行为面（内联触发行位）**：`Triggers` 是 `TRIG-4` 的落点——buff 自己的生命周期事件触发行**内联进 Def**
 * （区别于 `UTcsEffectTriggerDefAsset` 那条"独立资产"载体）；施加时登记、`Source` = 状态实例来源句柄、
 * 移除/到期按同一句柄级联退订，登记与退订时机归 `effect-trigger` 能力（R5 Task 6 接线）。
 *
 * **关系字段本轮只落形状**：`Blocks` / `Requires` / `Priority` / `Cancels` 的**检查器**归 R5.5-e
 * （字段语义未定稿）——字段可先就位，但 MUST NOT 为它们造机制，零消费者如实登记。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsBuffDef : public FTcsStateDefBase
{
	GENERATED_BODY()

// 时值
#pragma region Duration

public:
	// 时值策略（Finite 走 DurationTime 到期；Infinite 永不过期）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Duration")
	EDurationPolicy DurationPolicy = EDurationPolicy::EDP_Finite;

	// 持续时间（Finite 时有效；可配等级表等参数源，apply/refresh 时快照化）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Duration")
	FTcsParamValue DurationTime;

#pragma endregion


// 周期
#pragma region Period

public:
	// 周期时长（秒；0 = 无周期。四组合全合法：Finite+Period = 标准 DoT、Infinite+Period = 光环）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Period")
	double Period = 0.0;

	// 刷新对周期计时的作用（仅在有周期时有意义）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Period")
	ETcsPeriodRefresh PeriodRefresh = ETcsPeriodRefresh::EPR_Keep;

#pragma endregion


// 堆叠与刷新
#pragma region Stacking

public:
	// 五轴堆叠与刷新策略（本轮只落形状，行为归 R5 Task 5）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Stacking")
	FStateStackPolicy StackPolicy;

#pragma endregion


// 行为（内联触发行）
#pragma region Behavior

public:
	// 内联触发行（`TRIG-4`）：施加时登记为该状态实例的触发行，`Source` = 实例来源句柄
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Behavior")
	TArray<FTcsEffectTriggerDef> Triggers;

#pragma endregion


// 关系
#pragma region Relations

public:
	// 互斥：这些状态存在时本状态不可施加（检查器归 R5.5-e）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Relations")
	TArray<FGameplayTag> Blocks;

	// 前提：这些状态必须已存在（检查器归 R5.5-e）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Relations")
	TArray<FGameplayTag> Requires;

	// 顶替优先级（大者胜；检查器与槽位竞争归 R5.5-e）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Relations")
	int32 Priority = 0;

	// 施加本状态时取消这些状态（检查器归 R5.5-e）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Relations")
	TArray<FGameplayTag> Cancels;

#pragma endregion
};
