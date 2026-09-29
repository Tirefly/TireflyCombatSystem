// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Parameter/TcsParamValue.h"

#include "TcsDamageFlowContextView.generated.h"



/**
 * 流程上下文**反射视图**（2026-09-24，台账 SCRIPT-8 的"宿主脚本插槽"）：
 * `FTcsDamageFlowContext` 的**可反射数据面投影**——宿主脚本层（C# / AS / Luau / TS / 蓝图）
 * 在 `ITcsDamageFlowDelegate` 的实现里读流程状态用。
 *
 * **为什么需要视图，而不是把 `FTcsDamageFlowContext` 整体反射化**：
 * 后者的黑板 `FTcsFlowAttributes` → `FTcsFlowAttributeSubmit` → `FTcsConsumePolicy::OnConsumed`
 * 是 `TFunction<void()>` 的**递归闭包**（见 `TcsFlowAttributes.h` 的消耗策略注释），
 * **物理不可反射**（UHT 实证）。真前置是"上下文/黑板分层"（台账 SCRIPT-3），成本高一个数量级；
 * 而插槽只需要读参与者与参数，视图足够。
 *
 * **不含的字段与理由**（MUST NOT 加，加则本类型失去意义）：
 * - `Blackboard`——见上，含 `TFunction` 物理不可反射；
 * - `Owner`（`TWeakObjectPtr<UTcsDamageSubsystem>`）——脚本实现本身是 UObject，可经自身
 *   `GetWorld()` 取门面；且弱引用非脚本友好；
 * - `FlowSource` / `CapturedAttrs`——框架簿记面（级联摘除锚点 / 捕获快照），宿主脚本无消费场景。
 *
 * **单向投影**：由 `MakeView` 从原上下文构造，**无反向写回**——视图是只读投影，
 * 不是"第二个可写上下文"（避免双真相）。宿主若要修正流程值，走收集事件协议提交黑板修正
 * （`PublishCollectEvent` 的既有通路），不是改视图。
 */
USTRUCT(BlueprintType)
struct TCSDAMAGE_API FTcsDamageFlowContextView
{
	GENERATED_BODY()

// 参与者
#pragma region Subjects

public:
	// 攻击者
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	FTcsCombatEntityHandle Attacker;

	// 发起者（表"谁发起的"，可与 Attacker 不同）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	FTcsCombatEntityHandle Instigator;

	// 目标集
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	TArray<FTcsCombatEntityHandle> Targets;

#pragma endregion


// 数据面
#pragma region Data

public:
	// 公式参数初值（只读原料；键 = 项目词表）——宿主公式按名取用
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	TMap<FGameplayTag, FTcsParamValue> FormulaParams;

	// 分类 Tag 集（来源标签启动写入 / 元素标签由 Element 步骤写入——词表归项目）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	TArray<FGameplayTag> ClassificationTags;

#pragma endregion


// 请求
#pragma region Request

public:
	// 基础伤害输入（链步骤的参数账本解算结果——流程零计算，本字段只承接不推导）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	double BaseDamageInput = 0.0;

	// 本次流程要结算的属性键（调用方指定，如 `Health`；空 = 未指定）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Flow")
	FGameplayTag TargetAttrKey;

#pragma endregion
};
