// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Parameter/TcsParamValue.h"
#include "State/TcsBuffEnums.h"
#include "State/TcsBuffStackPolicy.h"
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
 * （区别于 `UTcsEffectTriggerDefAsset` 那条"独立资产"载体）；施加时登记、`Source` = 实例级联锚点，
 * 移除/到期按同一锚点级联退订。行为 Fragment 则由 `Fragments` 持有（精确兴趣 Tag + const 回调），
 * 订阅句柄归旁表，不进状态实例；两类行为都在施加广播前接线、移除广播前退订。
 *
 * **关系字段本轮只落形状**：`Blocks` / `Requires` / `Priority` / `Cancels` 的**检查器**归 R5.5-e
 * （字段语义未定稿）——字段可先就位，但 MUST NOT 为它们造机制，零消费者如实登记。
 */
USTRUCT(BlueprintType)
struct TCSBUFF_API FTcsBuffDef : public FTcsStateDefBase
{
	GENERATED_BODY()

// 时值
#pragma region Duration

public:
	// 时值策略（Finite 走 DurationTime 到期；Infinite 永不过期）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Duration")
	EBuffDurationPolicy DurationPolicy = EBuffDurationPolicy::EBDP_Finite;

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
	ETcsBuffPeriodRefresh PeriodRefresh = ETcsBuffPeriodRefresh::EBPR_Keep;

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
	// 内联触发行（TRIG-4）：Source = 实例 CascadeAnchor；Subject = 实例 Handle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Behavior")
	TArray<FTcsEffectTriggerDef> Triggers;

	// 行为片段由 Def 持有，实例不持策略；载具沿用 2026-09-24 裸 FInstancedStruct 换型口径。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Behavior",
		meta = (BaseStruct = "/Script/TcsBuff.TcsBuffBehaviorFragment"))
	TArray<FInstancedStruct> Fragments;

#pragma endregion


// 关系
#pragma region Relations

public:
	// **四个关系字段住在基类**（2026-10-17 上移，理由见 `FTcsStateDefBase` 的 `Relations` 区注释）：
	// 本状态的「前摇/参与规则」与状态侧**逐字同义**，两侧共用同一检查器 `FTcsStateRelation`。
	// 本类 MUST NOT 重复声明 `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel`。

    // 顶替优先级（大者胜；**不属关系字段族**——归槽位竞争表 `R5.5-e4`）
	// `R5.5-e1` MUST NOT 触碰本字段：它今天仍零消费者，检查器不为它做任何判定。
	//
	// **★ 布局（是否与关系字段同上移基类）同批归 `R5.5-e4`**（2026-10-17 用户裁定）：
	// 它同样「无时值 / 无堆叠」⇒ 按基类自己的判据**本也该在基类**；但 `e4` 要裁的正是
	// 「移除 / 改述」，**目的地未定** ⇒ 现在上移可能白做（搬进去又删掉）⇒ 留到 `e4` 一次动。
	// ⇒ **`e4` MUST 把「物理去留」与「住哪一层」两件事一并裁**（两者同源，分开裁会漏）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def|Relations")
	int32 Priority = 0;

#pragma endregion
};
