// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTargetFilterHost.generated.h"



// 宿主过滤器插槽（TcsTargeting 反射面；宿主用任意 UE 脚本语言实现——台账 SCRIPT-8）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsTargetFilterHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * 宿主目标过滤器插槽（2026-09-24，台账 SCRIPT-8）：**怎么算存活、怎么算敌人、阵营如何判定全是宿主本体论**
 * （10 §2.2）——本接口让宿主用任意 UE 脚本语言表达它，零 C++ 改动。
 *
 * **为什么必须经本接口转发**：同 `ITcsTargetSelectorHost`——`FTcsTargetFilterStrategy` 走 C++ 虚分派，
 * 脚本定义的 struct 没有 C++ 类型（vtable 位为 0，调用即野函数指针），引擎层面无解。
 *
 * **组合语义不变**：AND 全过 + 短路（由 `SelectTargets` 执行器保证，本插槽不改变它）——
 * 本接口只回答"单个候选是否通过"。
 */
class ITcsTargetFilterHost
{
	GENERATED_BODY()

// 判定契约
#pragma region Pass

public:
	/**
	 * 候选是否通过过滤（**纯判定**——MUST NOT 改写候选或上下文）。
	 *
	 * @param Candidate 候选目标实体句柄。
	 * @param Caster 施法者实体句柄。
	 * @param Instigator 发起者实体句柄。
	 * @return 返回是否通过（false = 淘汰该候选，后续 Filter 不再询问——短路）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Targeting")
	bool PassTarget(FTcsCombatEntityHandle Candidate, FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator);

#pragma endregion
};
