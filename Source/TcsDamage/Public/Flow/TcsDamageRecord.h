// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsDamageRecord.generated.h"



/**
 * 伤害记录**已产出**事件 Tag（09 §2.4；按 `Tcs.Event.<域>.<事件名>` 公约原生声明，
 * 不进项目 Tag 表）——载荷 = `FTcsDamageRecord`，经总线**立即通道**同步派发（回放/统计/UI 数据源）。
 * 带模块导出宏（宿主订阅必需——`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 是裸 `extern`，见 `TcsDamageFlowCollectEvent.h` 说明）。
 */
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Event_Damage_Recorded;



/**
 * 伤害记录（09 §2.4）：**结果快照**，不是"每键修改器贡献清单"（09 §5 非目标——千人战场每笔伤害
 * 带贡献清单是量灾难；"这个数怎么算出来的"走 M8 Explain 开发期回放）。
 *
 * 结构纪律（对齐 `openspec/project.md`"过网结构 MUST 纯反射数据"）：
 * - **全体字段扁平**——MUST NOT 出现 `TMap`/`TSet`（UHT 层面不可复制）与 `TFunction`（UHT 报错）；
 * - 参与者为**实体身份句柄**（MUST NOT 用 `AActor*`——承接 2026-09-20 句柄化）；
 * - 因此将来可直接上 `FFastArraySerializer`（追加型结果流的推荐通道）。
 */
USTRUCT(BlueprintType)
struct TCSDAMAGE_API FTcsDamageRecord
{
	GENERATED_BODY()

// 身份与参与者
#pragma region Identity

public:
	// 流程唯一号（同一流程的所有记录共享；R3 用流程来源句柄的 Id 作号）
	// 类型 = `int64`（UHT 不支持 `uint64` 作属性类型——句柄 Id 恒为正，无符号性差异无实际影响）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	int64 FlowId = 0;

	// 攻击者实体句柄
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	FTcsCombatEntityHandle Source;

	// 目标实体句柄
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	FTcsCombatEntityHandle Target;

	// 元素（delegate 解析结果，可能为空）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	FGameplayTag Element;

#pragma endregion


// 判定与数值
#pragma region Outcome

public:
	// 是否命中
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	bool bHit = false;

	// 是否暴击
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	bool bCrit = false;

	// 本次基础值（= 链步骤解算结果；流程零计算，PV-7）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	double Base = 0.0;

	// 修正后的最终值（收集的修改器全部生效后）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	double Final = 0.0;

	// 实际执行量（扣除护盾吸收后落进 M2 事务的量）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	double Executed = 0.0;

	// 被护盾吸收量
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	double Absorbed = 0.0;

	// **记账而非裁定**：本次事务提交后目标生命 ≤ 0（死亡规则仍归宿主——09 §3）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	bool bKill = false;

#pragma endregion


// 序与时刻
#pragma region Ordering

public:
	// 记录序号（门面单调递增发号——回放依赖序；`int64` 同 FlowId 的 UHT 约束）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	int64 Sequence = 0;

	// 产出时刻（战斗时钟 Elapsed——确定性纪律 D0-1，非 wall-clock）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Damage|Record")
	double Timestamp = 0.0;

#pragma endregion
};
