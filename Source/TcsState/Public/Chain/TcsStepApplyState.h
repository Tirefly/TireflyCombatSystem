// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "TcsStepApplyState.generated.h"



/**
 * 状态施加步骤（D4-16 十五原语之一，**属 TcsState**；04 §2.1"ApplyState 住 TcsState"）。
 *
 * **做什么**：在链里施加一个状态——经黑板 `Owner` 取世界 → 状态门面 → `ApplyState(...)`：
 *
 * | 形参 | 取值 |
 * |---|---|
 * | `Source` | 黑板 `RunSource`（**链运行态来源锚点**：同一运行内同源 ⇒ 续杯；两次运行异源 ⇒ 可叠层） |
 * | `Instigator` | 黑板 `Instigator`（无效时由门面退化为目标） |
 * | `ParamTable` | **空**（链侧参数表载体不存在 ⇒ 引用类参数源落兜底——如实边界） |
 * | `Overrides` | 本步骤字段（施加方参数覆盖，参与快照构建） |
 *
 * **语义**：**即时步骤**（恒 `TSR_Completed`，不挂起）。**四档回执**（`Applied` / `Refreshed` /
 * `Stacked` / `Rejected`）都是业务结果 ⇒ 一律 `Log` 级记录，**MUST NOT** 因"被拒绝"产生红字。
 *
 * **失败面（都不 ensure、都不断链）**：步骤载荷类型不符 / 目标无效且黑板目标集为空 / 状态门面不可得
 * ⇒ `Warning` + 按完成处理；`DefTag` 无效（未配置）⇒ `Error` + 按完成处理；定义未在本世界登记
 * ⇒ 由门面拒绝（`Rejected`，门面已留 Warning，本步不重复报错）。
 * **`ETcsStepResult` 只有 `Completed` / `Running` ⇒ 步骤无法中断链**；软失败接管归 `OnError`（R5.5-a）。
 *
 * 执行器住 `Private/Chain/TcsStepApplyState.cpp`（+ 一行宏自注册）。
 */
USTRUCT()
struct TCSSTATE_API FTcsStepApplyState
{
	GENERATED_BODY()

// 施加
#pragma region Apply

public:
	// 目标实体（无效则取黑板 `Targets[0]`，仍无效 ⇒ 软失败）
	UPROPERTY(EditAnywhere, Category = "Tcs|State|ApplyState")
	FTcsCombatEntityHandle Target;

	// 施加的定义身份（须已在本世界登记；无效 ⇒ Error + 按完成处理）
	UPROPERTY(EditAnywhere, Category = "Tcs|State|ApplyState")
	FGameplayTag DefTag;

	// 施加方参数覆盖（参与快照构建；键落宿主声明的状态参数词表根）
	UPROPERTY(EditAnywhere, Category = "Tcs|State|ApplyState")
	TMap<FGameplayTag, double> Overrides;

#pragma endregion
};
